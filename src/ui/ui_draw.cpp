#include "ui/ui_draw.h"
#include "ui/ui_theme.h"

#include <cmath>
#include <cstdio>

namespace moto {
namespace draw {
namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDegToRad = kPi / 180.0f;

} // namespace

int32_t text_width(const char* txt, const lv_font_t* font, int32_t letter_space) {
    if (txt == nullptr || font == nullptr) return 0;
    lv_point_t size{};
    lv_text_get_size(&size, txt, font, letter_space, 0, 320, LV_TEXT_FLAG_NONE);
    return size.x;
}

void text(lv_layer_t* layer, const char* txt, const lv_font_t* font, lv_color_t color,
          int32_t x, int32_t y, Align align, int32_t letter_space, lv_opa_t opa) {
    if (txt == nullptr || txt[0] == '\0') return;
    lv_point_t size{};
    lv_text_get_size(&size, txt, font, letter_space, 0, 320, LV_TEXT_FLAG_NONE);
    if (align == Align::Center) x -= size.x / 2;
    else if (align == Align::Right) x -= size.x;

    lv_area_t coords{x, y, x + size.x + 8, y + size.y + 6};
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.text = txt;
    dsc.font = font;
    dsc.color = color;
    dsc.letter_space = letter_space;
    dsc.opa = opa;
    lv_draw_label(layer, &dsc, &coords);
}

void text_in(lv_layer_t* layer, const char* txt, const lv_font_t* font, lv_color_t color,
             int32_t cx, int32_t cy, Align align, int32_t letter_space, lv_opa_t opa) {
    if (txt == nullptr || txt[0] == '\0') return;
    lv_point_t size{};
    lv_text_get_size(&size, txt, font, letter_space, 0, 320, LV_TEXT_FLAG_NONE);
    text(layer, txt, font, color, cx, cy - size.y / 2, align, letter_space, opa);
}

void rect(lv_layer_t* layer, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius,
          lv_color_t color, lv_opa_t opa) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = opa;
    dsc.radius = radius;
    lv_area_t coords{x, y, x + w - 1, y + h - 1};
    lv_draw_rect(layer, &dsc, &coords);
}

void rect_stroke(lv_layer_t* layer, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius,
                 int32_t border_width, lv_color_t color, lv_opa_t opa) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_TRANSP;
    dsc.radius = radius;
    dsc.border_width = border_width;
    dsc.border_color = color;
    dsc.border_opa = opa;
    dsc.border_side = LV_BORDER_SIDE_FULL;
    lv_area_t coords{x, y, x + w - 1, y + h - 1};
    lv_draw_rect(layer, &dsc, &coords);
}

void circle(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t radius, lv_color_t color,
            lv_opa_t opa) {
    rect(layer, cx - radius, cy - radius, radius * 2, radius * 2, LV_RADIUS_CIRCLE, color, opa);
}

void line(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t width,
          lv_color_t color, lv_opa_t opa, bool rounded) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.p1.x = (float)x1;
    dsc.p1.y = (float)y1;
    dsc.p2.x = (float)x2;
    dsc.p2.y = (float)y2;
    dsc.color = color;
    dsc.width = width;
    dsc.opa = opa;
    dsc.round_start = rounded;
    dsc.round_end = rounded;
    lv_draw_line(layer, &dsc);
}

void arc(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t radius, int32_t width,
         float start_deg, float end_deg, lv_color_t color, lv_opa_t opa, bool rounded) {
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.center.x = cx;
    dsc.center.y = cy;
    dsc.radius = (uint16_t)radius;
    dsc.width = width;
    dsc.start_angle = start_deg;
    dsc.end_angle = end_deg;
    dsc.color = color;
    dsc.opa = opa;
    dsc.rounded = rounded ? 1 : 0;
    lv_draw_arc(layer, &dsc);
}

