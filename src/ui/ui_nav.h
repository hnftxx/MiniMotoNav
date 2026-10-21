#pragma once

#include <vector>

#include "lvgl.h"

#include "app/moto_telemetry.h"
#include "app/moto_types.h"
#include "ui/ui_theme.h"

namespace moto {

class NavView {
public:
    void create(lv_obj_t* screen);
    void reset();
    void setPalette(const Palette& p) { palette_ = p; }
    void update(const Telemetry& t, const PerfStats& perf, const Clock& clock, float dt);
    lv_obj_t* root() const { return root_; }

private:
    struct Point {
        float x;
        float y;
        float dist;
    };
    struct Maneuver {
        float dist;
        int street;   // name of the street entered after the turn
        int turn;     // +1 right, -1 left
    };
    struct Projected {
        int x;
        int y;
        bool on_screen;
    };

    static void draw_cb(lv_event_t* e);
    void draw(lv_layer_t* layer);
    void draw_map(lv_layer_t* layer);
    void draw_route(lv_layer_t* layer);
    void draw_hud(lv_layer_t* layer);
    void build_route();
    void project(float wx, float wy, Projected& out) const;
    int next_maneuver_index() const;
    void update_strings();

    static constexpr int kWidth = 320;
    static constexpr int kHeight = 240;
    static constexpr float kScale = 2.6f;
    static constexpr int kCarX = 160;
    static constexpr int kCarY = 124;

    lv_obj_t* root_ = nullptr;
    Palette palette_ = day_palette();
    PerfStats perf_{};
    Clock clock_{};

    std::vector<Point> route_;
    std::vector<Maneuver> maneuvers_;
    float route_len_ = 0.0f;
    float route_dist_ = 0.0f;
    int route_hint_ = 0;
    float car_x_ = -700.0f;
    float car_y_ = 500.0f;
    float heading_ = 0.0f;
    float heading_target_ = 0.0f;
    float map_time_ = 0.0f;
    float redraw_accum_ = 0.0f;

    char turn_dist_[16] = "250 m";
    char turn_street_[40] = "Jl. Margonda Raya";
    char remain_dist_[16] = "5.3 km";
    char remain_time_[16] = "8 min";
    char eta_[16] = "14:40";
    char fps_[24] = "";
    char hw_a_[32] = "";
    char hw_b_[32] = "";
    char hw_c_[32] = "";
    int next_turn_dir_ = 1;
};

} // namespace moto
