#pragma once

#include "lvgl.h"

#include "app/moto_telemetry.h"
#include "app/moto_types.h"
#include "ui/ui_controls.h"
#include "ui/ui_nav.h"
#include "ui/ui_speedo.h"
#include "ui/ui_theme.h"

namespace moto {

class MotoApp {
public:
    void init();
    void update(float dt);

    void setPerf(float fps, float frame_ms, float render_ms, float worst_ms);

    const Telemetry& telemetry() const { return sim_.data(); }
    const PerfStats& perf() const { return perf_; }
    bool nightMode() const { return theme_ == Theme::Night; }
    Theme theme() const { return theme_; }
    bool navMode() const { return nav_mode_; }
    bool controlsVisible() const { return controls_.visible(); }

    void togglePause();
    void toggleAuto();
    void resetSim();
    void toggleTheme();
    void setTheme(Theme theme);
    void toggleMode();
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
    void loadScreen(bool nav, bool animate = true);

    TelemetrySim sim_;
    Clock clock_;
    PerfStats perf_;

    lv_obj_t* speedo_screen_ = nullptr;
    lv_obj_t* nav_screen_ = nullptr;
    SpeedoView speedo_;
    NavView nav_;
    UiControls controls_;

    Palette palette_ = racing_palette();
    Theme theme_ = Theme::Race;
    Palette theme_from_ = racing_palette();
    float theme_t_ = 1.0f;
    bool nav_mode_ = false;
    float clock_accum_s_ = 0.0f;
    bool initialized_ = false;
};

} // namespace moto
