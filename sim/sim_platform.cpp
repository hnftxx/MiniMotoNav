#include "sim_platform.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "app/moto_app.h"
#include "boot/boot_player.h"

namespace moto {
namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 240;
constexpr int kBytesPerPixel = 2;

uint64_t g_tick_start = 0;
uint64_t g_tick_freq = 1;

double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    size_t idx = (size_t)(p * (double)(values.size() - 1));
    if (idx >= values.size()) idx = values.size() - 1;
    return values[idx];
}

} // namespace

bool SimPlatform::init(const SimOptions& options, MotoApp* app) {
    options_ = options;
    app_ = app;
    hardware_ = options.hardware;
    if (hardware_.spi_hz == 0) hardware_ = make_profile(options_.perf_profile);
    hardware_.preset = options_.perf_profile;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    const int scale = std::max(1, options_.scale);
    window_ = SDL_CreateWindow("GODSPEED - motorcycle dashboard simulator",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kWidth * scale,
                               kHeight * scale, SDL_WINDOW_SHOWN);
    if (window_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    Uint32 renderer_flags = SDL_RENDERER_ACCELERATED;
    renderer_ = SDL_CreateRenderer(window_, -1, renderer_flags);
    if (renderer_ == nullptr) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_RenderSetLogicalSize(renderer_, kWidth, kHeight);

    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                                 kWidth, kHeight);
    if (texture_ == nullptr) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE);

    g_tick_freq = SDL_GetPerformanceFrequency();
    g_tick_start = SDL_GetPerformanceCounter();

    lv_init();
    lv_tick_set_cb(tick_cb);

    display_ = lv_display_create(kWidth, kHeight);
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display_, flush_cb);
    lv_display_set_user_data(display_, this);

    static std::vector<uint8_t> buf1;
    static std::vector<uint8_t> buf2;
    const size_t buf_bytes = (size_t)kWidth * 40 * kBytesPerPixel;
    buf1.resize(buf_bytes);
    buf2.resize(buf_bytes);
    lv_display_set_buffers(display_, buf1.data(), buf2.data(), (uint32_t)buf_bytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    indev_ = lv_indev_create();
    lv_indev_set_type(indev_, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_, read_cb);
    lv_indev_set_user_data(indev_, this);

    app_->init();
    app_->setTheme(options_.start_theme);
    app_->dragSession().setTargetDistance(options_.drag_target);
    app_->dragSession().setSplitInterval(options_.drag_split);
    app_->dragSession().setCountdownMs(options_.drag_countdown_ms);
    if (options_.start_mode != AppMode::Speedo) {
        app_->setMode(options_.start_mode, !options_.bench);
    }
    if (options_.show_controls) app_->toggleControls();
    if (options_.screenshot) {
        if (options_.start_mode == AppMode::Drag) {
            if (options_.drag_armed) app_->toggleDragRun();
        } else {
            app_->toggleAuto();
            app_->setManualSpeed(options_.preview_speed);
            app_->setManualThrottle(62.0f);
            app_->setManualRpm(5340.0f);
        }
    }

    return true;
}

