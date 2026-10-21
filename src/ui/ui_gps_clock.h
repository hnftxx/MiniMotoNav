#pragma once

#include "lvgl.h"

#include "app/moto_telemetry.h"
#include "app/moto_types.h"
#include "ui/ui_theme.h"

namespace moto {

class GpsClockView {
public:
    void create(lv_obj_t* screen);
    void setPalette(const Palette& p) { palette_ = p; }
    void update(const Telemetry& t, const PerfStats& perf, const Clock& clock, float dt);
    lv_obj_t* root() const { return root_; }

private:
    static void draw_cb(lv_event_t* e);
    void draw(lv_layer_t* layer);
    void draw_background(lv_layer_t* layer);
    void draw_clock(lv_layer_t* layer);
    void draw_compass(lv_layer_t* layer);
    void draw_stats(lv_layer_t* layer);

    lv_obj_t* root_ = nullptr;
    Palette palette_ = racing_palette();
    Telemetry telemetry_{};
    PerfStats perf_{};
    Clock clock_{};
    float anim_ = 0.0f;
    float stats_accum_ = 0.0f;
};

} // namespace moto
