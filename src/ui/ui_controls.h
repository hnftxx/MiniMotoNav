#pragma once

#include "lvgl.h"

#include "app/moto_telemetry.h"
#include "ui/ui_theme.h"

namespace moto {

class MotoApp;

class UiControls {
public:
    void create(MotoApp* app);
    void setVisible(bool visible);
    bool visible() const;
    void refresh(const Telemetry& t, Theme theme, bool nav_mode);

private:
    static void slider_cb(lv_event_t* e);
    static void button_cb(lv_event_t* e);
    static void close_cb(lv_event_t* e);

    void onSlider(int id);
    void onButton(int id);

    lv_obj_t* makeButton(lv_obj_t* parent, const char* text, int x, int y, int w, int h, int id);
    lv_obj_t* makeSlider(lv_obj_t* parent, int x, int y, int w, int id, int min, int max,
                         int value);
    void setLabelText(lv_obj_t* label, const char* text);

    MotoApp* app_ = nullptr;
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* speed_slider_ = nullptr;
    lv_obj_t* throttle_slider_ = nullptr;
    lv_obj_t* rpm_slider_ = nullptr;
    lv_obj_t* speed_value_ = nullptr;
    lv_obj_t* throttle_value_ = nullptr;
    lv_obj_t* rpm_value_ = nullptr;
    lv_obj_t* run_label_ = nullptr;
    lv_obj_t* auto_label_ = nullptr;
    lv_obj_t* mode_label_ = nullptr;
    lv_obj_t* theme_label_ = nullptr;
    lv_obj_t* fps_label_ = nullptr;
    bool syncing_ = false;
};

} // namespace moto