int SimPlatform::run() {
    bool quit = false;
    if (options_.boot) {
        bool boot_quit = false;
        playBoot(boot_quit);
        if (boot_quit) return 0;
    }
    if (!options_.boot_shot_path.empty()) return 0;

    int frame_index = 0;
    int stat_skip = options_.bench ? 30 : 0;

    std::vector<double> frame_samples;
    std::vector<double> render_samples;
    frame_samples.reserve(options_.bench ? options_.bench_frames : 256);
    render_samples.reserve(options_.bench ? options_.bench_frames : 256);

    std::vector<double> worst_ring;
    worst_ring.reserve(120);

    uint64_t last = SDL_GetPerformanceCounter();
    const double fixed_ms = 1000.0 / 60.0;

    while (!quit) {
        handle_events(quit);
        if (quit) break;
        app_->pollCommands();

        const uint64_t now = SDL_GetPerformanceCounter();
        double real_ms = (double)(now - last) * 1000.0 / (double)g_tick_freq;
        last = now;
        if (real_ms <= 0.0 || real_ms > 250.0) real_ms = fixed_ms;

        double render_ms = 0.0;

        frame_ms_ += (float)((real_ms - frame_ms_) * 0.12);
        fps_ = 1000.0f / std::max(1.0f, frame_ms_);
        app_->setPerf(fps_, frame_ms_, render_ms_, worst_ms_);

        float dt = (options_.screenshot || options_.bench) ? (float)(fixed_ms / 1000.0)
                                                           : (float)(real_ms / 1000.0);
        if (dt > 0.1f) dt = 0.1f;

        app_->update(dt);

        const uint64_t r0 = SDL_GetPerformanceCounter();
        lv_timer_handler();
        if (options_.bench) lv_refr_now(display_);
        const uint64_t r1 = SDL_GetPerformanceCounter();
        render_ms = (double)(r1 - r0) * 1000.0 / (double)g_tick_freq;
        render_ms_ += (float)((render_ms - render_ms_) * 0.12);

        worst_ring.push_back(real_ms);
        if (worst_ring.size() > 120) worst_ring.erase(worst_ring.begin());
        worst_ms_ = (float)*std::max_element(worst_ring.begin(), worst_ring.end());
        app_->setPerf(fps_, frame_ms_, render_ms_, worst_ms_);

        if (options_.bench && frame_index_ > (uint32_t)stat_skip &&
            frame_index_ != bench_last_frame_) {
            bench_last_frame_ = frame_index_;
            if (hardware_estimate_.pixels > 0) {
                bench_pixels_sum_ += hardware_estimate_.pixels;
                bench_rects_sum_ += hardware_estimate_.rects;
                bench_draw_frames_++;
            }
        }

        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);

        const bool take_shot = options_.screenshot && frame_index >= options_.warmup;
        if (take_shot) capture_screenshot();

        SDL_RenderPresent(renderer_);

        ++frame_index;
        if (!options_.screenshot && frame_index > stat_skip) {
            frame_samples.push_back(real_ms);
            render_samples.push_back(render_ms);
        }

        if (take_shot) break;
        if (options_.bench && frame_index >= options_.bench_frames) break;

        if (!options_.bench && options_.vsync) {
            const uint64_t after = SDL_GetPerformanceCounter();
            double elapsed = (double)(after - now) * 1000.0 / (double)g_tick_freq;
            if (elapsed < fixed_ms - 1.5) {
                SDL_Delay((Uint32)std::floor(fixed_ms - elapsed - 1.0));
            }
            while (elapsed < fixed_ms - 0.2) {
                const uint64_t spin = SDL_GetPerformanceCounter();
                elapsed = (double)(spin - now) * 1000.0 / (double)g_tick_freq;
                if (elapsed < fixed_ms - 1.2) SDL_Delay(0);
            }
        }
    }

    if (options_.bench) {
        bench_frame_sum_ = 0.0;
        bench_render_sum_ = 0.0;
        for (double v : frame_samples) bench_frame_sum_ += v;
        for (double v : render_samples) bench_render_sum_ += v;
        bench_frames_ = (int)frame_samples.size();
        if (bench_frames_ > 0) {
            bench_frame_sum_ /= bench_frames_;
            bench_render_sum_ /= bench_frames_;
        }
        bench_frame_max_ = percentile(frame_samples, 1.0);
        bench_render_max_ = percentile(render_samples, 1.0);
        print_benchmark();
        std::printf("[bench] frame p95=%.2f ms  render p95=%.2f ms\n",
                    percentile(frame_samples, 0.95), percentile(render_samples, 0.95));
    }

    return 0;
}

void SimPlatform::handle_events(bool& quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            quit = true;
            break;
        case SDL_MOUSEMOTION:
            mouse_x_ = e.motion.x;
            mouse_y_ = e.motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button == SDL_BUTTON_LEFT) {
                mouse_down_ = true;
                mouse_x_ = e.button.x;
                mouse_y_ = e.button.y;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (e.button.button == SDL_BUTTON_LEFT) {
                mouse_down_ = false;
                mouse_x_ = e.button.x;
                mouse_y_ = e.button.y;
            }
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_LEAVE) mouse_down_ = false;
            break;
        case SDL_KEYDOWN:
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                quit = true;
            } else {
                handle_key(e.key.keysym.sym, e.key.repeat != 0);
            }
            break;
        default:
            break;
        }
    }
}

