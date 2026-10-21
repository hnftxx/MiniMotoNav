#include "platform/ble_server.h"

#if defined(ARDUINO)

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

namespace moto {
namespace platform {

namespace {

constexpr char kServiceUuid[] = "b5e10001-6d6e-4d4e-8000-6d696e696d6f";
constexpr char kTelemetryUuid[] = "b5e10002-6d6e-4d4e-8000-6d696e696d6f";
constexpr char kNavigationUuid[] = "b5e10003-6d6e-4d4e-8000-6d696e696d6f";
constexpr char kControlUuid[] = "b5e10004-6d6e-4d4e-8000-6d696e696d6f";
constexpr char kStatusUuid[] = "b5e10005-6d6e-4d4e-8000-6d696e696d6f";
constexpr char kCapabilityUuid[] = "b5e10006-6d6e-4d4e-8000-6d696e696d6f";

constexpr uint16_t kRequestedMtu = 247;
constexpr uint16_t kAttWriteOverhead = 3;
constexpr size_t kMaxReassembly = 512;
constexpr uint32_t kReassemblyTimeoutMs = 5000;
constexpr uint8_t kCapabilityAll = 0x0F;

const char* const kDeviceName = "GODSPEED";
const char* const kDeviceCompany = "GODSPEED";

uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void writeLe16(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)(value >> 8);
}

void writeLe32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
    p[2] = (uint8_t)((value >> 16) & 0xFF);
    p[3] = (uint8_t)((value >> 24) & 0xFF);
}

size_t buildCapability(uint16_t mtu, uint8_t* out, size_t capacity) {
    const size_t name_length = std::strlen(kDeviceName);
    const size_t payload_length = 8 + name_length;
    const size_t total = ble::kHeaderBytes + payload_length + 2;
    if (total > capacity) return 0;

    const uint16_t max_payload =
        mtu > (ble::kOverheadBytes + kAttWriteOverhead)
            ? (uint16_t)(mtu - ble::kOverheadBytes - kAttWriteOverhead)
            : 0;

    out[0] = ble::kVersion;
    out[1] = ble::kCapability;
    out[2] = 0;
    out[3] = 0;
    writeLe32(out + 4, (uint32_t)time(nullptr));
    writeLe16(out + 8, 0);
    writeLe16(out + 10, (uint16_t)payload_length);

    uint8_t* payload = out + ble::kHeaderBytes;
    payload[0] = ble::kVersion;
    payload[1] = (uint8_t)mtu;
    payload[2] = kCapabilityAll;
    payload[3] = 0;
    writeLe16(payload + 4, max_payload);
    writeLe16(payload + 6, (uint16_t)name_length);
    std::memcpy(payload + 8, kDeviceName, name_length);

    const uint16_t crc = ble::crc16(out, ble::kHeaderBytes + payload_length);
    writeLe16(out + ble::kHeaderBytes + payload_length, crc);
    return total;
}

} // namespace

struct BleServer::Impl {
    BleServer* owner = nullptr;

    NimBLEServer* server = nullptr;
    NimBLEService* service = nullptr;
    NimBLECharacteristic* telemetry_char = nullptr;
    NimBLECharacteristic* navigation_char = nullptr;
    NimBLECharacteristic* control_char = nullptr;
    NimBLECharacteristic* status_char = nullptr;
    NimBLECharacteristic* capability_char = nullptr;

    QueueHandle_t queue = nullptr;
    SemaphoreHandle_t reassembly_mutex = nullptr;

    std::atomic<bool> connected{false};
    std::atomic<bool> link_dirty{false};
    std::atomic<uint16_t> mtu{kRequestedMtu};

    struct Reassembly {
        uint8_t buffer[kMaxReassembly] = {};
        size_t length = 0;
        uint8_t sequence = 0;
        uint8_t type = 0;
        uint32_t started_ms = 0;
        bool active = false;
    } reassembly;

    void onConnect();
    void onDisconnect();
    void onMtuChange(uint16_t value);
    void handleWrite(const uint8_t* data, size_t length, uint8_t expected_type);
    void emitPacket(const ble::Packet& packet);
    void emitControl(const ble::Packet& packet);
    void updateCapability();
    void dropReassembly(uint8_t code, uint8_t sequence);
    void sendStatus(uint8_t code, uint8_t ack_sequence, const char* detail = nullptr);
    void checkReassemblyTimeout();
};

namespace {

class ServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit ServerCallbacks(BleServer::Impl* impl) : impl_(impl) {}

    void onConnect(NimBLEServer*, NimBLEConnInfo&) override { impl_->onConnect(); }

    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
        impl_->onDisconnect();
    }

    void onMTUChange(uint16_t mtu, NimBLEConnInfo&) override {
        impl_->onMtuChange(mtu);
    }

private:
    BleServer::Impl* impl_;
};

