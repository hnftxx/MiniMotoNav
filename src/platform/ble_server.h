#pragma once

#if defined(ARDUINO)

#include <cstdint>

#include "app/moto_ble_protocol.h"

namespace moto {
namespace platform {

struct BleIncoming {
    enum class Kind : uint8_t { kTelemetry, kNavigation };
    Kind kind = Kind::kTelemetry;
    ble::TelemetryFields telemetry;
    ble::NavigationFields navigation;
};

enum BleStatusCode : uint8_t {
    kBleStatusAck = 1,
    kBleStatusVersionMismatch = 2,
    kBleStatusUnsupportedType = 3,
    kBleStatusCrcError = 4,
    kBleStatusReassemblyTimeout = 5,
    kBleStatusPayloadOverflow = 6,
    kBleStatusMalformedPayload = 7,
};

// NimBLE GATT peripheral for the version-1 GODSPEED display protocol.
// Android is the central. Telemetry and navigation writes are validated,
// reassembled, and queued for the render loop; control messages are answered
// with a status notification. No ECU write path exists here.
class BleServer {
public:
    struct Impl;

    BleServer();
    ~BleServer();

    void begin(const char* name = "GODSPEED");
    bool poll(BleIncoming& out);
    bool pollLinkChange(bool& linked);
    void notifyStatus(uint8_t code, uint8_t ack_sequence, const char* detail = nullptr);
    bool connected() const;
    uint16_t mtu() const;

private:
    Impl* impl_ = nullptr;
};

} // namespace platform
} // namespace moto

#endif
