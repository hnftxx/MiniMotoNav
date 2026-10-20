// ESP32-S3 target entry point. Not part of the simulator build.
#if defined(ARDUINO)

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_timer.h>

#include "lvgl.h"

#include "app/moto_app.h"

namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 240;
constexpr int kBufferLines = 40;

TFT_eSPI tft;
moto::MotoApp app;
lv_display_t* display = nullptr;

alignas(4) uint8_t draw_buf[kWidth * kBufferLines * 2];

uint32_t tick_cb() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(reinterpret_cast<uint16_t*>(px_map), w * h, true);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(150);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    lv_init();
    lv_tick_set_cb(tick_cb);

    display = lv_display_create(kWidth, kHeight);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, flush_cb);
    lv_display_set_buffers(display, draw_buf, nullptr, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    app.init();

    Serial.println("MiniMotoNav ready: 320x240 ILI9341");
}

void loop() {
    static uint32_t last_us = 0;
    const uint32_t now_us = (uint32_t)esp_timer_get_time();
    if (last_us == 0) last_us = now_us;
    float dt = (float)(now_us - last_us) / 1000000.0f;
    last_us = now_us;
    if (dt > 0.1f) dt = 0.1f;

    app.update(dt);
    lv_timer_handler();
    delay(2);
}

#endif
