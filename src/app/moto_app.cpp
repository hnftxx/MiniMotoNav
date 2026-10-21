#include "app/moto_app.h"

#include <cmath>

namespace moto {

void MotoApp::init() {
    speedo_screen_ = lv_obj_create(nullptr);
    nav_screen_ = lv_obj_create(nullptr);
    drag_screen_ = lv_obj_create(nullptr);
    gps_screen_ = lv_obj_create(nullptr);
    lv_obj_remove_style_all(speedo_screen_);
    lv_obj_remove_style_all(nav_screen_);
    lv_obj_remove_style_all(drag_screen_);
    lv_obj_remove_style_all(gps_screen_);
    lv_obj_set_style_bg_color(speedo_screen_, racing_palette().bg_bottom, 0);
    lv_obj_set_style_bg_opa(speedo_screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(nav_screen_, racing_palette().map_land, 0);
    lv_obj_set_style_bg_opa(nav_screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(drag_screen_, racing_palette().bg_bottom, 0);
    lv_obj_set_style_bg_opa(drag_screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(gps_screen_, racing_palette().bg_bottom, 0);
    lv_obj_set_style_bg_opa(gps_screen_, LV_OPA_COVER, 0);

    speedo_.create(speedo_screen_);
    nav_.create(nav_screen_);
    drag_view_.create(drag_screen_);
    gps_view_.create(gps_screen_);

    lv_screen_load(speedo_screen_);
    controls_.create(this);
    controls_.setVisible(false);

    sim_.reset();
    sim_.setRunning(true);
    sim_.setAutoDrive(true);
    clock_ = Clock{};
    perf_ = PerfStats{};
    perf_.visible = true;
    theme_ = Theme::Race;
    theme_from_ = racing_palette();
    theme_t_ = 1.0f;
    mode_ = AppMode::Speedo;
    now_us_ = 0;
    clock_accum_s_ = 0.0f;
    applyPalette();
    initialized_ = true;
}

void MotoApp::setPerf(float fps, float frame_ms, float render_ms, float worst_ms) {
    perf_.fps = fps;
    perf_.frame_ms = frame_ms;
    perf_.render_ms = render_ms;
    perf_.worst_ms = worst_ms;
}

void MotoApp::setHardwareEstimate(const HardwareEstimate& estimate) {
    perf_.hw_valid = estimate.valid;
    perf_.hw_fps = estimate.fps;
    perf_.hw_frame_ms = estimate.frame_ms;
    perf_.hw_spi_ms = estimate.spi_ms;
    perf_.hw_cpu_ms = estimate.cpu_ms;
    perf_.hw_jitter_ms = estimate.jitter_ms;
    perf_.hw_drop_pct = estimate.drop_pct;
    perf_.dirty_pixels = estimate.pixels;
    perf_.dirty_rects = estimate.rects;
    perf_.spi_hz = estimate.spi_hz;
    perf_.hw_dma = estimate.dma;
    perf_.hw_measured = false;
}

void MotoApp::setHardwareMeasured(float fps, float frame_ms, uint32_t spi_hz, float spi_ms,
                                 uint32_t pixels, uint16_t rects) {
    if (fps <= 0.0f) return;
    perf_.hw_valid = true;
    perf_.hw_measured = true;
    perf_.hw_fps = fps;
    perf_.hw_frame_ms = frame_ms;
    perf_.hw_spi_ms = spi_ms;
    perf_.hw_cpu_ms = 0.0f;
    perf_.hw_jitter_ms = 0.0f;
    perf_.hw_drop_pct = 0.0f;
    perf_.dirty_pixels = pixels;
    perf_.dirty_rects = rects;
    perf_.spi_hz = spi_hz;
}

void MotoApp::postCommand(const Command& command) { commands_.post(command); }

void MotoApp::handleCommand(const Command& command) {
    switch (command.type) {
    case CommandType::NextMode: cycleMode(); break;
    case CommandType::SetMode: setMode(command.mode); break;
    case CommandType::NextTheme: toggleTheme(); break;
    case CommandType::SetTheme:
        if (command.theme == 1) {
            setTheme(Theme::Day);
        } else if (command.theme == 2) {
            setTheme(Theme::Night);
        } else {
            setTheme(Theme::Race);
        }
        break;
    case CommandType::TogglePause: togglePause(); break;
    case CommandType::ToggleAuto: toggleAuto(); break;
    case CommandType::Reset: resetSim(); break;
    case CommandType::ToggleControls: toggleControls(); break;
    case CommandType::ToggleFps: toggleFps(); break;
    case CommandType::AdjustSpeed: setManualSpeed(command.value); break;
    case CommandType::AdjustThrottle: setManualThrottle(command.value); break;
    case CommandType::AdjustRpm: setManualRpm(command.value); break;
    case CommandType::DragRun: toggleDragRun(); break;
    case CommandType::DragReset: resetDrag(); break;
    case CommandType::SetDragTarget: drag_.setTargetDistance(command.value); break;
    case CommandType::SetDragSplit: drag_.setSplitInterval(command.value); break;
    case CommandType::SetDragCountdown:
        drag_.setCountdownMs((uint32_t)std::fmax(0.0f, command.value));
        break;
    case CommandType::Invalid:
    default: break;
    }
}

void MotoApp::pollCommands() {
    Command command;
    while (commands_.pop(command)) handleCommand(command);
}

void MotoApp::update(float dt) {
    if (!initialized_) return;
    if (dt > 0.1f) dt = 0.1f;

    now_us_ += (uint64_t)std::llround(dt * 1000000.0f);
    if (!feed_active_) sim_.tick(dt);

    const Telemetry& data = telemetry();
    const bool running = feed_active_ ? (feed_.state().has_telemetry && !feed_.state().stale)
                                      : sim_.running();

    if (mode_ == AppMode::Drag) {
        drag_.tick(now_us_, data.speed_kph, data.gps_fix);
        if (!feed_active_) sim_.setRunning(drag_.phase() == DragPhase::Running);
    }

    if (running) {
        clock_accum_s_ += dt;
        while (clock_accum_s_ >= 60.0f) {
            clock_accum_s_ -= 60.0f;
            clock_.addMinutes(1);
        }
    }

    if (theme_t_ < 1.0f) {
        theme_t_ += (1.0f - theme_t_) * (1.0f - std::exp(-dt / 0.14f));
        if (1.0f - theme_t_ < 0.002f) theme_t_ = 1.0f;
        palette_ = mix_palette(theme_from_, palette_for_theme(theme_), theme_t_);
    } else {
        palette_ = palette_for_theme(theme_);
    }
    speedo_.setPalette(palette_);
    nav_.setPalette(palette_);
    drag_view_.setPalette(palette_);
    gps_view_.setPalette(palette_);

    switch (mode_) {
    case AppMode::Speedo: speedo_.update(data, perf_, clock_, dt); break;
    case AppMode::Nav: nav_.update(data, perf_, clock_, dt); break;
    case AppMode::Drag: drag_view_.update(drag_, data, perf_, clock_, dt); break;
    case AppMode::GpsClock: gps_view_.update(data, perf_, clock_, dt); break;
    }
    if (theme_t_ < 1.0f) {
        lv_obj_t* root = nullptr;
        switch (mode_) {
        case AppMode::Speedo: root = speedo_.root(); break;
        case AppMode::Nav: root = nav_.root(); break;
        case AppMode::Drag: root = drag_view_.root(); break;
        case AppMode::GpsClock: root = gps_view_.root(); break;
        }
        if (root) lv_obj_invalidate(root);
    }
    controls_.refresh(data, theme_, mode_, drag_);
}

void MotoApp::applyBleTelemetry(const ble::TelemetryFields& fields, uint32_t now_ms) {
    feed_active_ = true;
    feed_.applyTelemetry(fields, now_ms);
}

void MotoApp::applyBleNavigation(const ble::NavigationFields& navigation, uint32_t now_ms) {
    feed_active_ = true;
    feed_.applyNavigation(navigation, now_ms);
}

void MotoApp::setBleLinked(bool linked, uint32_t now_ms) {
    if (!feed_active_) return;
    feed_.setLinked(linked, now_ms);
}

void MotoApp::tickBle(uint32_t now_ms) {
    if (!feed_active_) return;
    feed_.tick(now_ms);
}

void MotoApp::clearBleFeed() {
    feed_ = DisplayFeed{};
    feed_active_ = false;
    sim_.setRunning(true);
}

void MotoApp::applyPalette() {
    palette_ = palette_for_theme(theme_);
    theme_from_ = palette_;
    theme_t_ = 1.0f;
    speedo_.setPalette(palette_);
    nav_.setPalette(palette_);
    drag_view_.setPalette(palette_);
    gps_view_.setPalette(palette_);
    if (speedo_screen_) lv_obj_set_style_bg_color(speedo_screen_, palette_.bg_bottom, 0);
    if (nav_screen_) lv_obj_set_style_bg_color(nav_screen_, palette_.map_land, 0);
    if (drag_screen_) lv_obj_set_style_bg_color(drag_screen_, palette_.bg_bottom, 0);
    if (gps_screen_) lv_obj_set_style_bg_color(gps_screen_, palette_.bg_bottom, 0);
}

void MotoApp::togglePause() {
    if (mode_ == AppMode::Drag) {
        toggleDragRun();
        return;
    }
    sim_.setRunning(!sim_.running());
}

void MotoApp::toggleAuto() {
    if (mode_ == AppMode::Drag) return;
    sim_.setAutoDrive(!sim_.autoDrive());
}

void MotoApp::resetSim() {
    if (mode_ == AppMode::Drag) {
        resetDrag();
        return;
    }
    sim_.reset();
    sim_.setRunning(true);
    sim_.setAutoDrive(true);
    clock_ = Clock{};
    clock_accum_s_ = 0.0f;
    nav_.reset();
    applyPalette();
}

void MotoApp::toggleTheme() {
    setTheme(next_theme(theme_));
}

void MotoApp::setTheme(Theme theme) {
    if (theme == theme_) return;
    theme_from_ = palette_;
    theme_ = theme;
    theme_t_ = 0.0f;
    const Palette target = palette_for_theme(theme_);
    if (speedo_screen_) lv_obj_set_style_bg_color(speedo_screen_, target.bg_bottom, 0);
    if (nav_screen_) lv_obj_set_style_bg_color(nav_screen_, target.map_land, 0);
    if (drag_screen_) lv_obj_set_style_bg_color(drag_screen_, target.bg_bottom, 0);
    if (gps_screen_) lv_obj_set_style_bg_color(gps_screen_, target.bg_bottom, 0);
}

void MotoApp::toggleMode() { cycleMode(); }

void MotoApp::cycleMode() {
    switch (mode_) {
    case AppMode::Speedo: setMode(AppMode::Nav); break;
    case AppMode::Nav: setMode(AppMode::Drag); break;
    case AppMode::Drag: setMode(AppMode::GpsClock); break;
    case AppMode::GpsClock:
    default: setMode(AppMode::Speedo); break;
    }
}

void MotoApp::setMode(AppMode mode, bool animate) {
    if (mode == mode_) return;
    const bool leaving_drag = mode_ == AppMode::Drag;
    mode_ = mode;

    if (leaving_drag) {
        drag_.cancel();
        sim_.setDragMode(false);
        sim_.setAutoDrive(true);
        sim_.setRunning(true);
    }
    if (mode_ == AppMode::Drag) {
        sim_.setDragMode(true);
        sim_.beginDragRun();
        sim_.setRunning(false);
        drag_.cancel();
    }

    loadScreen(mode_, animate);
}

void MotoApp::toggleDragRun() {
    if (mode_ != AppMode::Drag) setMode(AppMode::Drag);
    const uint64_t now = now_us_;
    switch (drag_.phase()) {
    case DragPhase::Idle:
    case DragPhase::Finished:
        sim_.beginDragRun();
        sim_.setRunning(false);
        drag_.arm(now);
        break;
    case DragPhase::Countdown:
        drag_.cancel();
        sim_.setRunning(false);
        break;
    case DragPhase::Running:
        drag_.pause(now);
        sim_.setRunning(false);
        break;
    case DragPhase::Paused:
        drag_.resume(now);
        sim_.setRunning(true);
        break;
    }
}

void MotoApp::resetDrag() {
    drag_.cancel();
    sim_.beginDragRun();
    sim_.setRunning(false);
}

void MotoApp::toggleControls() {
    controls_.setVisible(!controls_.visible());
}

void MotoApp::setControlsVisible(bool visible) {
    controls_.setVisible(visible);
}

void MotoApp::toggleFps() {
    perf_.visible = !perf_.visible;
}

void MotoApp::loadScreen(AppMode mode, bool animate) {
    lv_obj_t* target = speedo_screen_;
    switch (mode) {
    case AppMode::Nav: target = nav_screen_; break;
    case AppMode::Drag: target = drag_screen_; break;
    case AppMode::GpsClock: target = gps_screen_; break;
    case AppMode::Speedo:
    default: target = speedo_screen_; break;
    }
    if (target == nullptr) return;
    if (!animate) {
        lv_screen_load(target);
        return;
    }
    const bool backward = mode == AppMode::Speedo;
    lv_screen_load_anim(target, backward ? LV_SCR_LOAD_ANIM_MOVE_RIGHT : LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        280, 0, false);
}

void MotoApp::adjustSpeed(float delta) {
    const float next = sim_.data().speed_kph + delta;
    sim_.setManualSpeed(std::fmax(0.0f, std::fmin(160.0f, next)));
}

void MotoApp::adjustThrottle(float delta) {
    const float next = sim_.data().throttle + delta;
    sim_.setManualThrottle(std::fmax(0.0f, std::fmin(100.0f, next)));
}

void MotoApp::adjustRpm(float delta) {
    const float next = sim_.data().rpm + delta;
    sim_.setManualRpm(std::fmax(800.0f, std::fmin(9000.0f, next)));
}

void MotoApp::setManualSpeed(float kph) { sim_.setManualSpeed(kph); }
void MotoApp::setManualThrottle(float percent) { sim_.setManualThrottle(percent); }
void MotoApp::setManualRpm(float rpm) { sim_.setManualRpm(rpm); }

} // namespace moto
