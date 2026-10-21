#pragma once

#include "lvgl.h"

#include "app/drag_session.h"
#include "app/moto_telemetry.h"
#include "app/moto_types.h"
#include "ui/ui_theme.h"

namespace moto {

class DragView {
public:
    void create(lv_obj_t* screen);
    void setPalette(const Palette& p) { palette_ = p; }
    void update(const DragSession& drag, const Telemetry& t, const PerfStats& perf,
                const Clock& clock, float dt);
    lv_obj_t* root() const { return root_; }

private:
    static void draw_cb(lv_event_t* e);
    void draw(lv_layer_t* layer);
    void draw_background(lv_layer_t* layer);
    void draw_header(lv_layer_t* layer);
    void draw_idle(lv_layer_t* layer);
    void draw_countdown(lv_layer_t* layer);
    void draw_running(lv_layer_t* layer);
    void draw_finished(lv_layer_t* layer);
    void draw_card(lv_layer_t* layer, int x, int y, int w, int h);

    lv_obj_t* root_ = nullptr;
    Palette palette_ = racing_palette();
    DragPhase phase_ = DragPhase::Idle;
    DragSettings settings_{};
    DragResult result_{};
    Telemetry telemetry_{};
    PerfStats perf_{};
    Clock clock_{};
    float elapsed_ = 0.0f;
    float speed_ = 0.0f;
    float distance_ = 0.0f;
    float progress_ = 0.0f;
    float countdown_ = 0.0f;
    int countdown_digit_ = 0;
    float best_ = 0.0f;
    float best_delta_ = 0.0f;
    float last_split_s_ = -1.0f;
    int last_split_m_ = 0;
    float accel_s_ = 0.0f;
    bool accel_complete_ = false;
    bool new_best_ = false;
    bool gps_ok_ = true;
    float anim_ = 0.0f;
    float stats_accum_ = 0.0f;
};

} // namespace moto
