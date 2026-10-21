#include <cstdio>
#include <cstring>
#include <string>

#include "app/moto_app.h"
#include "sim_platform.h"

namespace {

void print_usage() {
    std::printf(
        "MiniMotoNav - motorcycle dashboard simulator\n"
        "\n"
        "Usage:\n"
        "  MiniMotoNav [options]\n"
        "\n"
        "Options:\n"
        "  --mode=speedo|nav|drag|gps  Start on a display mode\n"
        "  --theme=race|day|night  Start with the racing, light, or dark palette\n"
        "  --controls          Open the on-screen control panel at startup\n"
        "  --scale=N           Window scale, 1..6 (default 3 => 960x720)\n"
        "  --novsync           Run uncapped instead of the paced 60 FPS loop\n"
        "  --bench[=N]         Measure N frames (default 600) and print timings\n"
        "  --profile=NAME      ESP32 model: conservative|balanced|optimistic|custom\n"
        "  --spi=HZ            Override modeled SPI clock (default 40000000)\n"
        "  --buffer=LINES      Override modeled LVGL buffer height (default 40)\n"
        "  --cpu=US_PER_PX     Override modeled CPU cost per flushed pixel\n"
        "  --jitter=RATIO      Override modeled frame jitter (0..0.5)\n"
        "  --dma               Model overlapped DMA with double buffering\n"
        "  --shot=FILE.bmp     Render a screenshot after the warm-up frames\n"
        "  --speed=N           Speed used for non-drag screenshots (default 78 km/h)\n"
        "  --drag-target=M     Drag distance in metres (default 400)\n"
        "  --drag-split=M      Split interval in metres, 0 disables (default 1000)\n"
        "  --drag-countdown=MS Countdown in milliseconds (default 3000)\n"
        "  --warmup=N          Frames rendered before a screenshot (default 150)\n"
        "  --no-fps            Hide the FPS counter\n"
        "  --help              Show this help\n"
        "\n"
        "Keys:\n"
        "  ESC quit        SPACE drag/run/pause   N display mode   T theme\n"
        "  C controls      F fps toggle      R reset        UP/DOWN speed\n"
        "  LEFT/RIGHT throttle               W/S rpm\n");
}

bool parse_int(const std::string& value, int& out) {
    try {
        size_t used = 0;
        int v = std::stoi(value, &used);
        if (used != value.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

int main(int argc, char** argv) {
    moto::SimOptions options;
    bool hide_fps = false;
    moto::PerfProfile profile = moto::PerfProfile::Balanced;
    uint32_t custom_spi = 0;
    int custom_buffer = 0;
    float custom_cpu = -1.0f;
    float custom_jitter = -1.0f;
    int custom_dma = -1;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else if (arg == "--bench") {
            options.bench = true;
        } else if (arg.rfind("--bench=", 0) == 0) {
            options.bench = true;
            int v = 0;
            if (parse_int(arg.substr(8), v) && v > 0) options.bench_frames = v;
        } else if (arg == "--mode=nav" || arg == "--nav") {
            options.start_mode = moto::AppMode::Nav;
        } else if (arg == "--mode=drag") {
            options.start_mode = moto::AppMode::Drag;
        } else if (arg == "--mode=gps" || arg == "--mode=clock") {
            options.start_mode = moto::AppMode::GpsClock;
        } else if (arg == "--mode=speedo") {
            options.start_mode = moto::AppMode::Speedo;
        } else if (arg.rfind("--drag-target=", 0) == 0) {
            try {
                options.drag_target = std::stof(arg.substr(14));
            } catch (...) {
            }
        } else if (arg.rfind("--drag-split=", 0) == 0) {
            try {
                options.drag_split = std::stof(arg.substr(13));
            } catch (...) {
            }
        } else if (arg.rfind("--drag-countdown=", 0) == 0) {
            int v = 0;
            if (parse_int(arg.substr(17), v) && v >= 0) options.drag_countdown_ms = (uint32_t)v;
        } else if (arg == "--theme=race" || arg == "--theme=racing") {
            options.start_theme = moto::Theme::Race;
        } else if (arg == "--theme=night" || arg == "--night") {
            options.start_theme = moto::Theme::Night;
        } else if (arg == "--theme=day") {
            options.start_theme = moto::Theme::Day;
        } else if (arg == "--controls") {
            options.show_controls = true;
        } else if (arg.rfind("--profile=", 0) == 0) {
            const std::string value = arg.substr(10);
            if (value == "conservative" || value == "slow") {
                profile = moto::PerfProfile::Conservative;
            } else if (value == "balanced" || value == "default") {
                profile = moto::PerfProfile::Balanced;
            } else if (value == "optimistic" || value == "fast") {
                profile = moto::PerfProfile::Optimistic;
            } else if (value == "custom") {
                profile = moto::PerfProfile::Custom;
            } else {
                std::fprintf(stderr, "Unknown profile: %s\n", value.c_str());
                return 2;
            }
        } else if (arg.rfind("--spi=", 0) == 0) {
            int v = 0;
            if (!parse_int(arg.substr(6), v) || v < 1000000 || v > 80000000) {
                std::fprintf(stderr, "Invalid --spi value\n");
                return 2;
            }
            custom_spi = (uint32_t)v;
        } else if (arg.rfind("--buffer=", 0) == 0) {
            int v = 0;
            if (!parse_int(arg.substr(9), v) || v < 8 || v > 120) {
                std::fprintf(stderr, "Invalid --buffer value\n");
                return 2;
            }
            custom_buffer = v;
        } else if (arg.rfind("--cpu=", 0) == 0) {
            try {
                custom_cpu = std::stof(arg.substr(6));
            } catch (...) {
                std::fprintf(stderr, "Invalid --cpu value\n");
                return 2;
            }
        } else if (arg.rfind("--jitter=", 0) == 0) {
            try {
                custom_jitter = std::stof(arg.substr(9));
            } catch (...) {
                std::fprintf(stderr, "Invalid --jitter value\n");
                return 2;
            }
        } else if (arg == "--dma") {
            custom_dma = 1;
        } else if (arg == "--novsync") {
            options.vsync = false;
        } else if (arg == "--no-fps") {
            hide_fps = true;
        } else if (arg.rfind("--scale=", 0) == 0) {
            int v = 0;
            if (parse_int(arg.substr(8), v) && v >= 1 && v <= 6) options.scale = v;
        } else if (arg.rfind("--warmup=", 0) == 0) {
            int v = 0;
            if (parse_int(arg.substr(9), v) && v > 0) options.warmup = v;
        } else if (arg.rfind("--speed=", 0) == 0) {
            try {
                options.preview_speed = std::stof(arg.substr(8));
            } catch (...) {
            }
        } else if (arg.rfind("--shot=", 0) == 0) {
            options.shot_path = arg.substr(7);
            options.screenshot = !options.shot_path.empty();
        } else if (arg == "--shot" && i + 1 < argc) {
            options.shot_path = argv[++i];
            options.screenshot = !options.shot_path.empty();
        } else if (arg.rfind("--frames=", 0) == 0) {
            int v = 0;
            if (parse_int(arg.substr(9), v) && v > 0) options.bench_frames = v;
        } else {
            std::fprintf(stderr, "Unknown option: %s\n\n", arg.c_str());
            print_usage();
            return 2;
        }
    }

    if (options.bench_frames < 60) options.bench_frames = 60;
    if (options.warmup < 1) options.warmup = 1;
    if (options.drag_target < 10.0f) options.drag_target = 10.0f;
    if (options.drag_target > 10000.0f) options.drag_target = 10000.0f;
    if (options.screenshot && options.start_mode == moto::AppMode::Drag) options.drag_armed = true;

    options.hardware = moto::make_profile(profile);
    bool overridden = profile == moto::PerfProfile::Custom;
    if (custom_spi != 0) {
        options.hardware.spi_hz = custom_spi;
        overridden = true;
    }
    if (custom_buffer != 0) {
        options.hardware.buffer_lines = (uint16_t)custom_buffer;
        overridden = true;
    }
    if (custom_cpu >= 0.0f) {
        options.hardware.cpu_us_per_pixel = custom_cpu;
        overridden = true;
    }
    if (custom_jitter >= 0.0f) {
        options.hardware.jitter_ratio = custom_jitter < 0.0f ? 0.0f : (custom_jitter > 0.5f ? 0.5f : custom_jitter);
        overridden = true;
    }
    if (custom_dma >= 0) {
        options.hardware.dma = custom_dma != 0;
        options.hardware.double_buffer = custom_dma != 0;
        overridden = true;
    }
    if (overridden) profile = moto::PerfProfile::Custom;
    options.perf_profile = profile;
    options.hardware.preset = profile;

    moto::MotoApp app;
    moto::SimPlatform platform;
    if (!platform.init(options, &app)) {
        platform.shutdown();
        return 1;
    }
    if (hide_fps) app.toggleFps();

    const int rc = platform.run();
    platform.shutdown();
    return rc;
}
