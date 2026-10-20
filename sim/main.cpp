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
        "  --mode=nav|speedo   Start on the navigation or speedometer screen\n"
        "  --theme=race|day|night  Start with the racing, light, or dark palette\n"
        "  --controls          Open the on-screen control panel at startup\n"
        "  --scale=N           Window scale, 1..6 (default 3 => 960x720)\n"
        "  --novsync           Run uncapped instead of the paced 60 FPS loop\n"
        "  --bench[=N]         Measure N frames (default 600) and print timings\n"
        "  --shot=FILE.bmp     Render a screenshot after the warm-up frames\n"
        "  --speed=N           Speed used for screenshots (default 78 km/h)\n"
        "  --warmup=N          Frames rendered before a screenshot (default 150)\n"
        "  --no-fps            Hide the FPS counter\n"
        "  --help              Show this help\n"
        "\n"
        "Keys:\n"
        "  ESC quit        SPACE run/pause   N nav/speedo   T theme\n"
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
            options.start_nav = true;
        } else if (arg == "--mode=speedo") {
            options.start_nav = false;
        } else if (arg == "--theme=race" || arg == "--theme=racing") {
            options.start_theme = moto::Theme::Race;
        } else if (arg == "--theme=night" || arg == "--night") {
            options.start_theme = moto::Theme::Night;
        } else if (arg == "--theme=day") {
            options.start_theme = moto::Theme::Day;
        } else if (arg == "--controls") {
            options.show_controls = true;
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
