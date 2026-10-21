#include "app/moto_ble_protocol.h"

#include <cmath>
#include <cstring>

namespace moto {
namespace ble {
namespace {

uint16_t read16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t read32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
int16_t read16s(const uint8_t* p) { return (int16_t)read16(p); }
void write16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}
void write32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}
uint8_t clamp_u8(float value, float scale, float lo, float hi) {
    const float scaled = std::round(value * scale);
    if (scaled < lo) return (uint8_t)lo;
    if (scaled > hi) return (uint8_t)hi;
    return (uint8_t)scaled;
}
int16_t clamp_i16(float value, float scale, float lo, float hi) {
    const float scaled = std::round(value * scale);
    if (scaled < lo) return (int16_t)lo;
    if (scaled > hi) return (int16_t)hi;
    return (int16_t)scaled;
}
int8_t clamp_i8(float value, float scale, float lo, float hi) {
    const float scaled = std::round(value * scale);
    if (scaled < lo) return (int8_t)lo;
    if (scaled > hi) return (int8_t)hi;
    return (int8_t)scaled;
}

} // namespace

uint16_t crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

bool parse(const uint8_t* data, size_t length, Packet& out) {
    if (data == nullptr || length < kOverheadBytes) return false;
    if (data[0] != kVersion) return false;
    const uint16_t payload_length = read16(data + 10);
    if (length != kHeaderBytes + payload_length + 2) return false;
    const uint16_t expected = read16(data + kHeaderBytes + payload_length);
    if (crc16(data, kHeaderBytes + payload_length) != expected) return false;
    out.type = data[1];
    out.flags = data[2];
    out.sequence = data[3];
    out.timestamp_s = read32(data + 4);
    out.data_age_ms = read16(data + 8);
    out.payload = data + kHeaderBytes;
    out.payload_length = payload_length;
    return true;
}

size_t encodeTelemetry(const TelemetryFields& fields, uint8_t sequence, uint32_t timestamp_s,
                       uint16_t data_age_ms, uint8_t* out, size_t capacity) {
    constexpr size_t kPayload = 16;
    constexpr size_t kTotal = kHeaderBytes + kPayload + 2;
    if (out == nullptr || capacity < kTotal) return 0;
    uint8_t payload[kPayload] = {};
    uint16_t mask = 0;
    if (fields.has_rpm) {
        mask |= 1 << 0;
        write16(payload + 2, (uint16_t)fields.rpm);
    }
    if (fields.has_speed) {
        mask |= 1 << 1;
        write16(payload + 4, (uint16_t)clamp_i16(fields.speed_kph, 10.0f, 0.0f, 4000.0f));
    }
    if (fields.has_throttle) {
        mask |= 1 << 2;
        payload[6] = clamp_u8(fields.throttle_percent, 2.0f, 0.0f, 200.0f);
    }
    if (fields.has_afr) {
        mask |= 1 << 3;
        payload[7] = clamp_u8(fields.afr, 10.0f, 50.0f, 250.0f);
    }
    if (fields.has_engine_temp) {
        mask |= 1 << 4;
        write16(payload + 8, (uint16_t)clamp_i16(fields.engine_temp_c, 10.0f, -600.0f, 3000.0f));
    }
    if (fields.has_intake_temp) {
        mask |= 1 << 5;
        write16(payload + 10, (uint16_t)clamp_i16(fields.intake_temp_c, 10.0f, -600.0f, 3000.0f));
    }
    if (fields.has_battery) {
        mask |= 1 << 6;
        write16(payload + 12, (uint16_t)clamp_i16(fields.battery_volts, 1000.0f, 0.0f, 30000.0f));
    }
    if (fields.has_advance) {
        mask |= 1 << 7;
        payload[14] = (uint8_t)clamp_i8(fields.advance_deg, 2.0f, -120.0f, 160.0f);
    }
    if (fields.has_gear) {
        mask |= 1 << 8;
        payload[15] = (uint8_t)fields.gear;
    }
    write16(payload, mask);

    out[0] = kVersion;
    out[1] = kTelemetry;
    out[2] = kValid;
    out[3] = sequence;
    write32(out + 4, timestamp_s);
    write16(out + 8, data_age_ms);
    write16(out + 10, (uint16_t)kPayload);
    std::memcpy(out + kHeaderBytes, payload, kPayload);
    write16(out + kHeaderBytes + kPayload, crc16(out, kHeaderBytes + kPayload));
    return kTotal;
}