void SimPlatform::handle_key(int key, bool repeat) {
    switch (key) {
    case SDLK_SPACE:
        if (!repeat) app_->togglePause();
        break;
    case SDLK_n:
        if (!repeat) app_->toggleMode();
        break;
    case SDLK_t:
        if (!repeat) app_->toggleTheme();
        break;
    case SDLK_c:
        if (!repeat) app_->toggleControls();
        break;
    case SDLK_f:
        if (!repeat) app_->toggleFps();
        break;
    case SDLK_r:
        if (!repeat) app_->resetSim();
        break;
    case SDLK_UP:
        app_->adjustSpeed(5.0f);
        break;
    case SDLK_DOWN:
        app_->adjustSpeed(-5.0f);
        break;
    case SDLK_RIGHT:
        app_->adjustThrottle(5.0f);
        break;
    case SDLK_LEFT:
        app_->adjustThrottle(-5.0f);
        break;
    case SDLK_w:
        app_->adjustRpm(250.0f);
        break;
    case SDLK_s:
        app_->adjustRpm(-250.0f);
        break;
    default:
        break;
    }
}

void SimPlatform::present() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

bool SimPlatform::playBoot(bool& quit) {
    std::vector<uint16_t> pixels((size_t)boot::kWidth * boot::kHeight);
    boot::Player player;
    if (!player.begin(pixels.data(), (uint32_t)pixels.size())) {
        std::fprintf(stderr, "[boot] animation unavailable\n");
        return false;
    }
    if (!player.reset()) {
        std::fprintf(stderr, "[boot] animation decode failed\n");
        return false;
    }

    drawBoot(pixels.data());
    std::printf("[boot] frames=%u ticks=%u\n", (unsigned)player.frameCount(),
                (unsigned)player.tickCount());

    if (!options_.boot_shot_path.empty() && player.frameIndex() >= options_.boot_frame) {
        saveScreenshot(options_.boot_shot_path);
        return true;
    }
    SDL_RenderPresent(renderer_);

    uint64_t last = SDL_GetPerformanceCounter();
    while (!player.finished()) {
        handle_events(quit);
        if (quit) return false;

        const uint64_t now = SDL_GetPerformanceCounter();
        float dt = (float)((double)(now - last) / (double)g_tick_freq);
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        if (player.update(dt)) {
            drawBoot(pixels.data());
            if (!options_.boot_shot_path.empty() &&
                player.frameIndex() >= options_.boot_frame) {
                saveScreenshot(options_.boot_shot_path);
                return true;
            }
            SDL_RenderPresent(renderer_);
        }
        SDL_Delay(1);
    }

    if (!options_.boot_shot_path.empty()) {
        drawBoot(pixels.data());
        saveScreenshot(options_.boot_shot_path);
    }
    return true;
}

void SimPlatform::drawBoot(const uint16_t* pixels) {
    if (texture_ == nullptr || pixels == nullptr) return;
    SDL_UpdateTexture(texture_, nullptr, pixels, boot::kWidth * kBytesPerPixel);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
}

void SimPlatform::saveScreenshot(const std::string& requested_path) {
    if (requested_path.empty()) return;

    int w = 0;
    int h = 0;
    SDL_GetRendererOutputSize(renderer_, &w, &h);
    if (w <= 0 || h <= 0) return;

    std::vector<uint32_t> pixels((size_t)w * h);
    if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(), w * 4) !=
        0) {
        std::fprintf(stderr, "SDL_RenderReadPixels failed: %s\n", SDL_GetError());
        return;
    }

    std::string path = requested_path;
    if (path.size() < 4 || path.compare(path.size() - 4, 4, ".bmp") != 0) path += ".bmp";

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr) return;
    if (SDL_SaveBMP(surface, path.c_str()) != 0) {
        std::fprintf(stderr, "SDL_SaveBMP failed: %s\n", SDL_GetError());
    } else {
        std::printf("[shot] saved %s (%dx%d)\n", path.c_str(), w, h);
    }
    SDL_FreeSurface(surface);
}

void SimPlatform::capture_screenshot() {
    saveScreenshot(options_.shot_path);
}

