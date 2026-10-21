#include "ui/ui_speedo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/ui_draw.h"

namespace moto {
namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 240;
constexpr int kGaugeCx = 160;
constexpr int kGaugeCy = 116;
constexpr int kGaugeR = 80;
constexpr float kAngleStart = 150.0f;
constexpr float kAngleSweep = 240.0f;
constexpr float kMaxSpeed = 160.0f;
constexpr float kRedZoneKph = 120.0f;
constexpr int kSlotW = 42;
constexpr int kDigitY = 74;

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
float easeOutCubic(float t) { const float u = 1.0f - t; return 1.0f - u * u * u; }
float approach(float cur, float target, float tau, float dt) {
    if (tau <= 0.0001f) return target;
    return cur + (target - cur) * (1.0f - std::exp(-dt / tau));
}
int ipow10(int p) {
    int v = 1;
    while (p-- > 0) v *= 10;
    return v;
}

void arc_bounds(int cx, int cy, int r_in, int r_out, float a0, float a1, lv_area_t& out) {
    out.x1 = cx - r_out;
    out.y1 = cy - r_out;
    out.x2 = cx + r_out;
    out.y2 = cy + r_out;
    bool first = true;
    const int steps = std::max(2, (int)std::ceil(std::fabs(a1 - a0) / 8.0f));
    for (int i = 0; i <= steps; ++i) {
        const float a = a0 + (a1 - a0) * ((float)i / (float)steps);
        const float rad = a * 0.01745329252f;
        const float c = std::cos(rad);
        const float s = std::sin(rad);
        for (int ri = 0; ri < 2; ++ri) {
            const int r = ri == 0 ? r_in : r_out;
            const int32_t x = cx + (int32_t)std::lround(c * (float)r);
            const int32_t y = cy + (int32_t)std::lround(s * (float)r);
            if (first) {
                out.x1 = out.x2 = x;
                out.y1 = out.y2 = y;
                first = false;
            } else {
                out.x1 = std::min(out.x1, x);
                out.y1 = std::min(out.y1, y);
                out.x2 = std::max(out.x2, x);
                out.y2 = std::max(out.y2, y);
            }
        }
    }
    out.x1 -= 14;
    out.y1 -= 14;
    out.x2 += 14;
    out.y2 += 14;
}

} // namespace

