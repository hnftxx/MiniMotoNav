#pragma once

#include <cstdint>
#include <string>

#include "lvgl.h"

#include "app/moto_types.h"
#include "app/perf_model.h"
#include "ui/ui_theme.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace moto {

class MotoApp;

struct SimOptions {
    int scale = 3;
    bool vsync = true;
    bool bench = false;
    bool screenshot = false;
    Theme start_theme = Theme::Race;
    AppMode start_mode = AppMode::Speedo;
    bool drag_armed = false;
    float drag_target = 400.0f;
    float drag_split = 1000.0f;
    uint32_t drag_countdown_ms = 3000;
    bool show_controls = false;
    PerfProfile perf_profile = PerfProfile::Balanced;
    HardwareProfile hardware{};
    int bench_frames = 600;
    int warmup = 150;
    float preview_speed = 78.0f;
    std::string shot_path;
};

class SimPlatform {
public:
    bool init(const SimOptions& options, MotoApp* app);
    int run();
    void shutdown();

private:
    static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
    static void read_cb(lv_indev_t* indev, lv_indev_data_t* data);
    static uint32_t tick_cb();

    void handle_events(bool& quit);
    void handle_key(int key, bool repeat);
    void present();
    void capture_screenshot();
    void print_benchmark();

    SimOptions options_;
    MotoApp* app_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    lv_display_t* display_ = nullptr;
    lv_indev_t* indev_ = nullptr;

    std::string bmp_path_;

    int mouse_x_ = 0;
    int mouse_y_ = 0;
    bool mouse_down_ = false;

    float fps_ = 60.0f;
    float frame_ms_ = 16.67f;
    float render_ms_ = 0.0f;
    float worst_ms_ = 0.0f;

    double bench_frame_sum_ = 0.0;
    double bench_render_sum_ = 0.0;
    double bench_frame_max_ = 0.0;
    double bench_render_max_ = 0.0;
    int bench_frames_ = 0;
    double bench_pixels_sum_ = 0.0;
    double bench_rects_sum_ = 0.0;
    int bench_draw_frames_ = 0;
    uint32_t bench_last_frame_ = UINT32_MAX;

    HardwareProfile hardware_{};
    HardwareEstimate hardware_estimate_{};
    uint64_t frame_pixels_ = 0;
    uint32_t frame_rects_ = 0;
    uint32_t frame_index_ = 0;
};

} // namespace moto
