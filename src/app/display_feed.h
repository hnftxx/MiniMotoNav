#pragma once

#include <cstdint>

#include "app/moto_ble_protocol.h"
#include "app/moto_telemetry.h"

namespace moto {

struct DisplayState {
    Telemetry telemetry;
    ble::NavigationFields navigation;
    bool has_telemetry = false;
    bool has_navigation = false;
    bool linked = false;
    bool stale = true;
    uint32_t last_telemetry_ms = 0;
    uint32_t last_navigation_ms = 0;
};

// Applies validated BLE fields to the existing display model. The LVGL views
// keep their current inputs; only the values behind those inputs change.
class DisplayFeed {
public:
    void applyTelemetry(const ble::TelemetryFields& fields, uint32_t now_ms);
    void applyNavigation(const ble::NavigationFields& navigation, uint32_t now_ms);
    void setLinked(bool linked, uint32_t now_ms);
    void tick(uint32_t now_ms, uint32_t stale_after_ms = 2000);

    const DisplayState& state() const { return state_; }
    const Telemetry& telemetry() const { return state_.telemetry; }
    const ble::NavigationFields& navigation() const { return state_.navigation; }

private:
    DisplayState state_{};
};

} // namespace moto
