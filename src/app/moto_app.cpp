#include "app/moto_app.h"

#include <cmath>

namespace moto {

void MotoApp::init() {
    speedo_screen_ = lv_obj_create(nullptr);
    nav_screen_ = lv_obj_create(nullptr);
    lv_obj_remove_style_all(speedo_screen_);
    lv_obj_remove_style_all(nav_screen_);
    lv_obj_set_style_bg_color(speedo_screen_, racing_palette().bg_bottom, 0);
    lv_obj_set_style_bg_opa(speedo_screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(nav_screen_, racing_palette().map_land, 0);
    lv_obj_set_style_bg_opa(nav_screen_, LV_OPA_COVER, 0);

    speedo_.create(speedo_screen_);
    nav_.create(nav_screen_);

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
    nav_mode_ = false;
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

void MotoApp::update(float dt) {
    if (!initialized_) return;
    if (dt > 0.1f) dt = 0.1f;

    sim_.tick(dt);

    if (sim_.running()) {
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

    speedo_.update(sim_.data(), perf_, clock_, dt);
    nav_.update(sim_.data(), perf_, clock_, dt);
    controls_.refresh(sim_.data(), theme_, nav_mode_);

    if (theme_t_ == 1.0f && speedo_screen_ && nav_screen_) {
        lv_obj_set_style_bg_color(speedo_screen_, palette_.bg_bottom, 0);
        lv_obj_set_style_bg_color(nav_screen_, palette_.map_land, 0);
    }
}

void MotoApp::applyPalette() {
    palette_ = palette_for_theme(theme_);
    theme_from_ = palette_;
    theme_t_ = 1.0f;
    speedo_.setPalette(palette_);
    nav_.setPalette(palette_);
    if (speedo_screen_) lv_obj_set_style_bg_color(speedo_screen_, palette_.bg_bottom, 0);
    if (nav_screen_) lv_obj_set_style_bg_color(nav_screen_, palette_.map_land, 0);
}

void MotoApp::togglePause() {
    sim_.setRunning(!sim_.running());
}

void MotoApp::toggleAuto() {
    sim_.setAutoDrive(!sim_.autoDrive());
}

void MotoApp::resetSim() {
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
}

void MotoApp::toggleMode() {
    nav_mode_ = !nav_mode_;
    loadScreen(nav_mode_, true);
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

void MotoApp::loadScreen(bool nav, bool animate) {
    lv_obj_t* target = nav ? nav_screen_ : speedo_screen_;
    if (target == nullptr) return;
    if (!animate) {
        lv_screen_load(target);
        return;
    }
    lv_screen_load_anim(target, nav ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
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
