#include "ui/ui_drag.h"

#include <algorithm>
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

void format_time(char* out, size_t size, float seconds) {
    int centis = (int)std::lround(std::max(0.0f, seconds) * 100.0f);
    const int minutes = centis / 6000;
    centis %= 6000;
    const int secs = centis / 100;
    const int frac = centis % 100;
    std::snprintf(out, size, "%02d:%02d.%02d", minutes, secs, frac);
}

void format_distance(char* out, size_t size, float metres) {
    if (metres >= 1000.0f) std::snprintf(out, size, "%.1f km", metres / 1000.0f);
    else std::snprintf(out, size, "%.0f m", metres);
}

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

void panel_card(lv_layer_t* layer, const Palette& p, int x, int y, int w, int h) {
    draw::rect(layer, x, y, w, h, 10, p.panel, (lv_opa_t)232);
    draw::rect_stroke(layer, x, y, w, h, 10, 1, p.panel_edge, LV_OPA_COVER);
}

// Deretan titik segmen. 25% titik terakhir berwarna merah.
void dots(lv_layer_t* layer, const Palette& p, int x, int y, int count, float level) {
    for (int i = 0; i < count; ++i) {
        const float pos = (float)(i + 1) / (float)count;
        lv_color_t c = p.seg_off;
        if (pos <= level + 0.001f) c = (i >= count * 3 / 4) ? kRedBright : kWhite;
        draw::circle(layer, x + i * 7, y, 2, c, LV_OPA_COVER);
    }
}

// Kartu throttle dan RPM di baris bawah.
void live_cards(lv_layer_t* layer, const Palette& p, const Telemetry& t, int y, int h) {
    panel_card(layer, p, 8, y, 150, h);
    draw::text(layer, "THROTTLE", &font_ui_10, kMuted, 18, y + 7, draw::Align::Left, 1);
    char th[12];
    std::snprintf(th, sizeof(th), "%.0f%%", t.throttle);
    draw::text(layer, th, &font_ui_12, p.ink, 18, y + 18, draw::Align::Left, 0);
    dots(layer, p, 18, y + h - 10, 20, clamp01(t.throttle / 100.0f));

    panel_card(layer, p, 162, y, 150, h);
    draw::text(layer, "RPM", &font_ui_10, kMuted, 172, y + 7, draw::Align::Left, 1);
    char rpm[12];
    std::snprintf(rpm, sizeof(rpm), "%.0f", t.rpm);
    draw::text(layer, rpm, &font_ui_12, p.ink, 172, y + 18, draw::Align::Left, 0);
    dots(layer, p, 172, y + h - 10, 20, clamp01((t.rpm - 1000.0f) / 8200.0f));
}

} // namespace

void DragView::create(lv_obj_t* screen) {
    root_ = lv_obj_create(screen);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, kWidth, kHeight);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root_, draw_cb, LV_EVENT_DRAW_MAIN, this);
}