void SimPlatform::print_benchmark() {
    if (bench_frames_ <= 0) {
        std::printf("[bench] no frames captured\n");
        return;
    }
    std::printf("[bench] frames=%d avg_frame=%.3f ms avg_render=%.3f ms max_frame=%.3f ms "
                "max_render=%.3f ms fps=%.1f\n",
                bench_frames_, bench_frame_sum_, bench_render_sum_, bench_frame_max_,
                bench_render_max_, bench_frame_sum_ > 0.0 ? 1000.0 / bench_frame_sum_ : 0.0);

    const int draw_frames = bench_draw_frames_ > 0 ? bench_draw_frames_ : 1;
    FrameWork avg;
    avg.pixels = (uint32_t)std::lround(bench_pixels_sum_ / (double)draw_frames);
    avg.rects = (uint16_t)std::lround(bench_rects_sum_ / (double)draw_frames);
    std::printf("[bench-hw] dirty=%u px/%.1f rects  drawn=%d/%d frames  spi=%.1f MHz  "
                "buffer=%u lines  measured=%s\n",
                avg.pixels, (double)avg.rects, bench_draw_frames_, bench_frames_,
                hardware_.spi_hz / 1000000.0, hardware_.buffer_lines,
                hardware_.dma ? "dma" : "blocking");

    const PerfProfile presets[] = {PerfProfile::Conservative, PerfProfile::Balanced,
                                   PerfProfile::Optimistic};
    for (PerfProfile preset : presets) {
        const HardwareProfile profile = make_profile(preset);
        const HardwareEstimate estimate = estimate_frame(profile, avg);
        std::printf("[bench-hw] %-12s %.1f MHz  frame=%.2f ms  fps=%.1f  spi=%.2f ms  "
                    "cpu=%.2f ms  late=%.1f%%\n",
                    profile_name(preset), profile.spi_hz / 1000000.0, estimate.frame_ms,
                    estimate.fps, estimate.spi_ms, estimate.cpu_ms, estimate.drop_pct);
    }
    const HardwareEstimate current = estimate_frame(hardware_, avg);
    std::printf("[bench-hw] %-12s %.1f MHz  frame=%.2f ms  fps=%.1f  spi=%.2f ms  "
                "cpu=%.2f ms  late=%.1f%%\n",
                profile_name(options_.perf_profile), hardware_.spi_hz / 1000000.0,
                current.frame_ms, current.fps, current.spi_ms, current.cpu_ms, current.drop_pct);
}

void SimPlatform::shutdown() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    texture_ = nullptr;
    renderer_ = nullptr;
    window_ = nullptr;
    SDL_Quit();
}

void SimPlatform::flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    auto* self = static_cast<SimPlatform*>(lv_display_get_user_data(disp));
    const int w = area->x2 - area->x1 + 1;
    const int h = area->y2 - area->y1 + 1;
    if (self && self->texture_ && w > 0 && h > 0) {
        SDL_Rect r;
        r.x = area->x1;
        r.y = area->y1;
        r.w = w;
        r.h = h;
        SDL_UpdateTexture(self->texture_, &r, px_map, w * kBytesPerPixel);
        self->frame_pixels_ += (uint64_t)w * (uint64_t)h;
        self->frame_rects_++;
    }
    if (self && lv_display_flush_is_last(disp)) {
        FrameWork work;
        work.pixels = (uint32_t)std::min<uint64_t>(self->frame_pixels_, 0xFFFFFFFFu);
        work.rects = (uint16_t)std::min<uint32_t>(self->frame_rects_, 65535u);
        self->hardware_estimate_ = estimate_frame(self->hardware_, work, self->frame_index_);
        if (self->app_) self->app_->setHardwareEstimate(self->hardware_estimate_);
        self->frame_pixels_ = 0;
        self->frame_rects_ = 0;
        self->frame_index_++;
    }
    lv_display_flush_ready(disp);
}

void SimPlatform::read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* self = static_cast<SimPlatform*>(lv_indev_get_user_data(indev));
    if (self == nullptr) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    data->point.x = (int16_t)std::max(0, std::min(kWidth - 1, self->mouse_x_));
    data->point.y = (int16_t)std::max(0, std::min(kHeight - 1, self->mouse_y_));
    data->state = self->mouse_down_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

uint32_t SimPlatform::tick_cb() {
    const uint64_t now = SDL_GetPerformanceCounter();
    return (uint32_t)((now - g_tick_start) * 1000ULL / std::max<uint64_t>(1, g_tick_freq));
}

} // namespace moto
