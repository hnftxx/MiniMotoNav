#include "app/display_feed.h"

namespace moto {

void DisplayFeed::applyTelemetry(const ble::TelemetryFields& fields, uint32_t now_ms) {
    Telemetry& t = state_.telemetry;
    if (fields.has_rpm) t.rpm = fields.rpm;
    if (fields.has_speed) {
        t.speed_kph = fields.speed_kph;
        t.target_kph = fields.speed_kph;
    }
    if (fields.has_throttle) t.throttle = fields.throttle_percent;
    if (fields.has_gear) t.gear = fields.gear;
    if (fields.has_afr) t.afr = fields.afr;
    if (fields.has_engine_temp) t.engine_temp_c = fields.engine_temp_c;
    if (fields.has_intake_temp) t.intake_temp_c = fields.intake_temp_c;
    if (fields.has_battery) t.battery_volts = fields.battery_volts;
    if (fields.has_advance) t.advance_deg = fields.advance_deg;
    t.running = true;
    t.auto_drive = false;
    state_.has_telemetry = true;
    state_.last_telemetry_ms = now_ms;
    state_.stale = false;
}

void DisplayFeed::applyNavigation(const ble::NavigationFields& navigation, uint32_t now_ms) {
    state_.navigation = navigation;
    state_.has_navigation = true;
    state_.last_navigation_ms = now_ms;
    state_.telemetry.gps_fix = navigation.gps_fix;
    state_.telemetry.gps_accuracy_m = navigation.gps_accuracy_m;
}

void DisplayFeed::setLinked(bool linked, uint32_t now_ms) {
    state_.linked = linked;
    if (!linked) {
        state_.stale = true;
    } else if (state_.has_telemetry) {
        state_.last_telemetry_ms = now_ms;
    }
}

void DisplayFeed::tick(uint32_t now_ms, uint32_t stale_after_ms) {
    if (!state_.has_telemetry) {
        state_.stale = true;
        return;
    }
    const uint32_t age = now_ms - state_.last_telemetry_ms;
    state_.stale = !state_.linked || age > stale_after_ms;
}

} // namespace moto