void DragView::draw_cb(lv_event_t* e) {
    auto* self = static_cast<DragView*>(lv_event_get_user_data(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    if (self && layer) self->draw(layer);
}

void DragView::update(const DragSession& drag, const Telemetry& t, const PerfStats& perf,
                      const Clock& clock, float dt) {
    const DragPhase prev_phase = phase_;
    const DragSettings prev_settings = settings_;
    const int prev_clock = clock_.hour_minute();
    phase_ = drag.phase();
    settings_ = drag.settings();
    result_ = drag.result();
    telemetry_ = t;
    perf_ = perf;
    clock_ = clock;
    elapsed_ = drag.elapsed_s();
    speed_ = t.speed_kph;
    distance_ = drag.distance_m();
    progress_ = drag.progress();
    countdown_ = drag.countdown_remaining_s();
    countdown_digit_ = drag.countdown_digit();
    best_delta_ = drag.bestDeltaS();
    new_best_ = drag.newBest();
    gps_ok_ = drag.gpsReliable();
    accel_complete_ = drag.accelComplete();
    accel_s_ = drag.accelTimeS();
    dt = dt > 0.1f ? 0.1f : dt;
    anim_ += dt * 3.0f;
    stats_accum_ += dt;

    const DragResult* best = drag.personalBest();
    best_ = best ? best->elapsed_s : 0.0f;
    last_split_s_ = -1.0f;
    last_split_m_ = 0;
    if (const DragSplit* split = drag.lastSplit()) {
        last_split_s_ = split->elapsed_s;
        last_split_m_ = (int)std::lround(split->distance_m);
    }
    if (phase_ == DragPhase::Finished) {
        speed_ = result_.max_speed_kph;
        accel_complete_ = result_.accel_complete;
        accel_s_ = result_.accel_time_s;
        distance_ = result_.distance_m;
        progress_ = 1.0f;
        elapsed_ = result_.elapsed_s;
    }
    const bool settings_changed =
        settings_.target_distance_m != prev_settings.target_distance_m ||
        settings_.countdown_ms != prev_settings.countdown_ms ||
        settings_.split_interval_m != prev_settings.split_interval_m ||
        settings_.accel_test != prev_settings.accel_test;

    if (phase_ != prev_phase || settings_changed) {
        lv_obj_invalidate(root_);
    } else {
        switch (phase_) {
        case DragPhase::Countdown: {
            lv_area_t digit{96, 46, 224, 146};
            lv_obj_invalidate_area(root_, &digit);
            if (stats_accum_ >= 0.1f) {
                stats_accum_ = 0.0f;
                lv_area_t speed_card{222, 34, 314, 88};
                lv_area_t cards{6, 184, 314, 232};
                lv_obj_invalidate_area(root_, &speed_card);
                lv_obj_invalidate_area(root_, &cards);
            }
            break;
        }
        case DragPhase::Running: {
            lv_area_t timer{76, 48, 244, 96};
            lv_obj_invalidate_area(root_, &timer);
            if (stats_accum_ >= 0.1f) {
                stats_accum_ = 0.0f;
                lv_area_t speed{6, 98, 104, 152};
                lv_area_t distance{112, 98, 314, 142};
                lv_area_t cards{6, 160, 314, 232};
                lv_obj_invalidate_area(root_, &speed);
                lv_obj_invalidate_area(root_, &distance);
                lv_obj_invalidate_area(root_, &cards);
            }
            break;
        }
        case DragPhase::Idle:
            if (stats_accum_ >= 0.1f) {
                stats_accum_ = 0.0f;
                lv_area_t cards{6, 180, 314, 232};
                lv_obj_invalidate_area(root_, &cards);
            }
            break;
        case DragPhase::Paused:
        case DragPhase::Finished:
        default:
            break;
        }
    }

    if (clock.hour_minute() != prev_clock) {
        lv_area_t header_clock{246, 4, 318, 26};
        lv_obj_invalidate_area(root_, &header_clock);
    }
}

void DragView::draw(lv_layer_t* layer) {
    draw_background(layer);
    draw_header(layer);
    switch (phase_) {
    case DragPhase::Countdown: draw_countdown(layer); break;
    case DragPhase::Running:
    case DragPhase::Paused: draw_running(layer); break;
    case DragPhase::Finished: draw_finished(layer); break;
    case DragPhase::Idle:
    default: draw_idle(layer); break;
    }
}

void DragView::draw_background(lv_layer_t* layer) {
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
    for (int i = 0; i < 7; ++i) {
        const int y = 200 + i * 6;
        const int w = 30 + i * 20;
        draw::line(layer, kWidth - 8, y, kWidth - 8 - w, y, 1, kRed, (lv_opa_t)10);
    }
}

void DragView::draw_header(lv_layer_t* layer) {
    // Judul dengan dua garis merah miring di kiri dan kanan.
    draw::text_in(layer, "DRAG MODE", &font_ui_12, palette_.ink, 160, 14,
                  draw::Align::Center, 2);
    draw::line(layer, 94, 22, 100, 6, 3, kRed, LV_OPA_COVER, false);
    draw::line(layer, 102, 22, 108, 6, 3, kRed, LV_OPA_COVER, false);
    draw::line(layer, 212, 22, 218, 6, 3, kRed, LV_OPA_COVER, false);
    draw::line(layer, 220, 22, 226, 6, 3, kRed, LV_OPA_COVER, false);

    // Jam dan ikon sinyal di kanan.
    char clock_buf[8];
    std::snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d", clock_.hour, clock_.minute);
    draw::text(layer, clock_buf, &font_ui_12, palette_.ink, 284, 8, draw::Align::Right, 0);
    draw::arc(layer, 306, 12, 7, 2, 220.0f, 320.0f, palette_.ink, LV_OPA_COVER, true);
    draw::circle(layer, 306, 17, 2, palette_.ink, LV_OPA_COVER);

    draw::rect(layer, 8, 28, 304, 1, 0, palette_.panel_edge, LV_OPA_COVER);
}

void DragView::draw_card(lv_layer_t* layer, int x, int y, int w, int h) {
    panel_card(layer, palette_, x, y, w, h);
}

void DragView::draw_idle(lv_layer_t* layer) {
    draw_card(layer, 8, 36, 88, 50);
    draw::text(layer, "TARGET", &font_ui_10, kMuted, 16, 42, draw::Align::Left, 1);
    char target[16];
    format_distance(target, sizeof(target), settings_.target_distance_m);
    draw::text(layer, target, &font_ui_20, palette_.ink, 16, 58, draw::Align::Left, 0);

    draw_card(layer, 224, 36, 88, 50);
    draw::text(layer, "COUNTDOWN", &font_ui_10, kMuted, 232, 42, draw::Align::Left, 1);
    char cd[16];
    std::snprintf(cd, sizeof(cd), "%.1f s", (float)settings_.countdown_ms / 1000.0f);
    draw::text(layer, cd, &font_ui_20, palette_.ink, 232, 58, draw::Align::Left, 0);

    draw::text_in(layer, "00:00.00", &font_timer_46, palette_.ink, 160, 84,
                  draw::Align::Center, 1, LV_OPA_60);
    draw::text_in(layer, "READY TO LAUNCH", &font_ui_12, kRedBright, 160, 128,
                  draw::Align::Center, 2);
    draw::text_in(layer, "PRESS SPACE TO ARM", &font_ui_10, kMuted, 160, 152,
                  draw::Align::Center, 1);

    live_cards(layer, palette_, telemetry_, 182, 48);
}

void DragView::draw_countdown(lv_layer_t* layer) {
    draw_card(layer, 8, 36, 88, 50);
    draw::text(layer, "TARGET", &font_ui_10, kMuted, 16, 42, draw::Align::Left, 1);
    char target[16];
    format_distance(target, sizeof(target), settings_.target_distance_m);
    draw::text(layer, target, &font_ui_20, palette_.ink, 16, 58, draw::Align::Left, 0);

    draw_card(layer, 224, 36, 88, 50);
    draw::text(layer, "SPEED", &font_ui_10, kMuted, 232, 42, draw::Align::Left, 1);
    char spd[16];
    std::snprintf(spd, sizeof(spd), "%.0f", speed_);
    draw::text(layer, spd, &font_ui_20, palette_.ink, 232, 58, draw::Align::Left, 0);
    draw::text(layer, "KM/H", &font_ui_10, kMuted, 232, 76, draw::Align::Left, 1);

    // Angka hitung mundur di tengah, dengan cahaya merah di belakang.
    const float pulse = 0.5f + 0.5f * std::sin(anim_ * 2.0f);
    draw::circle(layer, 160, 96, 34 + (int)(pulse * 3.0f), kRed, (lv_opa_t)(18 + pulse * 12));

    // Chevron kiri dan kanan.
    draw::line(layer, 116, 80, 104, 96, 4, kRed, LV_OPA_COVER, true);
    draw::line(layer, 104, 96, 116, 112, 4, kRed, LV_OPA_COVER, true);
    draw::line(layer, 204, 80, 216, 96, 4, kRed, LV_OPA_COVER, true);
    draw::line(layer, 216, 96, 204, 112, 4, kRed, LV_OPA_COVER, true);

    char digit[8];
    if (countdown_digit_ > 0) std::snprintf(digit, sizeof(digit), "%d", countdown_digit_);
    else std::snprintf(digit, sizeof(digit), "GO!");
    draw::text_in(layer, digit, &font_count_96, kWhite, 160, 96, draw::Align::Center, 0);

    draw::text_in(layer, countdown_digit_ > 0 ? "READY?" : "LAUNCH", &font_ui_12, kRedBright,
                  160, 140, draw::Align::Center, 2);

    // Baris jarak dengan bar progres.
    draw::text(layer, "DISTANCE", &font_ui_10, kMuted, 16, 158, draw::Align::Left, 1);
    char dist[40];
    std::snprintf(dist, sizeof(dist), "%.1f / %.1f km", distance_ / 1000.0f,
                  settings_.target_distance_m / 1000.0f);
    draw::text(layer, dist, &font_ui_12, palette_.ink, 16, 168, draw::Align::Left, 0);
    draw::rect(layer, 200, 164, 108, 6, 3, palette_.seg_off, LV_OPA_COVER);
    const int fill = (int)std::lround(108 * clamp01(progress_));
    if (fill > 0) draw::rect(layer, 200, 164, fill, 6, 3, kRed, LV_OPA_COVER);

    live_cards(layer, palette_, telemetry_, 186, 44);
}

void DragView::draw_running(lv_layer_t* layer) {
    const bool paused = phase_ == DragPhase::Paused;
    draw::text(layer, "ELAPSED TIME", &font_ui_10, kMuted, 12, 36, draw::Align::Left, 1);
    if (paused) {
        draw::text(layer, "PAUSED", &font_ui_10, kRedBright, 308, 36, draw::Align::Right, 1);
    } else {
        draw::rect_stroke(layer, 256, 32, 52, 16, 8, 1, kRedBright, LV_OPA_COVER);
        draw::text_in(layer, "LIVE", &font_ui_10, kRedBright, 282, 40, draw::Align::Center, 1);
    }

    char time_buf[16];
    format_time(time_buf, sizeof(time_buf), elapsed_);
    draw::text_in(layer, time_buf, &font_timer_46, paused ? kRedBright : palette_.ink, 160, 70,
                  draw::Align::Center, 1);

    // Kartu kecepatan.
    draw_card(layer, 8, 100, 92, 50);
    draw::text(layer, "SPEED", &font_ui_10, kMuted, 16, 106, draw::Align::Left, 1);
    char speed[16];
    std::snprintf(speed, sizeof(speed), "%.0f", speed_);
    draw::text(layer, speed, &font_ui_20, palette_.ink, 16, 118, draw::Align::Left, 0);
    draw::text(layer, "KM/H", &font_ui_10, kMuted, 16, 139, draw::Align::Left, 1);

    // Jarak, PB, dan bar progres.
    draw::text(layer, "DISTANCE", &font_ui_10, kMuted, 116, 102, draw::Align::Left, 1);
    if (best_ > 0.0f) {
        char bt[16];
        char pb[32];
        format_time(bt, sizeof(bt), best_);
        std::snprintf(pb, sizeof(pb), "PB %s", bt);
        draw::text(layer, pb, &font_ui_10, kMuted, 308, 102, draw::Align::Right, 0);
    }
    char dist[40];
    std::snprintf(dist, sizeof(dist), "%.2f / %.1f km", distance_ / 1000.0f,
                  settings_.target_distance_m / 1000.0f);
    draw::text(layer, dist, &font_ui_12, palette_.ink, 116, 114, draw::Align::Left, 0);

    const int bar_x = 116;
    const int bar_w = 196;
    draw::rect(layer, bar_x, 132, bar_w, 6, 3, palette_.seg_off, LV_OPA_COVER);
    const int fill = (int)std::lround(bar_w * clamp01(progress_));
    if (fill > 0) draw::rect(layer, bar_x, 132, fill, 6, 3, kRed, LV_OPA_COVER);
    draw::circle(layer, bar_x + fill, 135, 4, kWhite, LV_OPA_COVER);

    live_cards(layer, palette_, telemetry_, 162, 68);
}

void DragView::draw_finished(lv_layer_t* layer) {
    // Banner merah.
    draw::rect(layer, 56, 34, 208, 22, 4, kRed, LV_OPA_COVER);
    draw::text_in(layer, "DRAG COMPLETED", &font_ui_12, kWhite, 160, 45,
                  draw::Align::Center, 2);

    // Waktu besar di kartu utama.
    draw_card(layer, 8, 62, 304, 64);
    draw::text_in(layer, "TIME", &font_ui_10, kMuted, 160, 70, draw::Align::Center, 2);
    char time_buf[16];
    format_time(time_buf, sizeof(time_buf), elapsed_);
    draw::text_in(layer, time_buf, &font_timer_46, palette_.ink, 160, 100,
                  draw::Align::Center, 1);

    // Baris kedua: jarak, rata-rata, kecepatan maks.
    draw_card(layer, 8, 132, 92, 50);
    draw::text(layer, "DISTANCE", &font_ui_10, kMuted, 16, 138, draw::Align::Left, 1);
    char dist[24];
    format_distance(dist, sizeof(dist), result_.distance_m);
    draw::text(layer, dist, &font_ui_12, palette_.ink, 16, 153, draw::Align::Left, 0);

    draw_card(layer, 106, 132, 92, 50);
    draw::text(layer, "AVG KM/H", &font_ui_10, kMuted, 114, 138, draw::Align::Left, 1);
    char avgs[16];
    std::snprintf(avgs, sizeof(avgs), "%.0f", result_.avg_speed_kph);
    draw::text(layer, avgs, &font_ui_20, palette_.ink, 114, 152, draw::Align::Left, 0);

    draw_card(layer, 204, 132, 108, 50);
    draw::text(layer, "MAX KM/H", &font_ui_10, kMuted, 212, 138, draw::Align::Left, 1);
    char maxs[16];
    std::snprintf(maxs, sizeof(maxs), "%.0f", result_.max_speed_kph);
    draw::text(layer, maxs, &font_ui_20, palette_.ink, 212, 152, draw::Align::Left, 0);

    // Baris ketiga: daftar split dan personal best.
    draw_card(layer, 8, 188, 200, 36);
    draw::text(layer, "SPLITS", &font_ui_10, kMuted, 16, 193, draw::Align::Left, 1);
    if (result_.split_count > 0) {
        const uint32_t start = result_.split_count > 2 ? result_.split_count - 2 : 0;
        for (uint32_t i = start; i < result_.split_count; ++i) {
            const DragSplit& split = result_.splits[i];
            char line[32];
            std::snprintf(line, sizeof(line), "%.0f m %.3f s", split.distance_m,
                          split.elapsed_s);
            const int col = (int)(i - start);
            draw::text(layer, line, &font_ui_10, palette_.ink, 16 + col * 96, 207,
                       draw::Align::Left, 0);
        }
    } else {
        char target[32];
        std::snprintf(target, sizeof(target), "TARGET %.0f M", result_.target_distance_m);
        draw::text(layer, target, &font_ui_10, kMuted, 16, 207, draw::Align::Left, 0);
    }

    draw_card(layer, 214, 188, 98, 36);
    draw::rect_stroke(layer, 214, 188, 98, 36, 10, 1, new_best_ ? kRedBright : kRed,
                      LV_OPA_60);
    draw::text(layer, new_best_ ? "NEW BEST" : "PB", &font_ui_10,
               new_best_ ? kRedBright : kMuted, 222, 193, draw::Align::Left, 1);
    char pb[24];
    if (best_ > 0.0f) format_time(pb, sizeof(pb), best_);
    else std::snprintf(pb, sizeof(pb), "--:--.--");
    draw::text(layer, pb, &font_ui_10, palette_.ink, 222, 207, draw::Align::Left, 0);

    // Keterangan tambahan di bawah.
    if (settings_.accel_test != AccelTest::None) {
        char accel[40];
        const char* name = settings_.accel_test == AccelTest::ZeroTo60 ? "0-60" : "0-100";
        if (accel_complete_ && accel_s_ > 0.0f) {
            std::snprintf(accel, sizeof(accel), "%s KM/H  %.2f s", name, accel_s_);
        } else {
            std::snprintf(accel, sizeof(accel), "%s KM/H  --", name);
        }
        draw::text(layer, accel, &font_ui_10, kRedBright, 12, 230, draw::Align::Left, 1);
    }

    if (!result_.gps_reliable) {
        draw::text(layer, "GPS UNRELIABLE", &font_ui_10, palette_.warn, 308, 230,
                   draw::Align::Right, 1);
    }
}

} // namespace moto
