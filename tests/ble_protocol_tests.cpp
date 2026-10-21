#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "app/display_feed.h"
#include "app/moto_ble_protocol.h"

namespace {

using namespace moto::ble;

size_t buildPacket(uint8_t type, uint8_t flags, uint8_t sequence, uint32_t timestamp,
                   uint16_t age, const uint8_t* payload, uint16_t length, uint8_t* out) {
    out[0] = kVersion;
    out[1] = type;
    out[2] = flags;
    out[3] = sequence;
    out[4] = (uint8_t)(timestamp & 0xFF);
    out[5] = (uint8_t)((timestamp >> 8) & 0xFF);
    out[6] = (uint8_t)((timestamp >> 16) & 0xFF);
    out[7] = (uint8_t)((timestamp >> 24) & 0xFF);
    out[8] = (uint8_t)(age & 0xFF);
    out[9] = (uint8_t)((age >> 8) & 0xFF);
    out[10] = (uint8_t)(length & 0xFF);
    out[11] = (uint8_t)((length >> 8) & 0xFF);
    if (length > 0) std::memcpy(out + kHeaderBytes, payload, length);
    const uint16_t crc = crc16(out, kHeaderBytes + length);
    out[kHeaderBytes + length] = (uint8_t)(crc & 0xFF);
    out[kHeaderBytes + length + 1] = (uint8_t)(crc >> 8);
    return kHeaderBytes + length + 2;
}

void testTelemetryRoundTrip() {
    TelemetryFields source;
    source.has_rpm = true;
    source.rpm = 4200;
    source.has_speed = true;
    source.speed_kph = 63.4f;
    source.has_throttle = true;
    source.throttle_percent = 41.5f;
    source.has_afr = true;
    source.afr = 13.2f;
    source.has_engine_temp = true;
    source.engine_temp_c = 88.3f;
    source.has_intake_temp = true;
    source.intake_temp_c = 31.6f;
    source.has_battery = true;
    source.battery_volts = 13.84f;
    source.has_advance = true;
    source.advance_deg = 18.5f;
    source.has_gear = true;
    source.gear = 3;

    uint8_t bytes[64] = {};
    const size_t length = encodeTelemetry(source, 7, 1792584000u, 120, bytes, sizeof(bytes));
    assert(length == 30);

    Packet packet;
    assert(parse(bytes, length, packet));
    assert(packet.type == kTelemetry);
    assert(packet.sequence == 7);
    assert(packet.timestamp_s == 1792584000u);
    assert(packet.data_age_ms == 120);

    TelemetryFields decoded;
    assert(decodeTelemetry(packet, decoded));
    assert(decoded.has_rpm && decoded.rpm == 4200);
    assert(decoded.has_speed && std::fabs(decoded.speed_kph - 63.4f) < 0.01f);
    assert(decoded.has_throttle && std::fabs(decoded.throttle_percent - 41.5f) < 0.01f);
    assert(decoded.has_afr && std::fabs(decoded.afr - 13.2f) < 0.01f);
    assert(decoded.has_engine_temp && std::fabs(decoded.engine_temp_c - 88.3f) < 0.01f);
    assert(decoded.has_intake_temp && std::fabs(decoded.intake_temp_c - 31.6f) < 0.01f);
    assert(decoded.has_battery && std::fabs(decoded.battery_volts - 13.84f) < 0.001f);
    assert(decoded.has_advance && std::fabs(decoded.advance_deg - 18.5f) < 0.01f);
    assert(decoded.has_gear && decoded.gear == 3);
}

void testDartParity() {
    // Produced by companion/lib/src/ble/ble_protocol.dart encodeTelemetry + encode.
    const uint8_t expected[] = {0x01, 0x01, 0x04, 0x07, 0x40, 0xA9, 0xD8, 0x6A, 0x78,
                                0x00, 0x10, 0x00, 0xFF, 0x01, 0x68, 0x10, 0x7A, 0x02,
                                0x53, 0x84, 0x73, 0x03, 0x3C, 0x01, 0x10, 0x36, 0x25,
                                0x03, 0x62, 0x41};

    TelemetryFields source;
    source.has_rpm = true;
    source.rpm = 4200;
    source.has_speed = true;
    source.speed_kph = 63.4f;
    source.has_throttle = true;
    source.throttle_percent = 41.5f;
    source.has_afr = true;
    source.afr = 13.2f;
    source.has_engine_temp = true;
    source.engine_temp_c = 88.3f;
    source.has_intake_temp = true;
    source.intake_temp_c = 31.6f;
    source.has_battery = true;
    source.battery_volts = 13.84f;
    source.has_advance = true;
    source.advance_deg = 18.5f;
    source.has_gear = true;
    source.gear = 3;

    uint8_t bytes[64] = {};
    const size_t length = encodeTelemetry(source, 7, 1792584000u, 120, bytes, sizeof(bytes));
    assert(length == sizeof(expected));
    assert(std::memcmp(bytes, expected, length) == 0);

    Packet packet;
    assert(parse(expected, sizeof(expected), packet));
    TelemetryFields decoded;
    assert(decodeTelemetry(packet, decoded));
    assert(decoded.rpm == 4200);
    assert(decoded.gear == 3);
}

void testIntegrity() {
    TelemetryFields fields;
    fields.has_rpm = true;
    fields.rpm = 1000;
    uint8_t bytes[64] = {};
    const size_t length = encodeTelemetry(fields, 1, 1, 0, bytes, sizeof(bytes));
    Packet packet;
    assert(parse(bytes, length, packet));

    bytes[2] ^= 0x01;
    assert(!parse(bytes, length, packet));
    bytes[2] ^= 0x01;

    bytes[0] = 2;
    assert(!parse(bytes, length, packet));
    bytes[0] = kVersion;

    assert(!parse(bytes, length - 1, packet));
}

void testNavigation() {
    uint8_t payload[64] = {};
    payload[0] = 0x0B;
    payload[2] = kManeuverLeft;
    payload[3] = 0x2C;
    payload[4] = 0x01;
    payload[5] = 0x7C;
    payload[6] = 0x00;
    const uint32_t eta = 1792585000u;
    payload[7] = (uint8_t)(eta & 0xFF);
    payload[8] = (uint8_t)(eta >> 8);
    payload[9] = (uint8_t)(eta >> 16);
    payload[10] = (uint8_t)(eta >> 24);
    payload[11] = 7;
    payload[12] = 5;
    payload[13] = 4;
    std::memcpy(payload + 14, "Jl. A", 5);

    uint8_t bytes[96] = {};
    const size_t length = buildPacket(kNavigation, kValid, 2, 1792584000u, 0, payload, 19, bytes);
    Packet packet;
    assert(parse(bytes, length, packet));

    NavigationFields navigation;
    assert(decodeNavigation(packet, navigation));
    assert(navigation.route_valid && navigation.gps_fix);
    assert(navigation.has_eta);
    assert(navigation.maneuver == kManeuverLeft);
    assert(navigation.distance_m == 300);
    assert(std::fabs(navigation.remaining_km - 12.4f) < 0.01f);
    assert(navigation.eta_s == eta);
    assert(std::fabs(navigation.gps_accuracy_m - 3.5f) < 0.01f);
    assert(std::strcmp(navigation.road, "Jl. A") == 0);
}

void testDartNavigationParity() {
    // Produced by companion/lib/src/ble/ble_protocol.dart encodeNavigation + encode.
    const uint8_t expected[] = {0x01, 0x02, 0x04, 0x02, 0x40, 0xA9, 0xD8, 0x6A, 0x00, 0x00,
                                0x13, 0x00, 0x0B, 0x00, 0x03, 0x2C, 0x01, 0x7C, 0x00, 0x28,
                                0xAD, 0xD8, 0x6A, 0x07, 0x05, 0x04, 0x4A, 0x6C, 0x2E, 0x20,
                                0x41, 0x12, 0x70};
    Packet packet;
    assert(parse(expected, sizeof(expected), packet));
    NavigationFields navigation;
    assert(decodeNavigation(packet, navigation));
    assert(navigation.route_valid && navigation.gps_fix && navigation.has_eta);
    assert(navigation.maneuver == kManeuverLeft);
    assert(navigation.distance_m == 300);
    assert(navigation.remaining_km == 12.4f);
    assert(navigation.gps_accuracy_m == 3.5f);
    assert(std::strcmp(navigation.road, "Jl. A") == 0);
}

void testRouteGeometry() {
    const int32_t lats[] = {1000000, 1000010};
    const int32_t lons[] = {2000000, 2000020};
    uint8_t payload[2 + 2 * 8] = {};
    payload[0] = 2;
    for (int i = 0; i < 2; ++i) {
        const uint32_t delta_lat = i == 0 ? (uint32_t)lats[i] : (uint32_t)(lats[i] - lats[i - 1]);
        const uint32_t delta_lon = i == 0 ? (uint32_t)lons[i] : (uint32_t)(lons[i] - lons[i - 1]);
        std::memcpy(payload + 2 + i * 8, &delta_lat, 4);
        std::memcpy(payload + 2 + i * 8 + 4, &delta_lon, 4);
    }
    uint8_t bytes[64] = {};
    const size_t length = buildPacket(kRouteGeometry, kValid, 3, 10, 0, payload, sizeof(payload), bytes);
    Packet packet;
    assert(parse(bytes, length, packet));

    RoutePoint points[4];
    size_t count = 0;
    assert(decodeRouteGeometry(packet, points, 4, count));
    assert(count == 2);
    assert(std::fabs(points[0].latitude - 0.1) < 1e-9);
    assert(std::fabs(points[1].longitude - 0.200002) < 1e-9);
}

void testDisplayFeed() {
    moto::DisplayFeed feed;
    TelemetryFields fields;
    fields.has_speed = true;
    fields.speed_kph = 72.0f;
    fields.has_rpm = true;
    fields.rpm = 6100;
    fields.has_gear = true;
    fields.gear = 4;
    feed.setLinked(true, 1000);
    feed.applyTelemetry(fields, 1000);
    feed.tick(1200);
    assert(!feed.state().stale);
    assert(feed.telemetry().speed_kph == 72.0f);
    assert(feed.telemetry().rpm == 6100.0f);
    assert(feed.telemetry().gear == 4);

    feed.tick(4000);
    assert(feed.state().stale);
    assert(feed.telemetry().speed_kph == 72.0f);

    feed.applyTelemetry(fields, 4100);
    feed.tick(4200);
    assert(!feed.state().stale);
    feed.setLinked(false, 4200);
    assert(feed.state().stale);
}

} // namespace

int main() {
    testTelemetryRoundTrip();
    testDartParity();
    testIntegrity();
    testNavigation();
    testDartNavigationParity();
    testRouteGeometry();
    testDisplayFeed();
    return 0;
}