void SpeedoView::create(lv_obj_t* screen) {
    screen_ = screen;
    root_ = lv_obj_create(screen);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, kWidth, kHeight);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root_, root_draw_cb, LV_EVENT_DRAW_MAIN, this);

    for (int i = 0; i < 3; ++i) {
        Digit& d = digits_[i];
        d.widget = lv_obj_create(root_);
        lv_obj_remove_style_all(d.widget);
        lv_obj_set_size(d.widget, kSlotW, 76);
        lv_obj_set_pos(d.widget, -80, kDigitY);
        lv_obj_clear_flag(d.widget, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(d.widget, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(d.widget, this);
        lv_obj_add_event_cb(d.widget, digit_draw_cb, LV_EVENT_DRAW_MAIN, (void*)(intptr_t)i);
    }

    display_speed_ = 0.0f;
    shown_value_ = -1;
    update_digits(0, 0.016f);
    for (auto& d : digits_) {
        d.x = d.target_x;
        lv_obj_set_pos(d.widget, (int32_t)std::lround(d.x), kDigitY);
    }
}

void SpeedoView::root_draw_cb(lv_event_t* e) {
    auto* self = static_cast<SpeedoView*>(lv_event_get_user_data(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    if (self && layer) self->draw(layer);
}

void SpeedoView::digit_draw_cb(lv_event_t* e) {
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    auto* self = static_cast<SpeedoView*>(lv_obj_get_user_data(obj));
    lv_layer_t* layer = lv_event_get_layer(e);
    const int place = (int)(intptr_t)lv_event_get_user_data(e);
    if (!self || !layer) return;
    self->draw_digit(layer, place);
}

void SpeedoView::update(const Telemetry& t, const PerfStats& perf, const Clock& clock, float dt) {
    const float prev_speed = telemetry_.speed_kph;
    const int prev_clock = clock_.hour_minute();
    const float prev_trip = telemetry_.trip_km;

    telemetry_ = t;
    perf_ = perf;
    clock_ = clock;

    dt = dt > 0.1f ? 0.1f : dt;
    shimmer_phase_ += dt * 1.15f;

    display_speed_ = approach(display_speed_, t.speed_kph, 0.05f, dt);
    const int value = (int)std::lround(display_speed_ < 0.0f ? 0.0f : display_speed_);
    update_digits(value, dt);
    shown_value_ = value;

    bool digits_animating = false;
    for (const auto& d : digits_) {
        if (d.active &&
            (d.progress < 0.999f || std::fabs(d.x - d.target_x) > 0.2f)) {
            digits_animating = true;
        }
    }
    if (std::fabs(t.speed_kph - prev_speed) > 0.03f) {
        float a0 = kAngleStart + kAngleSweep * clamp01(prev_speed / kMaxSpeed);
        float a1 = kAngleStart + kAngleSweep * clamp01(t.speed_kph / kMaxSpeed);
        if (a1 < a0) std::swap(a0, a1);
        lv_area_t area;
        arc_bounds(kGaugeCx, kGaugeCy, kGaugeR - 12, kGaugeR + 12, a0, a1, area);
        lv_obj_invalidate_area(root_, &area);
    }
    if (digits_animating) {
        lv_area_t area{92, 68, 228, 154};
        lv_obj_invalidate_area(root_, &area);
    }
    if (clock.hour_minute() != prev_clock) {
        lv_area_t area{246, 4, 318, 28};
        lv_obj_invalidate_area(root_, &area);
    }
    if (std::fabs(t.trip_km - prev_trip) > 0.001f) {
        lv_area_t area{116, 162, 204, 180};
        lv_obj_invalidate_area(root_, &area);
    }
    lv_area_t panel_left{8, 182, 152, 232};
    lv_area_t panel_right{152, 182, 311, 232};
    lv_obj_invalidate_area(root_, &panel_left);
    lv_obj_invalidate_area(root_, &panel_right);
}

void SpeedoView::update_digits(int value, float dt) {
    const int n_digits = value >= 100 ? 3 : (value >= 10 ? 2 : 1);
    const int group_x = kGaugeCx - (n_digits * kSlotW) / 2;

    for (int p = 0; p < 3; ++p) {
        Digit& d = digits_[p];
        const int active = p < n_digits;
        const int target_digit = (value / ipow10(p)) % 10;
        float target_x = -70.0f;

        if (active) {
            const int from_left = (n_digits - 1) - p;
            target_x = (float)(group_x + from_left * kSlotW);
        }

        if (active) {
            if (!d.active) {
                d.cur = target_digit;
                d.prev = target_digit;
                d.progress = 1.0f;
            } else if (target_digit != d.cur) {
                d.prev = d.cur;
                d.cur = target_digit;
                d.progress = 0.0f;
            }
            d.progress = clamp01(d.progress + dt / 0.13f);
        } else {
            d.cur = target_digit;
            d.prev = target_digit;
            d.progress = 1.0f;
        }

        d.active = active;
        d.target_x = target_x;
        if (!active) {
            d.x = target_x;
        } else {
            d.x = approach(d.x, target_x, 0.07f, dt);
            if (std::fabs(d.x - target_x) < 0.25f) d.x = target_x;
        }
        lv_obj_set_pos(d.widget, (int32_t)std::lround(d.x), kDigitY);
    }
}

void SpeedoView::draw(lv_layer_t* layer) {
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

    draw::circle(layer, kGaugeCx, kGaugeCy, 124, palette_.ink, (lv_opa_t)8);

    draw_header(layer);
    draw_gauge(layer);
    draw_panel(layer);

    char unit[] = "KM/H";
    draw::text_in(layer, unit, &font_ui_12, palette_.ink_muted, kGaugeCx, 154,
                  draw::Align::Center, 3, LV_OPA_80);
}

void SpeedoView::draw_header(lv_layer_t* layer) {
    draw::text(layer, "GPS", &font_ui_10, palette_.ink, 8, 9, draw::Align::Left, 1);
    draw::text(layer, "ON", &font_ui_10, palette_.good, 8, 21, draw::Align::Left, 1);

    char clock_buf[8];
    std::snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d", clock_.hour, clock_.minute);
    draw::text(layer, clock_buf, &font_ui_20, palette_.ink, 282, 9, draw::Align::Right, 0);
    draw::arc(layer, 304, 16, 8, 2, 220.0f, 320.0f, palette_.ink, LV_OPA_COVER, true);
    draw::circle(layer, 304, 22, 2, palette_.ink, LV_OPA_COVER);

    draw::text(layer, "MODE", &font_ui_10, palette_.warn, 316, 36, draw::Align::Right, 1);
    draw::text(layer, "SPEED >>", &font_ui_10, palette_.ink, 316, 48, draw::Align::Right, 0);
}

void SpeedoView::draw_gauge(lv_layer_t* layer) {
    const float speed = clamp01(telemetry_.speed_kph / kMaxSpeed);
    const float end_angle = kAngleStart + kAngleSweep * speed;
    const float red_start = kAngleStart + kAngleSweep * (kRedZoneKph / kMaxSpeed);

    draw::arc(layer, kGaugeCx, kGaugeCy, kGaugeR, 11, kAngleStart, kAngleStart + kAngleSweep,
              palette_.gauge_track_edge, LV_OPA_30);
    draw::arc(layer, kGaugeCx, kGaugeCy, kGaugeR, 9, kAngleStart, kAngleStart + kAngleSweep,
              palette_.gauge_track, LV_OPA_COVER);

    draw::arc(layer, kGaugeCx, kGaugeCy, kGaugeR, 9, red_start, kAngleStart + kAngleSweep,
              palette_.warn, LV_OPA_60);

    draw::arc(layer, kGaugeCx, kGaugeCy, kGaugeR - 22, 2, kAngleStart,
              kAngleStart + kAngleSweep, palette_.ink_muted, LV_OPA_40);

    for (int i = 0; i <= 40; ++i) {
        const float a = kAngleStart + kAngleSweep * ((float)i / 40.0f);
        const bool major = (i % 5) == 0;
        const float rad = a * 0.01745329252f;
        const float cosv = std::cos(rad);
        const float sinv = std::sin(rad);
        const float r_out = 96.0f;
        const float r_in = major ? 86.0f : 91.0f;
        const int x1 = kGaugeCx + (int)std::lround(cosv * r_out);
        const int y1 = kGaugeCy + (int)std::lround(sinv * r_out);
        const int x2 = kGaugeCx + (int)std::lround(cosv * r_in);
        const int y2 = kGaugeCy + (int)std::lround(sinv * r_in);
        draw::line(layer, x1, y1, x2, y2, major ? 2 : 1, palette_.tick,
                   major ? LV_OPA_70 : LV_OPA_30, true);

        if (major) {
            char label[5];
            std::snprintf(label, sizeof(label), "%d", i * 4);
            const int lx = kGaugeCx + (int)std::lround(cosv * 103.0f);
            const int ly = kGaugeCy + (int)std::lround(sinv * 103.0f);
            draw::text_in(layer, label, &font_ui_10, palette_.tick, lx, ly, draw::Align::Center, 0,
                          LV_OPA_80);
        }
    }

    if (speed > 0.002f) {
        const lv_color_t head_color = mix_color(palette_.accent_a, palette_.accent_b, speed);
        draw::arc_gradient(layer, kGaugeCx, kGaugeCy, kGaugeR, 9, kAngleStart, end_angle,
                           palette_.accent_a, head_color, LV_OPA_COVER, 96);

        const float end_rad = end_angle * 0.01745329252f;
        const int hx = kGaugeCx + (int)std::lround(std::cos(end_rad) * kGaugeR);
        const int hy = kGaugeCy + (int)std::lround(std::sin(end_rad) * kGaugeR);
        draw::circle(layer, hx, hy, 10, head_color, LV_OPA_20);
        draw::circle(layer, hx, hy, 6, lv_color_white(), LV_OPA_COVER);
    }

    char trip_buf[16];
    std::snprintf(trip_buf, sizeof(trip_buf), "TRIP %.1f km", telemetry_.trip_km);
    draw::text_in(layer, trip_buf, &font_ui_10, palette_.ink_muted, kGaugeCx, 169,
                  draw::Align::Center, 1, LV_OPA_70);
}

void SpeedoView::draw_panel(lv_layer_t* layer) {
    const int px = 8;
    const int py = 182;
    const int pw = 304;
    const int ph = 50;

    draw::rect(layer, px, py + 3, pw, ph, 16, lv_color_black(), LV_OPA_40);
    draw::rect(layer, px, py, pw, ph, 16, palette_.panel, LV_OPA_COVER);
    draw::rect_stroke(layer, px, py, pw, ph, 16, 1, palette_.panel_edge, LV_OPA_90);
    draw::line(layer, 152, py + 10, 152, py + ph - 8, 1, palette_.panel_edge, LV_OPA_90, false);

    char th_buf[12];
    std::snprintf(th_buf, sizeof(th_buf), "%.0f%%", telemetry_.throttle);
    draw::text(layer, "THROTTLE", &font_ui_10, palette_.panel_muted, 20, py + 8,
               draw::Align::Left, 1, LV_OPA_90);
    draw::text(layer, th_buf, &font_ui_20, palette_.panel_text, 20, py + 18, draw::Align::Left, 0);

    lv_color_t throttle_colors[24];
    for (int i = 0; i < 24; ++i) {
        throttle_colors[i] = (i < 16) ? lv_color_hex(0xEDEDED) : palette_.accent_b;
    }
    draw::segment_bar(layer, 20, py + 43, 124, 6, 20, telemetry_.throttle / 100.0f,
                      palette_.seg_off, throttle_colors, shimmer_phase_, false);

    char rpm_buf[12];
    std::snprintf(rpm_buf, sizeof(rpm_buf), "%.0f", telemetry_.rpm);
    draw::text(layer, "RPM", &font_ui_10, palette_.panel_muted, 166, py + 8, draw::Align::Left, 1,
               LV_OPA_90);
    draw::text(layer, rpm_buf, &font_ui_20, palette_.panel_text, 166, py + 18, draw::Align::Left,
               0);

    lv_color_t rpm_colors[24];
    for (int i = 0; i < 24; ++i) {
        if (i >= 18) {
            rpm_colors[i] = palette_.warn;
        } else {
            rpm_colors[i] = lv_color_hex(0xEDEDED);
        }
    }
    float rpm_level = (telemetry_.rpm - 1000.0f) / 8200.0f;
    draw::segment_bar(layer, 166, py + 43, 124, 6, 20, clamp01(rpm_level), palette_.seg_off,
                      rpm_colors, shimmer_phase_, false);
}

void SpeedoView::draw_digit(lv_layer_t* layer, int place) {
    Digit& d = digits_[place];
    if (!d.active) return;
    lv_area_t c;
    lv_obj_get_coords(d.widget, &c);
    const int cx = (c.x1 + c.x2) / 2;
    const int cy = (c.y1 + c.y2) / 2;

    const float e = easeOutCubic(clamp01(d.progress));
    char cur_txt[2] = {(char)('0' + d.cur), '\0'};
    char prev_txt[2] = {(char)('0' + d.prev), '\0'};

    if (e < 0.999f && d.prev != d.cur) {
        const int prev_y = cy - (int)std::lround(e * 4.0f);
        const int cur_y = cy + (int)std::lround((1.0f - e) * 4.0f);
        draw::text_in(layer, prev_txt, &font_speed_58, palette_.ink, cx, prev_y,
                      draw::Align::Center, 0, (lv_opa_t)(255.0f * (1.0f - e)));
        draw::text_in(layer, cur_txt, &font_speed_58, palette_.ink, cx, cur_y,
                      draw::Align::Center, 0, (lv_opa_t)(255.0f * e));
    } else {
        draw::text_in(layer, cur_txt, &font_speed_58, palette_.ink, cx, cy, draw::Align::Center, 0);
    }
}

} // namespace moto