class WriteCallbacks : public NimBLECharacteristicCallbacks {
public:
    WriteCallbacks(BleServer::Impl* impl, uint8_t type) : impl_(impl), type_(type) {}

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        const NimBLEAttValue value = characteristic->getValue();
        impl_->handleWrite(value.data(), value.length(), type_);
    }

private:
    BleServer::Impl* impl_;
    uint8_t type_;
};

} // namespace

BleServer::BleServer() : impl_(new Impl()) { impl_->owner = this; }

BleServer::~BleServer() {
    delete impl_;
    impl_ = nullptr;
}

void BleServer::begin(const char* name) {
    if (impl_->server != nullptr) return;

    NimBLEDevice::init(name != nullptr ? name : kDeviceName);
    NimBLEDevice::setMTU(kRequestedMtu);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    impl_->queue = xQueueCreate(8, sizeof(BleIncoming));
    impl_->reassembly_mutex = xSemaphoreCreateMutex();
    impl_->server = NimBLEDevice::createServer();
    impl_->server->setCallbacks(new ServerCallbacks(impl_));
    impl_->server->advertiseOnDisconnect(true);

    impl_->service = impl_->server->createService(kServiceUuid);

    impl_->telemetry_char = impl_->service->createCharacteristic(
        kTelemetryUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, kMaxReassembly);
    impl_->navigation_char = impl_->service->createCharacteristic(
        kNavigationUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, kMaxReassembly);
    impl_->control_char = impl_->service->createCharacteristic(
        kControlUuid, NIMBLE_PROPERTY::WRITE, 32);
    impl_->status_char = impl_->service->createCharacteristic(
        kStatusUuid, NIMBLE_PROPERTY::NOTIFY, 128);
    impl_->capability_char = impl_->service->createCharacteristic(
        kCapabilityUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 64);

    impl_->telemetry_char->setCallbacks(new WriteCallbacks(impl_, ble::kTelemetry));
    impl_->navigation_char->setCallbacks(new WriteCallbacks(impl_, ble::kNavigation));
    impl_->control_char->setCallbacks(new WriteCallbacks(impl_, ble::kControl));

    impl_->updateCapability();
    impl_->server->start();

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->setName(kDeviceName);
    advertising->addServiceUUID(kServiceUuid);
    advertising->enableScanResponse(true);
    advertising->start();
}

bool BleServer::poll(BleIncoming& out) {
    if (impl_->queue == nullptr) return false;
    impl_->checkReassemblyTimeout();
    return xQueueReceive(impl_->queue, &out, 0) == pdTRUE;
}

bool BleServer::pollLinkChange(bool& linked) {
    if (!impl_->link_dirty.exchange(false)) return false;
    linked = impl_->connected.load();
    return true;
}

bool BleServer::connected() const { return impl_->connected.load(); }

uint16_t BleServer::mtu() const { return impl_->mtu.load(); }

void BleServer::Impl::onConnect() {
    connected.store(true);
    link_dirty.store(true);
    if (capability_char != nullptr) {
        capability_char->notify();
    }
}

void BleServer::Impl::onDisconnect() {
    connected.store(false);
    link_dirty.store(true);
    if (reassembly_mutex != nullptr) {
        xSemaphoreTake(reassembly_mutex, portMAX_DELAY);
        reassembly.active = false;
        reassembly.length = 0;
        xSemaphoreGive(reassembly_mutex);
    }
}

void BleServer::Impl::onMtuChange(uint16_t value) {
    mtu.store(value);
    updateCapability();
    if (capability_char != nullptr) {
        capability_char->notify();
    }
}

void BleServer::Impl::updateCapability() {
    if (capability_char == nullptr) return;
    uint8_t bytes[64] = {};
    const size_t length = buildCapability(mtu.load(), bytes, sizeof(bytes));
    if (length == 0) return;
    capability_char->setValue(bytes, length);
}

void BleServer::Impl::dropReassembly(uint8_t code, uint8_t sequence) {
    reassembly.active = false;
    reassembly.length = 0;
    sendStatus(code, sequence);
}

