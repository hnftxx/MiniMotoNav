#include "ui/ui_controls.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "app/moto_app.h"
#include "ui/fonts/fonts.h"

namespace moto {
namespace {

lv_color_t kPanelEdge = lv_color_hex(0x1D3B54);
lv_color_t kText = lv_color_hex(0xEAF8FF);
lv_color_t kMuted = lv_color_hex(0x86A9BF);
lv_color_t kAccent = lv_color_hex(0x2F86FF);

} // namespace

void UiControls::create(MotoApp* app) {
    app_ = app;
    panel_ = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(panel_);
    lv_obj_set_size(panel_, 304, 224);
    lv_obj_set_pos(panel_, 8, 8);
    lv_obj_set_style_bg_color(panel_, lv_color_hex(0x061018), 0);
    lv_obj_set_style_bg_opa(panel_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel_, 16, 0);
    lv_obj_set_style_border_width(panel_, 1, 0);
    lv_obj_set_style_border_color(panel_, kPanelEdge, 0);
    lv_obj_set_style_border_opa(panel_, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(panel_, 26, 0);
    lv_obj_set_style_shadow_opa(panel_, 90, 0);
    lv_obj_set_style_shadow_color(panel_, lv_color_black(), 0);
    lv_obj_set_style_shadow_offset_y(panel_, 8, 0);
    lv_obj_remove_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(panel_);
    lv_obj_set_style_text_font(title, &font_ui_12, 0);
    lv_obj_set_style_text_color(title, kText, 0);
    lv_label_set_text(title, "SIM CONTROLS");
    lv_obj_set_pos(title, 10, 7);

    lv_obj_t* sub = lv_label_create(panel_);
    lv_obj_set_style_text_font(sub, &font_ui_10, 0);
    lv_obj_set_style_text_color(sub, kMuted, 0);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(sub, 106);
    lv_label_set_text(sub, "SIM TELEMETRY");
    lv_obj_set_pos(sub, 158, 9);

    lv_obj_t* close = lv_button_create(panel_);
    lv_obj_remove_style_all(close);
    lv_obj_set_size(close, 22, 22);
    lv_obj_set_pos(close, 272, 4);
    lv_obj_set_style_bg_color(close, kPanelEdge, 0);
    lv_obj_set_style_bg_opa(close, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(close, 7, 0);
    lv_obj_add_event_cb(close, close_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* close_lbl = lv_label_create(close);
    lv_obj_set_style_text_font(close_lbl, &font_ui_12, 0);
    lv_obj_set_style_text_color(close_lbl, kMuted, 0);
    lv_label_set_text(close_lbl, "X");
    lv_obj_center(close_lbl);

    auto row_label = [this](const char* text, int y) {
        lv_obj_t* l = lv_label_create(panel_);
        lv_obj_set_style_text_font(l, &font_ui_10, 0);
        lv_obj_set_style_text_color(l, kMuted, 0);
        lv_label_set_text(l, text);
        lv_obj_set_pos(l, 10, y + 1);
    };
    row_label("SPEED", 31);
    row_label("THROTTLE", 57);
    row_label("RPM", 83);

    speed_slider_ = makeSlider(panel_, 72, 30, 164, 0, 0, 160, 78);
    throttle_slider_ = makeSlider(panel_, 72, 56, 164, 1, 0, 100, 62);
    rpm_slider_ = makeSlider(panel_, 72, 82, 164, 2, 800, 9000, 5340);

    auto make_value = [this](int x, int y) {
        lv_obj_t* l = lv_label_create(panel_);
        lv_obj_set_style_text_font(l, &font_ui_10, 0);
        lv_obj_set_style_text_color(l, kText, 0);
        lv_obj_set_width(l, 54);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(l, "0");
        lv_obj_set_pos(l, x, y + 1);
        return l;
    };
    speed_value_ = make_value(242, 31);
    throttle_value_ = make_value(242, 57);
    rpm_value_ = make_value(242, 83);

    lv_obj_t* run_btn = makeButton(panel_, "PAUSE", 10, 102, 88, 26, 0);
    run_label_ = lv_obj_get_child(run_btn, 0);
    lv_obj_t* auto_btn = makeButton(panel_, "AUTO ON", 106, 102, 88, 26, 1);
    auto_label_ = lv_obj_get_child(auto_btn, 0);
    makeButton(panel_, "RESET", 202, 102, 88, 26, 2);

    lv_obj_t* mode_btn = makeButton(panel_, "NAV", 10, 134, 88, 26, 3);
    mode_label_ = lv_obj_get_child(mode_btn, 0);
    lv_obj_t* theme_btn = makeButton(panel_, "DAY", 106, 134, 88, 26, 4);
    theme_label_ = lv_obj_get_child(theme_btn, 0);
    lv_obj_t* fps_btn = makeButton(panel_, "FPS ON", 202, 134, 88, 26, 5);
    fps_label_ = lv_obj_get_child(fps_btn, 0);

    lv_obj_t* hint0 = lv_label_create(panel_);
    lv_obj_set_style_text_font(hint0, &font_ui_10, 0);
    lv_obj_set_style_text_color(hint0, kAccent, 0);
    lv_label_set_text(hint0, "Drag a slider to switch to manual mode");
    lv_obj_set_pos(hint0, 10, 170);

    lv_obj_t* hint1 = lv_label_create(panel_);
    lv_obj_set_style_text_font(hint1, &font_ui_10, 0);
    lv_obj_set_style_text_color(hint1, kMuted, 0);
    lv_label_set_text(hint1, "SPACE run   N nav   T theme   C close");
    lv_obj_set_pos(hint1, 10, 186);

    lv_obj_t* hint2 = lv_label_create(panel_);
    lv_obj_set_style_text_font(hint2, &font_ui_10, 0);
    lv_obj_set_style_text_color(hint2, kMuted, 0);
    lv_label_set_text(hint2, "UP/DOWN speed   LEFT/RIGHT throttle   W/S rpm");
    lv_obj_set_pos(hint2, 10, 200);

    setVisible(false);
}

void UiControls::setVisible(bool visible) {
    if (!panel_) return;
    if (visible) lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
}

bool UiControls::visible() const {
    return panel_ && !lv_obj_has_flag(panel_, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* UiControls::makeSlider(lv_obj_t* parent, int x, int y, int w, int id, int min, int max,
                                 int value) {
    lv_obj_t* slider = lv_slider_create(parent);
    lv_obj_set_size(slider, w, 8);
    lv_obj_set_pos(slider, x, y);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, kPanelEdge, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, kAccent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, slider_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_set_user_data(slider, (void*)(intptr_t)id);
    return slider;
}

lv_obj_t* UiControls::makeButton(lv_obj_t* parent, const char* text, int x, int y, int w, int h,
                                 int id) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, kPanelEdge, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 9, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, kPanelEdge, 0);
    lv_obj_add_event_cb(btn, button_cb, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(btn, (void*)(intptr_t)id);

    lv_obj_t* label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, &font_ui_10, 0);
    lv_obj_set_style_text_color(label, kText, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

void UiControls::slider_cb(lv_event_t* e) {
    auto* self = static_cast<UiControls*>(lv_event_get_user_data(e));
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    const int id = (int)(intptr_t)lv_obj_get_user_data(slider);
    if (self && !self->syncing_) self->onSlider(id);
}

void UiControls::button_cb(lv_event_t* e) {
    auto* self = static_cast<UiControls*>(lv_event_get_user_data(e));
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    const int id = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (self) self->onButton(id);
}

void UiControls::close_cb(lv_event_t* e) {
    auto* self = static_cast<UiControls*>(lv_event_get_user_data(e));
    if (self && self->app_) self->app_->setControlsVisible(false);
}

void UiControls::onSlider(int id) {
    if (!app_) return;
    if (id == 0) app_->setManualSpeed((float)lv_slider_get_value(speed_slider_));
    else if (id == 1) app_->setManualThrottle((float)lv_slider_get_value(throttle_slider_));
    else if (id == 2) app_->setManualRpm((float)lv_slider_get_value(rpm_slider_));
}

void UiControls::onButton(int id) {
    if (!app_) return;
    switch (id) {
    case 0: app_->togglePause(); break;
    case 1: app_->toggleAuto(); break;
    case 2: app_->resetSim(); break;
    case 3: app_->toggleMode(); break;
    case 4: app_->toggleTheme(); break;
    case 5: app_->toggleFps(); break;
    default: break;
    }
}

void UiControls::setLabelText(lv_obj_t* label, const char* text) {
    if (!label) return;
    const char* current = lv_label_get_text(label);
    if (current == nullptr || std::strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
    }
}

void UiControls::refresh(const Telemetry& t, Theme theme, bool nav_mode) {
    if (!panel_) return;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.0f", t.speed_kph);
    setLabelText(speed_value_, buf);
    std::snprintf(buf, sizeof(buf), "%.0f%%", t.throttle);
    setLabelText(throttle_value_, buf);
    std::snprintf(buf, sizeof(buf), "%.0f", t.rpm);
    setLabelText(rpm_value_, buf);

    setLabelText(run_label_, t.running ? "PAUSE" : "START");
    setLabelText(auto_label_, t.auto_drive ? "AUTO ON" : "AUTO OFF");
    setLabelText(mode_label_, nav_mode ? "SPEEDO" : "NAV");
    setLabelText(theme_label_, theme_button_label(theme));
    setLabelText(fps_label_, app_ && app_->perf().visible ? "FPS ON" : "FPS OFF");

    if (!syncing_) {
        syncing_ = true;
        if (t.auto_drive || !lv_slider_is_dragged(speed_slider_)) {
            const int v = (int)std::lround(t.speed_kph);
            if (lv_slider_get_value(speed_slider_) != v) lv_slider_set_value(speed_slider_, v, LV_ANIM_OFF);
        }
        if (t.auto_drive || !lv_slider_is_dragged(throttle_slider_)) {
            const int v = (int)std::lround(t.throttle);
            if (lv_slider_get_value(throttle_slider_) != v) lv_slider_set_value(throttle_slider_, v, LV_ANIM_OFF);
        }
        if (t.auto_drive || !lv_slider_is_dragged(rpm_slider_)) {
            const int v = (int)std::lround(t.rpm);
            if (lv_slider_get_value(rpm_slider_) != v) lv_slider_set_value(rpm_slider_, v, LV_ANIM_OFF);
        }
        syncing_ = false;
    }
}

} // namespace moto