void triangle(lv_layer_t* layer, float x1, float y1, float x2, float y2, float x3, float y3,
              lv_color_t color, lv_opa_t opa) {
    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.p[0].x = x1;
    dsc.p[0].y = y1;
    dsc.p[1].x = x2;
    dsc.p[1].y = y2;
    dsc.p[2].x = x3;
    dsc.p[2].y = y3;
    dsc.color = color;
    dsc.opa = opa;
    lv_draw_triangle(layer, &dsc);
}

void arc_gradient(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t radius, int32_t width,
                  float start_deg, float end_deg, lv_color_t c_from, lv_color_t c_to,
                  lv_opa_t opa, int segments) {
    if (end_deg <= start_deg || segments <= 0) return;
    const float sweep = end_deg - start_deg;
    const float step = sweep / (float)segments;
    for (int i = 0; i < segments; ++i) {
        const float a0 = start_deg + step * (float)i;
        const float a1 = a0 + step + 0.3f;
        const float t = (float)i / (float)(segments - 1);
        const lv_color_t c = mix_color(c_from, c_to, t);
        const float rad0 = a0 * 0.01745329252f;
        const float rad1 = a1 * 0.01745329252f;
        const int x0 = cx + (int)std::lround(std::cos(rad0) * (float)radius);
        const int y0 = cy + (int)std::lround(std::sin(rad0) * (float)radius);
        const int x1 = cx + (int)std::lround(std::cos(rad1) * (float)radius);
        const int y1 = cy + (int)std::lround(std::sin(rad1) * (float)radius);
        line(layer, x0, y0, x1, y1, width, c, opa, true);
    }
}

void segment_bar(lv_layer_t* layer, int32_t x, int32_t y, int32_t w, int32_t h, int count,
                 float level, lv_color_t off_color, lv_color_t* colors, float phase,
                 bool shimmer) {
    if (count <= 0) return;
    const int32_t gap = 2;
    const int32_t seg_w = (w - gap * (count - 1)) / count;
    if (seg_w <= 0) return;

    for (int i = 0; i < count; ++i) {
        const float seg_start = (float)i / (float)count;
        const float seg_end = (float)(i + 1) / (float)count;
        const float fill = level * (float)count - (float)i;
        lv_color_t c = off_color;
        lv_opa_t opa = LV_OPA_COVER;
        if (fill > 0.0f) {
            c = colors[i % 24];
            if (fill < 1.0f) {
                opa = (lv_opa_t)(255.0f * fill);
            }
            if (shimmer) {
                const float wave = 0.5f + 0.5f * std::sin(phase * 3.0f - (float)i * 0.55f);
                c = mix_color(c, lv_color_white(), 0.10f + wave * 0.16f);
            }
        }
        (void)seg_start;
        (void)seg_end;
        rect(layer, x + i * (seg_w + gap), y, seg_w, h, 2, c, opa);
    }
}

void turn_arrow(lv_layer_t* layer, int32_t cx, int32_t cy, int size, int direction,
                lv_color_t color) {
    const int32_t s = size;
    if (direction == 0) {
        line(layer, cx, cy + s, cx, cy - s, 5, color, LV_OPA_COVER, true);
        triangle(layer, (float)cx, (float)(cy - s - 2), (float)(cx - 7), (float)(cy - s + 8),
                 (float)(cx + 7), (float)(cy - s + 8), color);
    } else {
        const int dir = direction > 0 ? 1 : -1;
        line(layer, cx, cy + s, cx, cy - s + 5, 5, color, LV_OPA_COVER, true);
        line(layer, cx, cy - s + 5, cx + dir * (s - 3), cy - s + 5, 5, color, LV_OPA_COVER, true);
        triangle(layer, (float)(cx + dir * (s + 4)), (float)(cy - s + 5),
                 (float)(cx + dir * (s - 6)), (float)(cy - s - 2),
                 (float)(cx + dir * (s - 6)), (float)(cy - s + 12), color);
    }
}

} // namespace draw
} // namespace moto
