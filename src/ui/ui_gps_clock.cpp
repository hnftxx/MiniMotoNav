#include "ui/ui_gps_clock.h"

#include <cmath>
#include <cstdio>

#include "ui/fonts/fonts.h"
#include "ui/ui_draw.h"

namespace moto {
namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 240;
const lv_color_t kRed = lv_color_hex(0xE10600);
const lv_color_t kRedBright = lv_color_hex(0xFF2D2D);
const lv_color_t kWhite = lv_color_hex(0xF6F6F8);
const lv_color_t kMuted = lv_color_hex(0x9A9AA2);

void panel_card(lv_layer_t* layer, const Palette& p, int x, int y, int w, int h) {
    draw::rect(layer, x, y, w, h, 10, p.panel, (lv_opa_t)232);
    draw::rect_stroke(layer, x, y, w, h, 10, 1, p.panel_edge, LV_OPA_COVER);
}

void stat_card(lv_layer_t* layer, const Palette& p, int x, int y, int w, int h,
               const char* label, const char* value, const char* unit) {
    panel_card(layer, p, x, y, w, h);
    draw::text(layer, label, &font_ui_10, kMuted, x + 8, y + 7, draw::Align::Left, 1);
    draw::text(layer, value, &font_ui_12, kWhite, x + 8, y + 22, draw::Align::Left, 0);
    const int32_t vw = draw::text_width(value, &font_ui_12, 0);
    draw::text(layer, unit, &font_ui_10, kMuted, x + 8 + vw + 3, y + 28, draw::Align::Left, 0);
}

} // namespace

void GpsClockView::create(lv_obj_t* screen) {
    root_ = lv_obj_create(screen);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, kWidth, kHeight);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root_, draw_cb, LV_EVENT_DRAW_MAIN, this);
}

void GpsClockView::update(const Telemetry& t, const PerfStats& perf, const Clock& clock,
                          float dt) {
    const int prev_clock = clock_.hour_minute();
    telemetry_ = t;
    perf_ = perf;
    clock_ = clock;
    dt = dt > 0.1f ? 0.1f : dt;
    anim_ += dt;
    stats_accum_ += dt;

    lv_area_t compass{162, 32, 238, 134};
    lv_obj_invalidate_area(root_, &compass);

    if (stats_accum_ >= 0.1f) {
        stats_accum_ = 0.0f;
        lv_area_t cards{6, 138, 314, 198};
        lv_obj_invalidate_area(root_, &cards);
        lv_area_t altitude{238, 32, 314, 134};
        lv_obj_invalidate_area(root_, &altitude);
    }

    if (clock.hour_minute() != prev_clock) {
        lv_area_t time_card{6, 32, 160, 134};
        lv_obj_invalidate_area(root_, &time_card);
        lv_area_t header_clock{246, 4, 318, 26};
        lv_obj_invalidate_area(root_, &header_clock);
    }
}

