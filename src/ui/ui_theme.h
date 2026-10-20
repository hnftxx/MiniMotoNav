#pragma once

#include "lvgl.h"

namespace moto {

enum class Theme { Day, Night, Race };

struct Palette {
    lv_color_t bg_top;
    lv_color_t bg_bottom;
    lv_color_t ink;
    lv_color_t ink_muted;
    lv_color_t card;
    lv_color_t card_border;
    lv_color_t card_text;
    lv_color_t pill;
    lv_color_t pill_text;
    lv_color_t gauge_track;
    lv_color_t gauge_track_edge;
    lv_color_t tick;
    lv_color_t accent_a;
    lv_color_t accent_b;
    lv_color_t accent_c;
    lv_color_t panel;
    lv_color_t panel_edge;
    lv_color_t panel_text;
    lv_color_t panel_muted;
    lv_color_t seg_off;
    lv_color_t good;
    lv_color_t warn;
    lv_color_t map_land;
    lv_color_t map_road;
    lv_color_t map_case;
    lv_color_t map_park;
    lv_color_t map_water;
    lv_color_t map_block;
    lv_color_t route;
    lv_color_t route_case;
    lv_color_t route_core;
};

inline Palette day_palette() {
    Palette p{};
    p.bg_top = lv_color_hex(0xEAF8FF);
    p.bg_bottom = lv_color_hex(0xB7DCF2);
    p.ink = lv_color_hex(0x061320);
    p.ink_muted = lv_color_hex(0x47657A);
    p.card = lv_color_hex(0xFFFFFF);
    p.card_border = lv_color_hex(0xFFFFFF);
    p.card_text = lv_color_hex(0x061320);
    p.pill = lv_color_hex(0xFFFFFF);
    p.pill_text = lv_color_hex(0x0B2A3E);
    p.gauge_track = lv_color_hex(0xFFFFFF);
    p.gauge_track_edge = lv_color_hex(0x0C2D45);
    p.tick = lv_color_hex(0x0A1B2C);
    p.accent_a = lv_color_hex(0x7A2BFF);
    p.accent_b = lv_color_hex(0x00D5FF);
    p.accent_c = lv_color_hex(0xFF3E9D);
    p.panel = lv_color_hex(0x0A1522);
    p.panel_edge = lv_color_hex(0x183650);
    p.panel_text = lv_color_hex(0xEBF9FF);
    p.panel_muted = lv_color_hex(0x8FB6CC);
    p.seg_off = lv_color_hex(0x1B2C3E);
    p.good = lv_color_hex(0x14C46A);
    p.warn = lv_color_hex(0xFF5A36);
    p.map_land = lv_color_hex(0xE9F1E4);
    p.map_road = lv_color_hex(0xFCFEF9);
    p.map_case = lv_color_hex(0xC9D5CB);
    p.map_park = lv_color_hex(0xC4E6B8);
    p.map_water = lv_color_hex(0xB4DEF2);
    p.map_block = lv_color_hex(0xDDE8D7);
    p.route = lv_color_hex(0x1F6BFF);
    p.route_case = lv_color_hex(0x0A3C93);
    p.route_core = lv_color_hex(0xFFFFFF);
    return p;
}

inline Palette night_palette() {
    Palette p{};
    p.bg_top = lv_color_hex(0x0A1725);
    p.bg_bottom = lv_color_hex(0x03070D);
    p.ink = lv_color_hex(0xF5FBFF);
    p.ink_muted = lv_color_hex(0x8AA9BF);
    p.card = lv_color_hex(0x0B1724);
    p.card_border = lv_color_hex(0x24455F);
    p.card_text = lv_color_hex(0xEAF8FF);
    p.pill = lv_color_hex(0x0D1B2A);
    p.pill_text = lv_color_hex(0xCBEEFF);
    p.gauge_track = lv_color_hex(0x14243C);
    p.gauge_track_edge = lv_color_hex(0x06101C);
    p.tick = lv_color_hex(0xC7E2F2);
    p.accent_a = lv_color_hex(0xA35CFF);
    p.accent_b = lv_color_hex(0x22E6FF);
    p.accent_c = lv_color_hex(0xFF4D9F);
    p.panel = lv_color_hex(0x040A12);
    p.panel_edge = lv_color_hex(0x143047);
    p.panel_text = lv_color_hex(0xEBF9FF);
    p.panel_muted = lv_color_hex(0x8FB6CC);
    p.seg_off = lv_color_hex(0x152436);
    p.good = lv_color_hex(0x2BE07B);
    p.warn = lv_color_hex(0xFF5A36);
    p.map_land = lv_color_hex(0x0A1421);
    p.map_road = lv_color_hex(0x233A50);
    p.map_case = lv_color_hex(0x08111C);
    p.map_park = lv_color_hex(0x0B211C);
    p.map_water = lv_color_hex(0x071E34);
    p.map_block = lv_color_hex(0x101B28);
    p.route = lv_color_hex(0x2F85FF);
    p.route_case = lv_color_hex(0x0A2E6E);
    p.route_core = lv_color_hex(0xBFE3FF);
    return p;
}

inline Palette racing_palette() {
    Palette p{};
    p.bg_top = lv_color_hex(0x111114);
    p.bg_bottom = lv_color_hex(0x050506);
    p.ink = lv_color_hex(0xF6F6F8);
    p.ink_muted = lv_color_hex(0x9A9AA2);
    p.card = lv_color_hex(0x141417);
    p.card_border = lv_color_hex(0x323238);
    p.card_text = lv_color_hex(0xF6F6F8);
    p.pill = lv_color_hex(0x121215);
    p.pill_text = lv_color_hex(0xF6F6F8);
    p.gauge_track = lv_color_hex(0x2A2A2F);
    p.gauge_track_edge = lv_color_hex(0x08080A);
    p.tick = lv_color_hex(0xEDEDF0);
    p.accent_a = lv_color_hex(0xFFFFFF);
    p.accent_b = lv_color_hex(0xE10600);
    p.accent_c = lv_color_hex(0xFF2D2D);
    p.panel = lv_color_hex(0x0A0A0C);
    p.panel_edge = lv_color_hex(0x303036);
    p.panel_text = lv_color_hex(0xF6F6F8);
    p.panel_muted = lv_color_hex(0x9A9AA2);
    p.seg_off = lv_color_hex(0x222226);
    p.good = lv_color_hex(0x22C55E);
    p.warn = lv_color_hex(0xE10600);
    p.map_land = lv_color_hex(0x0B0B0D);
    p.map_road = lv_color_hex(0x2A2A2F);
    p.map_case = lv_color_hex(0x060607);
    p.map_park = lv_color_hex(0x102016);
    p.map_water = lv_color_hex(0x0A1420);
    p.map_block = lv_color_hex(0x141417);
    p.route = lv_color_hex(0xE10600);
    p.route_case = lv_color_hex(0x4A0200);
    p.route_core = lv_color_hex(0xFFFFFF);
    return p;
}

inline Palette palette_for_theme(Theme theme) {
    switch (theme) {
    case Theme::Night: return night_palette();
    case Theme::Race: return racing_palette();
    case Theme::Day:
    default: return day_palette();
    }
}

inline Theme next_theme(Theme theme) {
    switch (theme) {
    case Theme::Day: return Theme::Night;
    case Theme::Night: return Theme::Race;
    case Theme::Race:
    default: return Theme::Day;
    }
}

inline const char* theme_button_label(Theme theme) {
    switch (next_theme(theme)) {
    case Theme::Night: return "NIGHT";
    case Theme::Race: return "RACE";
    case Theme::Day:
    default: return "DAY";
    }
}

inline lv_color_t mix_color(lv_color_t a, lv_color_t b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    const uint8_t r = (uint8_t)(a.red + (b.red - a.red) * t);
    const uint8_t g = (uint8_t)(a.green + (b.green - a.green) * t);
    const uint8_t bl = (uint8_t)(a.blue + (b.blue - a.blue) * t);
    return lv_color_make(r, g, bl);
}

inline Palette mix_palette(const Palette& a, const Palette& b, float t) {
    Palette p;
    p.bg_top = mix_color(a.bg_top, b.bg_top, t);
    p.bg_bottom = mix_color(a.bg_bottom, b.bg_bottom, t);
    p.ink = mix_color(a.ink, b.ink, t);
    p.ink_muted = mix_color(a.ink_muted, b.ink_muted, t);
    p.card = mix_color(a.card, b.card, t);
    p.card_border = mix_color(a.card_border, b.card_border, t);
    p.card_text = mix_color(a.card_text, b.card_text, t);
    p.pill = mix_color(a.pill, b.pill, t);
    p.pill_text = mix_color(a.pill_text, b.pill_text, t);
    p.gauge_track = mix_color(a.gauge_track, b.gauge_track, t);
    p.gauge_track_edge = mix_color(a.gauge_track_edge, b.gauge_track_edge, t);
    p.tick = mix_color(a.tick, b.tick, t);
    p.accent_a = mix_color(a.accent_a, b.accent_a, t);
    p.accent_b = mix_color(a.accent_b, b.accent_b, t);
    p.accent_c = mix_color(a.accent_c, b.accent_c, t);
    p.panel = mix_color(a.panel, b.panel, t);
    p.panel_edge = mix_color(a.panel_edge, b.panel_edge, t);
    p.panel_text = mix_color(a.panel_text, b.panel_text, t);
    p.panel_muted = mix_color(a.panel_muted, b.panel_muted, t);
    p.seg_off = mix_color(a.seg_off, b.seg_off, t);
    p.good = mix_color(a.good, b.good, t);
    p.warn = mix_color(a.warn, b.warn, t);
    p.map_land = mix_color(a.map_land, b.map_land, t);
    p.map_road = mix_color(a.map_road, b.map_road, t);
    p.map_case = mix_color(a.map_case, b.map_case, t);
    p.map_park = mix_color(a.map_park, b.map_park, t);
    p.map_water = mix_color(a.map_water, b.map_water, t);
    p.map_block = mix_color(a.map_block, b.map_block, t);
    p.route = mix_color(a.route, b.route, t);
    p.route_case = mix_color(a.route_case, b.route_case, t);
    p.route_core = mix_color(a.route_core, b.route_core, t);
    return p;
}

} // namespace moto