bool decodeTelemetry(const Packet& packet, TelemetryFields& out) {
    if (packet.type != kTelemetry || packet.payload_length != 16) return false;
    const uint8_t* p = packet.payload;
    const uint16_t mask = read16(p);
    out = TelemetryFields{};
    if (mask & (1 << 0)) {
        out.has_rpm = true;
        out.rpm = read16(p + 2);
    }
    if (mask & (1 << 1)) {
        out.has_speed = true;
        out.speed_kph = read16(p + 4) / 10.0f;
    }
    if (mask & (1 << 2)) {
        out.has_throttle = true;
        out.throttle_percent = p[6] / 2.0f;
    }
    if (mask & (1 << 3)) {
        out.has_afr = true;
        out.afr = p[7] / 10.0f;
    }
    if (mask & (1 << 4)) {
        out.has_engine_temp = true;
        out.engine_temp_c = read16s(p + 8) / 10.0f;
    }
    if (mask & (1 << 5)) {
        out.has_intake_temp = true;
        out.intake_temp_c = read16s(p + 10) / 10.0f;
    }
    if (mask & (1 << 6)) {
        out.has_battery = true;
        out.battery_volts = read16(p + 12) / 1000.0f;
    }
    if (mask & (1 << 7)) {
        out.has_advance = true;
        out.advance_deg = (int8_t)p[14] / 2.0f;
    }
    if (mask & (1 << 8)) {
        out.has_gear = true;
        out.gear = (int8_t)p[15];
    }
    return true;
}

bool decodeNavigation(const Packet& packet, NavigationFields& out) {
    if (packet.type != kNavigation || packet.payload_length < 14) return false;
    const uint8_t* p = packet.payload;
    const uint16_t flags = read16(p);
    const uint8_t road_length = p[12];
    if (packet.payload_length < 14u + road_length) return false;
    out = NavigationFields{};
    out.route_valid = (flags & 1) != 0;
    out.gps_fix = (flags & 2) != 0;
    out.has_eta = (flags & 8) != 0;
    out.maneuver = p[2];
    out.distance_m = p[3] | (p[4] << 8);
    out.remaining_km = (p[5] | (p[6] << 8)) / 10.0f;
    out.eta_s = read32(p + 7);
    out.gps_accuracy_m = p[11] / 2.0f;
    out.route_points = p[13];
    const size_t copy = road_length < 40 ? road_length : 40;
    std::memcpy(out.road, p + 14, copy);
    out.road[copy] = '\0';
    return true;
}

bool decodeCapability(const Packet& packet, Capability& out) {
    if (packet.type != kCapability || packet.payload_length < 8) return false;
    const uint8_t* p = packet.payload;
    const uint16_t name_length = read16(p + 6);
    if (packet.payload_length < 8u + name_length) return false;
    out = Capability{};
    out.version = p[0];
    out.max_mtu = p[1];
    out.flags = p[2];
    out.max_payload = read16(p + 4);
    const size_t copy = name_length < 32 ? name_length : 32;
    std::memcpy(out.name, p + 8, copy);
    out.name[copy] = '\0';
    return true;
}

bool decodeRouteGeometry(const Packet& packet, RoutePoint* points, size_t capacity, size_t& count) {
    count = 0;
    if (packet.type != kRouteGeometry || packet.payload_length < 2) return false;
    const uint16_t total = read16(packet.payload);
    if (total > 512) return false;
    if (packet.payload_length != 2u + (size_t)total * 8u) return false;
    if (total > capacity) return false;
    int32_t lat = 0;
    int32_t lon = 0;
    for (uint16_t i = 0; i < total; ++i) {
        const uint8_t* p = packet.payload + 2 + (size_t)i * 8;
        lat += (int32_t)read32(p);
        lon += (int32_t)read32(p + 4);
        points[i].latitude = lat / 10000000.0;
        points[i].longitude = lon / 10000000.0;
    }
    count = total;
    return true;
}

} // namespace ble
} // namespace moto
