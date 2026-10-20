#pragma once

#include "lvgl.h"

#include "app/moto_telemetry.h"
#include "app/moto_types.h"
#include "ui/ui_theme.h"

namespace moto {

class SpeedoView {
public:
    void create(lv_obj_t* screen);
    void setPalette(const Palette& p) { palette_ = p; }
    void update(const Telemetry& t, const PerfStats& perf, const Clock& clock, float dt);
    lv_obj_t* root() const { return root_; }

    void onThemeChanged() { needs_redraw_ = true; }

private:
    struct Digit {
        int cur = 0;
        int prev = 0;
        float progress = 1.0f;
        lv_obj_t* widget = nullptr;
        float x = -80.0f;
        float target_x = -80.0f;
        bool active = false;
    };

    static void root_draw_cb(lv_event_t* e);
    static void digit_draw_cb(lv_event_t* e);

    void draw(lv_layer_t* layer);
    void draw_header(lv_layer_t* layer);
    void draw_gauge(lv_layer_t* layer);
    void draw_panel(lv_layer_t* layer);
    void draw_digit(lv_layer_t* layer, int place);
    void update_digits(int value, float dt);

    lv_obj_t* root_ = nullptr;
    lv_obj_t* screen_ = nullptr;
    Palette palette_ = racing_palette();
    PerfStats perf_{};
    Telemetry telemetry_{};
    Clock clock_{};

    Digit digits_[3];
    float display_speed_ = 0.0f;
    int shown_value_ = -1;
    float shimmer_phase_ = 0.0f;
    bool needs_redraw_ = true;
};

} // namespace moto