void BleServer::Impl::handleWrite(const uint8_t* data, size_t length, uint8_t expected_type) {
    if (data == nullptr || length < ble::kOverheadBytes) {
        const uint8_t sequence =
            (data != nullptr && length > 3) ? data[3] : 0;
        sendStatus(kBleStatusMalformedPayload, sequence);
        return;
    }

    const uint8_t sequence = data[3];
    if (data[0] != ble::kVersion) {
        sendStatus(kBleStatusVersionMismatch, sequence);
        return;
    }

    ble::Packet packet;
    if (!ble::parse(data, length, packet)) {
        sendStatus(kBleStatusCrcError, sequence);
        return;
    }
    if (packet.type != expected_type) {
        sendStatus(kBleStatusUnsupportedType, sequence);
        return;
    }
    if (packet.type == ble::kControl) {
        emitControl(packet);
        return;
    }
    if (packet.type != ble::kTelemetry && packet.type != ble::kNavigation) {
        sendStatus(kBleStatusUnsupportedType, sequence);
        return;
    }

    const bool fragmented = (packet.flags & ble::kFragmented) != 0;
    if (!fragmented) {
        xSemaphoreTake(reassembly_mutex, portMAX_DELAY);
        reassembly.active = false;
        reassembly.length = 0;
        xSemaphoreGive(reassembly_mutex);
        emitPacket(packet);
        return;
    }

    xSemaphoreTake(reassembly_mutex, portMAX_DELAY);
    if ((packet.flags & ble::kFirst) != 0) {
        reassembly.active = true;
        reassembly.length = 0;
        reassembly.sequence = packet.sequence;
        reassembly.type = packet.type;
        reassembly.started_ms = millis();
    } else if (!reassembly.active || reassembly.sequence != packet.sequence ||
               reassembly.type != packet.type) {
        dropReassembly(kBleStatusMalformedPayload, sequence);
        xSemaphoreGive(reassembly_mutex);
        return;
    }

    if (reassembly.length + packet.payload_length > sizeof(reassembly.buffer)) {
        dropReassembly(kBleStatusPayloadOverflow, sequence);
        xSemaphoreGive(reassembly_mutex);
        return;
    }
    std::memcpy(reassembly.buffer + reassembly.length, packet.payload, packet.payload_length);
    reassembly.length += packet.payload_length;

    if ((packet.flags & ble::kLast) != 0) {
        ble::Packet assembled = packet;
        assembled.payload = reassembly.buffer;
        assembled.payload_length = reassembly.length;
        reassembly.active = false;
        reassembly.length = 0;
        xSemaphoreGive(reassembly_mutex);
        emitPacket(assembled);
        return;
    }
    xSemaphoreGive(reassembly_mutex);
}

void BleServer::Impl::emitPacket(const ble::Packet& packet) {
    BleIncoming incoming;
    if (packet.type == ble::kTelemetry) {
        incoming.kind = BleIncoming::Kind::kTelemetry;
        if (!ble::decodeTelemetry(packet, incoming.telemetry)) {
            sendStatus(kBleStatusMalformedPayload, packet.sequence);
            return;
        }
    } else if (packet.type == ble::kNavigation) {
        incoming.kind = BleIncoming::Kind::kNavigation;
        if (!ble::decodeNavigation(packet, incoming.navigation)) {
            sendStatus(kBleStatusMalformedPayload, packet.sequence);
            return;
        }
    } else {
        sendStatus(kBleStatusUnsupportedType, packet.sequence);
        return;
    }

    if (queue == nullptr || xQueueSend(queue, &incoming, 0) != pdTRUE) {
        // The display keeps its last valid frame; the sender retries.
        return;
    }
    if ((packet.flags & ble::kAckRequired) != 0) {
        sendStatus(kBleStatusAck, packet.sequence);
    }
}

void BleServer::Impl::emitControl(const ble::Packet& packet) {
    if (packet.payload_length < 1) {
        sendStatus(kBleStatusMalformedPayload, packet.sequence);
        return;
    }
    const uint8_t command = packet.payload[0];
    const uint16_t value =
        packet.payload_length >= 3 ? readLe16(packet.payload + 1) : 0;
    (void)value;
    switch (command) {
    case 1: // subscribe
    case 2: // unsubscribe
    case 3: // ping
        sendStatus(kBleStatusAck, packet.sequence);
        break;
    default:
        sendStatus(kBleStatusUnsupportedType, packet.sequence);
        break;
    }
}

void BleServer::notifyStatus(uint8_t code, uint8_t ack_sequence, const char* detail) {
    impl_->sendStatus(code, ack_sequence, detail);
}

void BleServer::Impl::sendStatus(uint8_t code, uint8_t ack_sequence, const char* detail) {
    if (status_char == nullptr) return;
    uint8_t bytes[128] = {};
    const size_t detail_length =
        detail != nullptr ? std::min(std::strlen(detail), sizeof(bytes) - 2) : 0;
    bytes[0] = code;
    bytes[1] = ack_sequence;
    if (detail_length > 0) {
        std::memcpy(bytes + 2, detail, detail_length);
    }
    status_char->setValue(bytes, 2 + detail_length);
    status_char->notify();
}

void BleServer::Impl::checkReassemblyTimeout() {
    if (reassembly_mutex == nullptr) return;
    if (xSemaphoreTake(reassembly_mutex, 0) != pdTRUE) return;
    if (reassembly.active &&
        (uint32_t)(millis() - reassembly.started_ms) > kReassemblyTimeoutMs) {
        const uint8_t sequence = reassembly.sequence;
        reassembly.active = false;
        reassembly.length = 0;
        sendStatus(kBleStatusReassemblyTimeout, sequence);
    }
    xSemaphoreGive(reassembly_mutex);
}

} // namespace platform
} // namespace moto

#endif
