#pragma once

#include "lvgl.h"

#include "app/display_feed.h"
#include "app/drag_session.h"
#include "app/moto_command.h"
#include "app/moto_telemetry.h"
#include "app/moto_types.h"
#include "app/perf_model.h"
#include "ui/ui_controls.h"
#include "ui/ui_drag.h"
#include "ui/ui_gps_clock.h"
#include "ui/ui_nav.h"
#include "ui/ui_speedo.h"
#include "ui/ui_theme.h"

namespace moto {

class MotoApp {
public:
    void init();
    void update(float dt);

    void setPerf(float fps, float frame_ms, float render_ms, float worst_ms);
    void setHardwareEstimate(const HardwareEstimate& estimate);
    void setHardwareMeasured(float fps, float frame_ms, uint32_t spi_hz, float spi_ms = 0.0f,
                             uint32_t pixels = 0, uint16_t rects = 0);

    void postCommand(const Command& command);
    void handleCommand(const Command& command);
    void pollCommands();

    const Telemetry& telemetry() const { return feed_active_ ? feed_.telemetry() : sim_.data(); }
    const PerfStats& perf() const { return perf_; }
    bool bleActive() const { return feed_active_; }

    void applyBleTelemetry(const ble::TelemetryFields& fields, uint32_t now_ms);
    void applyBleNavigation(const ble::NavigationFields& navigation, uint32_t now_ms);
    void setBleLinked(bool linked, uint32_t now_ms);
    void tickBle(uint32_t now_ms);
    void clearBleFeed();
    bool nightMode() const { return theme_ == Theme::Night; }
    Theme theme() const { return theme_; }
    AppMode mode() const { return mode_; }
    bool navMode() const { return mode_ == AppMode::Nav; }
    DragSession& dragSession() { return drag_; }
    const DragSession& dragSession() const { return drag_; }
    bool controlsVisible() const { return controls_.visible(); }

    void togglePause();
    void toggleAuto();
    void resetSim();
    void toggleTheme();
    void setTheme(Theme theme);
    void toggleMode();
    void setMode(AppMode mode, bool animate = true);
    void cycleMode();
    void toggleDragRun();
    void resetDrag();
    void toggleControls();
    void setControlsVisible(bool visible);
    void toggleFps();

    void adjustSpeed(float delta);
    void adjustThrottle(float delta);
    void adjustRpm(float delta);

    void setManualSpeed(float kph);
    void setManualThrottle(float percent);
    void setManualRpm(float rpm);

private:
    void applyPalette();
    void loadScreen(AppMode mode, bool animate = true);

    TelemetrySim sim_;
    DisplayFeed feed_;
    bool feed_active_ = false;
    Clock clock_;
    PerfStats perf_;
    CommandQueue commands_;

    lv_obj_t* speedo_screen_ = nullptr;
    lv_obj_t* nav_screen_ = nullptr;
    lv_obj_t* drag_screen_ = nullptr;
    lv_obj_t* gps_screen_ = nullptr;
    SpeedoView speedo_;
    NavView nav_;
    DragView drag_view_;
    GpsClockView gps_view_;
    DragSession drag_;
    UiControls controls_;

    Palette palette_ = racing_palette();
    Theme theme_ = Theme::Race;
    Palette theme_from_ = racing_palette();
    float theme_t_ = 1.0f;
    AppMode mode_ = AppMode::Speedo;
    uint64_t now_us_ = 0;
    float clock_accum_s_ = 0.0f;
    bool initialized_ = false;
};

} // namespace moto
