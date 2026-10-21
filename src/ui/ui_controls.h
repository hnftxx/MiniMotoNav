#pragma once

#include "lvgl.h"

#include "app/drag_session.h"
#include "app/moto_telemetry.h"
#include "app/moto_types.h"
#include "ui/ui_theme.h"

namespace moto {

class MotoApp;

class UiControls {
public:
    void create(MotoApp* app);
    void createDragPanel();
    void setVisible(bool visible);
    bool visible() const;
    void refresh(const Telemetry& t, Theme theme, AppMode mode, const DragSession& drag);

private:
    static void slider_cb(lv_event_t* e);
    static void button_cb(lv_event_t* e);
    static void close_cb(lv_event_t* e);
    static void drag_slider_cb(lv_event_t* e);

    void onSlider(int id);
    void onDragSlider(int id);
    void onButton(int id);
    void openDragPanel();
    void closeDragPanel();

    lv_obj_t* makeButton(lv_obj_t* parent, const char* text, int x, int y, int w, int h, int id);
    lv_obj_t* makeSlider(lv_obj_t* parent, int x, int y, int w, int id, int min, int max,
                         int value);
    void setLabelText(lv_obj_t* label, const char* text);

    MotoApp* app_ = nullptr;
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* drag_panel_ = nullptr;
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

    lv_obj_t* drag_target_slider_ = nullptr;
    lv_obj_t* drag_countdown_slider_ = nullptr;
    lv_obj_t* drag_split_slider_ = nullptr;
    lv_obj_t* drag_target_value_ = nullptr;
    lv_obj_t* drag_countdown_value_ = nullptr;
    lv_obj_t* drag_split_value_ = nullptr;
    lv_obj_t* drag_accel_label_ = nullptr;
    lv_obj_t* drag_run_label_ = nullptr;
    lv_obj_t* drag_best_label_ = nullptr;
    lv_obj_t* drag_last_label_ = nullptr;
    bool syncing_ = false;
};

} // namespace moto