void GpsClockView::draw_cb(lv_event_t* e) {
    auto* self = static_cast<GpsClockView*>(lv_event_get_user_data(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    if (self && layer) self->draw(layer);
}

void GpsClockView::draw(lv_layer_t* layer) {
    draw_background(layer);
    draw_clock(layer);
    draw_compass(layer);
    draw_stats(layer);
}

void GpsClockView::draw_background(lv_layer_t* layer) {
    lv_draw_rect_dsc_t bg;
    lv_draw_rect_dsc_init(&bg);
    bg.bg_grad.dir = LV_GRAD_DIR_VER;
    bg.bg_grad.stops[0].color = palette_.bg_top;
    bg.bg_grad.stops[0].opa = LV_OPA_COVER;
    bg.bg_grad.stops[0].frac = 0;
    bg.bg_grad.stops[1].color = palette_.bg_bottom;
    bg.bg_grad.stops[1].opa = LV_OPA_COVER;
    bg.bg_grad.stops[1].frac = 255;
    bg.bg_grad.stops_count = 2;
    lv_area_t area{0, 0, kWidth - 1, kHeight - 1};
    lv_draw_rect(layer, &bg, &area);

    draw::rect(layer, 0, 0, kWidth, 2, 0, kRed, LV_OPA_COVER);
}

void GpsClockView::draw_clock(lv_layer_t* layer) {
    // Header: judul di tengah, jam dan ikon sinyal di kanan.
    draw::text_in(layer, "GPS CLOCK", &font_ui_12, palette_.ink, 160, 14,
                  draw::Align::Center, 2);
    char hdr[8];
    std::snprintf(hdr, sizeof(hdr), "%02d:%02d", clock_.hour, clock_.minute);
    draw::text(layer, hdr, &font_ui_12, palette_.ink, 284, 8, draw::Align::Right, 0);
    draw::arc(layer, 306, 12, 7, 2, 220.0f, 320.0f, palette_.ink, LV_OPA_COVER, true);
    draw::circle(layer, 306, 17, 2, palette_.ink, LV_OPA_COVER);
    draw::rect(layer, 8, 28, 304, 1, 0, palette_.panel_edge, LV_OPA_COVER);

    // Kartu waktu besar.
    panel_card(layer, palette_, 8, 34, 150, 98);
    char time_buf[12];
    std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d", clock_.hour, clock_.minute);
    draw::text_in(layer, time_buf, &font_timer_46, palette_.ink, 83, 64, draw::Align::Center, 0);
    draw::text_in(layer, "LOCAL TIME", &font_ui_10, kMuted, 83, 100, draw::Align::Center, 2);
}

void GpsClockView::draw_compass(lv_layer_t* layer) {
    const int cx = 200;
    const int cy = 83;
    panel_card(layer, palette_, 164, 34, 72, 98);

    draw::circle(layer, cx, cy, 26, palette_.panel_edge, LV_OPA_60);
    draw::circle(layer, cx, cy, 24, palette_.panel, LV_OPA_COVER);

    for (int i = 0; i < 12; ++i) {
        const float rad = (float)i * 30.0f * 0.01745329252f;
        const bool major = (i % 3) == 0;
        const float inner = major ? 18.0f : 20.0f;
        draw::line(layer, cx + (int)(std::sin(rad) * inner), cy - (int)(std::cos(rad) * inner),
                   cx + (int)(std::sin(rad) * 23.0f), cy - (int)(std::cos(rad) * 23.0f),
                   major ? 2 : 1, palette_.tick, major ? LV_OPA_80 : LV_OPA_30);
    }

    draw::text_in(layer, "N", &font_ui_10, kRedBright, cx, cy - 35, draw::Align::Center, 0);
    draw::text_in(layer, "S", &font_ui_10, kMuted, cx, cy + 35, draw::Align::Center, 0);

    // Jarum: merah ke arah heading, putih ke arah berlawanan.
    const float rad = telemetry_.heading_deg * 0.01745329252f;
    const float sx = std::sin(rad);
    const float sy = -std::cos(rad);
    const float wx = -sy * 4.0f;
    const float wy = sx * 4.0f;
    draw::triangle(layer, cx + sx * 18.0f, cy + sy * 18.0f, cx + wx, cy + wy, cx - wx, cy - wy,
                   kRedBright, LV_OPA_COVER);
    draw::triangle(layer, cx - sx * 17.0f, cy - sy * 17.0f, cx + wx, cy + wy, cx - wx, cy - wy,
                   kWhite, LV_OPA_COVER);
    draw::circle(layer, cx, cy, 2, palette_.panel, LV_OPA_COVER);
}

void GpsClockView::draw_stats(lv_layer_t* layer) {
    // Kartu ketinggian dengan ikon gunung sederhana.
    panel_card(layer, palette_, 240, 34, 72, 98);
    draw::text_in(layer, "ALTITUDE", &font_ui_10, kMuted, 276, 44, draw::Align::Center, 1);
    char alt[16];
    std::snprintf(alt, sizeof(alt), "%.0f", telemetry_.altitude_m);
    draw::text_in(layer, alt, &font_ui_12, kWhite, 268, 60, draw::Align::Center, 0);
    draw::text_in(layer, "m", &font_ui_10, kMuted, 288, 61, draw::Align::Center, 0);
    draw::triangle(layer, 252.0f, 120.0f, 268.0f, 100.0f, 284.0f, 120.0f, kMuted, LV_OPA_50);
    draw::triangle(layer, 266.0f, 120.0f, 276.0f, 108.0f, 286.0f, 120.0f, kMuted, LV_OPA_COVER);

    // Baris statistik bawah.
    char trip[16];
    std::snprintf(trip, sizeof(trip), "%.1f", telemetry_.trip_km);
    stat_card(layer, palette_, 8, 140, 98, 56, "TRIP", trip, "km");

    char spd[16];
    std::snprintf(spd, sizeof(spd), "%.0f", telemetry_.speed_kph);
    stat_card(layer, palette_, 111, 140, 98, 56, "SPEED", spd, "km/h");

    char acc[16];
    std::snprintf(acc, sizeof(acc), "%.1f", telemetry_.gps_accuracy_m);
    stat_card(layer, palette_, 214, 140, 98, 56, "ACCURACY", acc, "m");

    // Footer: garis merah dan nama aplikasi.
    draw::rect(layer, 8, 217, 52, 2, 0, kRed, LV_OPA_COVER);
    draw::rect(layer, 260, 217, 52, 2, 0, kRed, LV_OPA_COVER);
    draw::text_in(layer, "MINIMOTONAV", &font_ui_10, kMuted, 160, 218, draw::Align::Center, 2);
}

} // namespace moto
