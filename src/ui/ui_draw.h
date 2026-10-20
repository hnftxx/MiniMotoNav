#pragma once

#include "lvgl.h"

#include "ui/fonts/fonts.h"

namespace moto {
namespace draw {

enum class Align { Left, Center, Right };

void text(lv_layer_t* layer, const char* txt, const lv_font_t* font, lv_color_t color,
          int32_t x, int32_t y, Align align = Align::Left, int32_t letter_space = 0,
          lv_opa_t opa = LV_OPA_COVER);

void text_in(lv_layer_t* layer, const char* txt, const lv_font_t* font, lv_color_t color,
             int32_t cx, int32_t cy, Align align = Align::Center, int32_t letter_space = 0,
             lv_opa_t opa = LV_OPA_COVER);

int32_t text_width(const char* txt, const lv_font_t* font, int32_t letter_space = 0);

void rect(lv_layer_t* layer, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius,
          lv_color_t color, lv_opa_t opa = LV_OPA_COVER);

void rect_stroke(lv_layer_t* layer, int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius,
                 int32_t border_width, lv_color_t color, lv_opa_t opa);

void circle(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t radius, lv_color_t color,
            lv_opa_t opa = LV_OPA_COVER);

void line(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t width,
          lv_color_t color, lv_opa_t opa = LV_OPA_COVER, bool rounded = true);

void arc(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t radius, int32_t width,
         float start_deg, float end_deg, lv_color_t color, lv_opa_t opa = LV_OPA_COVER,
         bool rounded = false);

void triangle(lv_layer_t* layer, float x1, float y1, float x2, float y2, float x3, float y3,
              lv_color_t color, lv_opa_t opa = LV_OPA_COVER);

void arc_gradient(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t radius, int32_t width,
                  float start_deg, float end_deg, lv_color_t c_from, lv_color_t c_to,
                  lv_opa_t opa = LV_OPA_COVER, int segments = 64);

void segment_bar(lv_layer_t* layer, int32_t x, int32_t y, int32_t w, int32_t h, int count,
                 float level, lv_color_t off_color, lv_color_t* colors, float phase,
                 bool shimmer);

void turn_arrow(lv_layer_t* layer, int32_t cx, int32_t cy, int size, int direction,
                lv_color_t color);

} // namespace draw
} // namespace moto
