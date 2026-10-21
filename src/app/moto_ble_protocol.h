#pragma once

#include <cstddef>
#include <cstdint>

namespace moto {
namespace ble {

constexpr uint8_t kVersion = 1;
constexpr size_t kHeaderBytes = 12;
constexpr size_t kOverheadBytes = 14;

enum Type : uint8_t {
    kTelemetry = 1,
    kNavigation = 2,
    kControl = 3,
    kStatus = 4,
    kCapability = 5,
    kRouteGeometry = 6,
};

enum Flag : uint8_t {
    kFirst = 1 << 0,
    kLast = 1 << 1,
    kValid = 1 << 2,
    kAckRequired = 1 << 3,
    kFragmented = 1 << 4,
};

struct Packet {
    uint8_t type = 0;
    uint8_t flags = 0;
    uint8_t sequence = 0;
    uint32_t timestamp_s = 0;
    uint16_t data_age_ms = 0;
    const uint8_t* payload = nullptr;
    size_t payload_length = 0;
};

struct TelemetryFields {
    bool has_rpm = false;
    bool has_speed = false;
    bool has_throttle = false;
    bool has_afr = false;
    bool has_engine_temp = false;
    bool has_intake_temp = false;
    bool has_battery = false;
    bool has_advance = false;
    bool has_gear = false;
    uint16_t rpm = 0;
    float speed_kph = 0.0f;
    float throttle_percent = 0.0f;
    float afr = 0.0f;
    float engine_temp_c = 0.0f;
    float intake_temp_c = 0.0f;
    float battery_volts = 0.0f;
    float advance_deg = 0.0f;
    int8_t gear = 0;
};

enum Maneuver : uint8_t {
    kManeuverNone = 0,
    kManeuverStraight = 1,
    kManeuverSlightLeft = 2,
    kManeuverLeft = 3,
    kManeuverSharpLeft = 4,
    kManeuverSlightRight = 5,
    kManeuverRight = 6,
    kManeuverSharpRight = 7,
    kManeuverUTurn = 8,
    kManeuverRoundabout = 9,
    kManeuverArrive = 10,
};

struct NavigationFields {
    bool route_valid = false;
    bool gps_fix = false;
    bool has_eta = false;
    uint8_t maneuver = kManeuverNone;
    uint16_t distance_m = 0;
    float remaining_km = 0.0f;
    uint32_t eta_s = 0;
    float gps_accuracy_m = 0.0f;
    uint8_t route_points = 0;
    char road[41] = {};
};

struct RoutePoint {
    double latitude;
    double longitude;
};

struct Capability {
    uint8_t version = 0;
    uint8_t max_mtu = 0;
    uint8_t flags = 0;
    uint16_t max_payload = 0;
    char name[33] = {};
};

uint16_t crc16(const uint8_t* data, size_t length);
bool parse(const uint8_t* data, size_t length, Packet& out);

size_t encodeTelemetry(const TelemetryFields& fields, uint8_t sequence, uint32_t timestamp_s,
                       uint16_t data_age_ms, uint8_t* out, size_t capacity);
bool decodeTelemetry(const Packet& packet, TelemetryFields& out);
bool decodeNavigation(const Packet& packet, NavigationFields& out);
bool decodeCapability(const Packet& packet, Capability& out);
bool decodeRouteGeometry(const Packet& packet, RoutePoint* points, size_t capacity, size_t& count);

} // namespace ble
} // namespace moto
