#include "ui/ui_nav.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/ui_draw.h"

namespace moto {
namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

const char* kStreetNames[4] = {
    "Jl. Margonda Raya",
    "Jl. Ir. H. Juanda",
    "Jl. Siliwangi",
    "Jl. Kartini",
};

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

float angle_delta(float a, float b) {
    float d = b - a;
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

uint32_t cell_hash(int x, int y) {
    uint32_t h = (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

} // namespace

void NavView::create(lv_obj_t* screen) {
    build_route();
    root_ = lv_obj_create(screen);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, kWidth, kHeight);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(root_, draw_cb, LV_EVENT_DRAW_MAIN, this);

    reset();
}

void NavView::reset() {
    if (route_.empty()) return;
    car_x_ = route_.front().x;
    car_y_ = route_.front().y;
    heading_target_ = 0.0f;
    heading_ = 0.0f;
    route_dist_ = 0.0f;
    route_hint_ = 0;
    map_time_ = 0.0f;
    update_strings();
    if (root_) lv_obj_invalidate(root_);
}

void NavView::build_route() {
    route_.clear();
    maneuvers_.clear();

    const float W = -700.0f, E = 700.0f, S = -700.0f, N = 700.0f, R = 70.0f;
    const float start_y = 500.0f;

    auto add_point = [this](float x, float y) {
        Point p;
        p.x = x;
        p.y = y;
        p.dist = route_.empty() ? 0.0f : route_.back().dist;
        if (!route_.empty()) {
            const float dx = x - route_.back().x;
            const float dy = y - route_.back().y;
            p.dist += std::sqrt(dx * dx + dy * dy);
        }
        route_.push_back(p);
    };
    auto add_line = [&](float x0, float y0, float x1, float y1) {
        const float dx = x1 - x0, dy = y1 - y0;
        const float len = std::sqrt(dx * dx + dy * dy);
        const int steps = std::max(2, (int)std::ceil(len / 8.0f));
        for (int i = 1; i <= steps; ++i) {
            const float t = (float)i / (float)steps;
            add_point(x0 + dx * t, y0 + dy * t);
        }
    };
    auto add_arc = [&](float cx, float cy, float start_deg, float end_deg, float radius) {
        const int steps = 10;
        for (int i = 1; i <= steps; ++i) {
            const float t = (float)i / (float)steps;
            const float a = (start_deg + (end_deg - start_deg) * t) * kDegToRad;
            add_point(cx + std::cos(a) * radius, cy + std::sin(a) * radius);
        }
    };

    add_point(W, start_y);
    add_line(W, start_y, W, N - R);

    maneuvers_.push_back({route_.back().dist, 0, +1});
    add_arc(W + R, N - R, 180.0f, 90.0f, R);
    add_line(W + R, N, E - R, N);

    maneuvers_.push_back({route_.back().dist, 1, +1});
    add_arc(E - R, N - R, 90.0f, 0.0f, R);
    add_line(E, N - R, E, S + R);

    maneuvers_.push_back({route_.back().dist, 2, +1});
    add_arc(E - R, S + R, 0.0f, -90.0f, R);
    add_line(E - R, S, W + R, S);

    maneuvers_.push_back({route_.back().dist, 3, +1});
    add_arc(W + R, S + R, -90.0f, -180.0f, R);
    add_line(W, S + R, W, start_y);

    route_len_ = route_.back().dist;
}

void NavView::project(float wx, float wy, Projected& out) const {
    const float rad = heading_ * kDegToRad;
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    const float dx = wx - car_x_;
    const float dy = wy - car_y_;
    out.x = kCarX + (int)std::lround(dx * c - dy * s);
    out.y = kCarY - (int)std::lround(dx * s + dy * c);
    out.on_screen = out.x >= -120 && out.x <= kWidth + 120 && out.y >= -120 &&
                    out.y <= kHeight + 120;
}

int NavView::next_maneuver_index() const {
    for (int i = 0; i < (int)maneuvers_.size(); ++i) {
        if (maneuvers_[i].dist > route_dist_ + 1.0f) return i;
    }
    return 0;
}

void NavView::update(const Telemetry& t, const PerfStats& perf, const Clock& clock, float dt) {
    perf_ = perf;
    clock_ = clock;
    dt = dt > 0.1f ? 0.1f : dt;
    map_time_ += dt;

    if (t.running && t.speed_kph > 0.1f) {
        route_dist_ += (t.speed_kph / 3.6f) * dt;
        if (route_dist_ >= route_len_) route_dist_ -= route_len_;
    }

    // Locate the current path segment. The hint only moves forward, so this is O(1) in practice.
    while (route_hint_ + 1 < (int)route_.size() && route_[route_hint_ + 1].dist <= route_dist_) {
        ++route_hint_;
    }
    while (route_hint_ > 0 && route_[route_hint_].dist > route_dist_) {
        --route_hint_;
    }

    const Point& a = route_[route_hint_];
    const Point& b = route_[std::min(route_hint_ + 1, (int)route_.size() - 1)];
    const float seg_len = std::max(0.001f, b.dist - a.dist);
    float f = clamp01((route_dist_ - a.dist) / seg_len);
    car_x_ = a.x + (b.x - a.x) * f;
    car_y_ = a.y + (b.y - a.y) * f;

    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    if (std::fabs(dx) + std::fabs(dy) > 0.0001f) {
        heading_target_ = std::atan2(dx, dy) * kRadToDeg;
    }
    heading_ += angle_delta(heading_, heading_target_) * (1.0f - std::exp(-dt / 0.22f));

    update_strings();
    redraw_accum_ += dt;
    if (redraw_accum_ >= 1.0f / 30.0f) {
        redraw_accum_ = 0.0f;
        lv_obj_invalidate(root_);
    }
}

void NavView::update_strings() {
    const int mi = next_maneuver_index();
    const Maneuver& m = maneuvers_[mi];
    float to_turn = m.dist - route_dist_;
    if (to_turn < 0.0f) to_turn += route_len_;
    if (to_turn < 50.0f) {
        std::snprintf(turn_dist_, sizeof(turn_dist_), "TURN NOW");
    } else if (to_turn < 1000.0f) {
        std::snprintf(turn_dist_, sizeof(turn_dist_), "%d m", (int)(std::round(to_turn / 10.0f) * 10));
    } else {
        std::snprintf(turn_dist_, sizeof(turn_dist_), "%.1f km", to_turn / 1000.0f);
    }
    std::snprintf(turn_street_, sizeof(turn_street_), "via %s", kStreetNames[m.street]);
    next_turn_dir_ = m.turn;

    const float remaining = route_len_ - route_dist_;
    if (remaining < 1000.0f) {
        std::snprintf(remain_dist_, sizeof(remain_dist_), "%d m", (int)remaining);
    } else {
        std::snprintf(remain_dist_, sizeof(remain_dist_), "%.1f km", remaining / 1000.0f);
    }

    // Use a realistic city average when stationary so the ETA does not explode.
    const float avg_mps = 11.4f; // ~41 km/h
    int minutes = (int)std::ceil((remaining / avg_mps) / 60.0f);
    if (minutes < 1) minutes = 1;
    std::snprintf(remain_time_, sizeof(remain_time_), "%d min", minutes);

    int eta_total = clock_.hour_minute() + minutes;
    eta_total %= 24 * 60;
    std::snprintf(eta_, sizeof(eta_), "%02d:%02d", eta_total / 60, eta_total % 60);

    if (perf_.visible) {
        std::snprintf(fps_, sizeof(fps_), "PC %.0fFPS %.1fMS", perf_.fps, perf_.render_ms);
        if (perf_.hw_valid) {
            std::snprintf(hw_a_, sizeof(hw_a_), "PC %.0f  %s %.0f FPS", perf_.fps,
                          perf_.hw_measured ? "HW" : "ESP", perf_.hw_fps);
            std::snprintf(hw_b_, sizeof(hw_b_), "FRM %.1f  SPI %.1f ms", perf_.hw_frame_ms,
                          perf_.hw_spi_ms);
            std::snprintf(hw_c_, sizeof(hw_c_), "JIT %.1f  DROP %.0f%%", perf_.hw_jitter_ms,
                          perf_.hw_drop_pct);
        } else {
            hw_a_[0] = '\0';
            hw_b_[0] = '\0';
            hw_c_[0] = '\0';
        }
    } else {
        fps_[0] = '\0';
        hw_a_[0] = '\0';
        hw_b_[0] = '\0';
        hw_c_[0] = '\0';
    }
}

void NavView::draw_cb(lv_event_t* e) {
    auto* self = static_cast<NavView*>(lv_event_get_user_data(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    if (self && layer) self->draw(layer);
}

void NavView::draw(lv_layer_t* layer) {
    draw_map(layer);
    draw_route(layer);
    draw_hud(layer);
}

void NavView::draw_map(lv_layer_t* layer) {
    draw::rect(layer, 0, 0, kWidth, kHeight, 0, palette_.map_land, LV_OPA_COVER);

    const int cx_cell = (int)std::floor(car_x_ / 100.0f);
    const int cy_cell = (int)std::floor(car_y_ / 100.0f);

    // City blocks, parks, and ponds on a deterministic grid.
    for (int gy = cy_cell - 3; gy <= cy_cell + 3; ++gy) {
        for (int gx = cx_cell - 3; gx <= cx_cell + 3; ++gx) {
            const uint32_t h = cell_hash(gx, gy);
            Projected c;
            project((float)gx * 100.0f + 50.0f, (float)gy * 100.0f + 50.0f, c);
            if (!c.on_screen) continue;

            const int block = 70;
            const float half = block * 0.5f * kScale;
            if ((h % 11u) == 0u) {
                draw::circle(layer, c.x, c.y, (int)half, palette_.map_water, LV_OPA_COVER);
                continue;
            }
            if ((h % 7u) == 0u) {
                draw::circle(layer, c.x, c.y, (int)half, palette_.map_park, LV_OPA_COVER);
                continue;
            }

            const float rad = heading_ * kDegToRad;
            const float co = std::cos(rad);
            const float si = std::sin(rad);
            const float wx[4] = {-half, half, half, -half};
            const float wy[4] = {-half, -half, half, half};
            int px[4];
            int py[4];
            for (int i = 0; i < 4; ++i) {
                px[i] = c.x + (int)std::lround(wx[i] * co - wy[i] * si);
                py[i] = c.y - (int)std::lround(wx[i] * si + wy[i] * co);
            }
            draw::triangle(layer, (float)px[0], (float)py[0], (float)px[1], (float)py[1],
                           (float)px[2], (float)py[2], palette_.map_block, LV_OPA_COVER);
            draw::triangle(layer, (float)px[0], (float)py[0], (float)px[2], (float)py[2],
                           (float)px[3], (float)py[3], palette_.map_block, LV_OPA_COVER);
        }
    }

    struct RoadLine {
        Projected a;
        Projected b;
        int width;
    };
    RoadLine roads[40];
    int road_count = 0;

    const int kx0 = (int)std::floor((car_x_ - 260.0f) / 100.0f);
    const int kx1 = (int)std::ceil((car_x_ + 260.0f) / 100.0f);
    const int ky0 = (int)std::floor((car_y_ - 260.0f) / 100.0f);
    const int ky1 = (int)std::ceil((car_y_ + 260.0f) / 100.0f);

    for (int k = kx0; k <= kx1 && road_count < 40; ++k) {
        RoadLine& r = roads[road_count++];
        project((float)k * 100.0f, car_y_ - 280.0f, r.a);
        project((float)k * 100.0f, car_y_ + 280.0f, r.b);
        r.width = (k % 3 == 0) ? 12 : 7;
    }
    for (int k = ky0; k <= ky1 && road_count < 40; ++k) {
        RoadLine& r = roads[road_count++];
        project(car_x_ - 280.0f, (float)k * 100.0f, r.a);
        project(car_x_ + 280.0f, (float)k * 100.0f, r.b);
        r.width = (k % 3 == 0) ? 12 : 7;
    }

    for (int i = 0; i < road_count; ++i) {
        const RoadLine& r = roads[i];
        draw::line(layer, r.a.x, r.a.y, r.b.x, r.b.y, r.width + 4, palette_.map_case,
                   LV_OPA_COVER, false);
    }
    for (int i = 0; i < road_count; ++i) {
        const RoadLine& r = roads[i];
        draw::line(layer, r.a.x, r.a.y, r.b.x, r.b.y, r.width, palette_.map_road, LV_OPA_COVER,
                   false);
    }

    // Lane hints on the wider avenues.
    for (int i = 0; i < road_count; ++i) {
        const RoadLine& r = roads[i];
        if (r.width < 10) continue;
        draw::line(layer, r.a.x, r.a.y, r.b.x, r.b.y, 1, palette_.map_case, LV_OPA_30, false);
    }
}

void NavView::draw_route(lv_layer_t* layer) {
    const int count = (int)route_.size();
    if (count < 2) return;

    Projected prev{};
    bool have_prev = false;
    Projected prev2{};
    bool have_prev2 = false;

    for (int step = -14; step <= 40; ++step) {
        int idx = (route_hint_ + step) % count;
        if (idx < 0) idx += count;
        const Point& p = route_[idx];
        Projected sp;
        project(p.x, p.y, sp);
        const float dx = p.x - car_x_;
        const float dy = p.y - car_y_;
        const bool near = dx * dx + dy * dy < 260.0f * 260.0f;

        if (have_prev && near) {
            draw::line(layer, prev.x, prev.y, sp.x, sp.y, 13, palette_.route_case,
                       LV_OPA_COVER, true);
        }
        if (have_prev2 && near) {
            draw::line(layer, prev2.x, prev2.y, prev.x, prev.y, 8, palette_.route, LV_OPA_COVER,
                       true);
            draw::line(layer, prev2.x, prev2.y, prev.x, prev.y, 2, palette_.route_core,
                       LV_OPA_40, true);
        }
        prev2 = prev;
        have_prev2 = have_prev;
        prev = sp;
        have_prev = true;
    }

    // Direction chevrons along the route.
    for (int step = 0; step < 70; step += 8) {
        int idx = (route_hint_ + step) % count;
        const Point& p = route_[idx];
        const float dx = p.x - car_x_;
        const float dy = p.y - car_y_;
        if (dx * dx + dy * dy > 150.0f * 150.0f) continue;
        if (idx + 1 >= count) continue;
        const Point& q = route_[idx + 1];
        Projected a, b;
        project(p.x, p.y, a);
        project(q.x, q.y, b);
        const float vx = (float)(b.x - a.x);
        const float vy = (float)(b.y - a.y);
        const float len = std::sqrt(vx * vx + vy * vy);
        if (len < 0.1f) continue;
        const float ux = vx / len;
        const float uy = vy / len;
        const float px = -uy;
        const float py = ux;
        const int cx = (a.x + b.x) / 2;
        const int cy = (a.y + b.y) / 2;
        draw::line(layer, cx - (int)(ux * 5 + px * 4), cy - (int)(uy * 5 + py * 4),
                   cx + (int)(ux * 4), cy + (int)(uy * 4), 2, lv_color_white(), LV_OPA_80, true);
        draw::line(layer, cx - (int)(ux * 5 - px * 4), cy - (int)(uy * 5 - py * 4),
                   cx + (int)(ux * 4), cy + (int)(uy * 4), 2, lv_color_white(), LV_OPA_80, true);
    }

    // Vehicle puck.
    draw::circle(layer, kCarX, kCarY + 2, 16, lv_color_black(), LV_OPA_20);
    draw::circle(layer, kCarX, kCarY, 14, lv_color_white(), LV_OPA_COVER);
    draw::circle(layer, kCarX, kCarY, 12, palette_.route, LV_OPA_COVER);
    draw::triangle(layer, (float)kCarX, (float)(kCarY - 9), (float)(kCarX - 6), (float)(kCarY + 6),
                   (float)(kCarX + 6), (float)(kCarY + 6), lv_color_white(), LV_OPA_COVER);
}

void NavView::draw_hud(lv_layer_t* layer) {
    // Instruction card.
    draw::rect(layer, 10, 15, 220, 54, 14, lv_color_black(), LV_OPA_20);
    draw::rect(layer, 10, 12, 220, 54, 14, palette_.panel, LV_OPA_90);
    draw::rect_stroke(layer, 10, 12, 220, 54, 14, 1, palette_.panel_edge, LV_OPA_80);
    draw::rect(layer, 18, 20, 36, 36, 10, palette_.route, LV_OPA_20);
    draw::turn_arrow(layer, 36, 38, 12, next_turn_dir_, palette_.route);

    draw::text(layer, turn_dist_, &font_ui_20, palette_.panel_text, 64, 16, draw::Align::Left, 0);
    draw::text(layer, turn_street_, &font_ui_10, palette_.panel_muted, 64, 42, draw::Align::Left,
               0, LV_OPA_90);
    draw::text(layer, "SIM", &font_ui_10, palette_.panel_muted, 222, 17, draw::Align::Right, 1,
               LV_OPA_70);

    // Compass.
    draw::circle(layer, 292, 84, 18, palette_.panel, LV_OPA_80);
    draw::rect_stroke(layer, 274, 66, 36, 36, 18, 1, palette_.panel_edge, LV_OPA_80);
    const float rad = heading_ * kDegToRad;
    const float nx = -std::sin(rad);
    const float ny = -std::cos(rad);
    draw::line(layer, 292, 84, 292 + (int)std::lround(nx * 11.0f),
               84 + (int)std::lround(ny * 11.0f), 3, palette_.warn, LV_OPA_COVER, true);
    draw::circle(layer, 292, 84, 2, palette_.panel_text, LV_OPA_COVER);
    draw::text_in(layer, "N", &font_ui_10, palette_.panel_text,
                  292 + (int)std::lround(nx * 12.0f), 84 + (int)std::lround(ny * 12.0f),
                  draw::Align::Center, 0, LV_OPA_90);

    draw::rect(layer, 10, 70, 58, 16, 8, palette_.panel, LV_OPA_70);
    draw::text(layer, "SIM NAV", &font_ui_10, palette_.panel_muted, 16, 73, draw::Align::Left, 1,
               LV_OPA_90);

    // Bottom status bar.
    draw::rect(layer, 8, 194, 304, 38, 13, palette_.panel, LV_OPA_90);
    draw::rect_stroke(layer, 8, 194, 304, 38, 13, 1, palette_.panel_edge, LV_OPA_80);
    draw::line(layer, 110, 201, 110, 225, 1, palette_.panel_edge, LV_OPA_80, false);
    draw::line(layer, 210, 201, 210, 225, 1, palette_.panel_edge, LV_OPA_80, false);

    draw::text_in(layer, "DIST", &font_ui_10, palette_.panel_muted, 60, 202, draw::Align::Center,
                  1, LV_OPA_80);
    draw::text_in(layer, remain_dist_, &font_ui_16, palette_.panel_text, 60, 217,
                  draw::Align::Center, 0);

    draw::text_in(layer, "TIME", &font_ui_10, palette_.panel_muted, 160, 202, draw::Align::Center,
                  1, LV_OPA_80);
    draw::text_in(layer, remain_time_, &font_ui_16, palette_.panel_text, 160, 217,
                  draw::Align::Center, 0);

    draw::text_in(layer, "ETA", &font_ui_10, palette_.panel_muted, 260, 202, draw::Align::Center, 1,
                  LV_OPA_80);
    draw::text_in(layer, eta_, &font_ui_16, palette_.panel_text, 260, 217, draw::Align::Center, 0);

    if (perf_.visible) {
        if (perf_.hw_valid) {
            draw::rect(layer, 140, 132, 178, 54, 10, lv_color_black(), LV_OPA_20);
            draw::rect(layer, 140, 130, 178, 54, 10, palette_.panel, LV_OPA_80);
            draw::rect_stroke(layer, 140, 130, 178, 54, 10, 1, palette_.panel_edge, LV_OPA_70);
            draw::text(layer, hw_a_, &font_ui_10, palette_.panel_text, 148, 136,
                       draw::Align::Left, 0, LV_OPA_90);
            draw::text(layer, hw_b_, &font_ui_10, palette_.panel_muted, 148, 149,
                       draw::Align::Left, 0, LV_OPA_90);
            draw::text(layer, hw_c_, &font_ui_10, palette_.panel_muted, 148, 162,
                       draw::Align::Left, 0, LV_OPA_90);
        } else {
            draw::rect(layer, 202, 176, 110, 14, 7, palette_.panel, LV_OPA_70);
            draw::text(layer, fps_, &font_ui_10, palette_.panel_muted, 306, 178,
                       draw::Align::Right, 0, LV_OPA_80);
        }
    }
}

} // namespace moto
