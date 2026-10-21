// ESP32-S3 target entry point. Not part of the simulator build.
#if defined(ARDUINO)

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_timer.h>
#include <cctype>

#include "lvgl.h"

#include "app/moto_app.h"
#include "boot/boot_player.h"
#include "platform/ble_server.h"

namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 240;
constexpr int kBufferLines = 40;
constexpr uint64_t kStatsPeriodUs = 1000000;

TFT_eSPI tft;
moto::MotoApp app;
moto::platform::BleServer ble;
lv_display_t* display = nullptr;

alignas(4) uint8_t draw_buf[kWidth * kBufferLines * 2];

uint32_t refresh_count = 0;
uint64_t refresh_start_us = 0;
uint64_t render_accum_us = 0;
uint32_t worst_render_us = 0;
uint64_t stats_start_us = 0;
uint64_t flush_accum_us = 0;
uint64_t flush_pixels = 0;
uint32_t flush_rects = 0;

char line_buf[64] = {};
size_t line_len = 0;
uint32_t line_last_ms = 0;

void print_help() {
    Serial.println("[cmd] mode speedo|nav|drag|gps  theme race|day|night  speed N  throttle N");
    Serial.println("[cmd] rpm N  target M  split M  countdown MS  drag  dragreset  reset  fps");
    Serial.println("[cmd] next  pause  auto  controls  help");
    Serial.println("keys: n=mode t=theme f=fps p=pause d=drag r=reset ?=help");
}

uint32_t tick_cb() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    const uint32_t start_us = (uint32_t)esp_timer_get_time();
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(reinterpret_cast<uint16_t*>(px_map), w * h, true);
    tft.endWrite();
    flush_accum_us += (uint32_t)esp_timer_get_time() - start_us;
    flush_pixels += (uint64_t)w * (uint64_t)h;
    flush_rects++;
    lv_display_flush_ready(disp);
}

void display_event_cb(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_REFR_START) {
        refresh_start_us = (uint64_t)esp_timer_get_time();
    } else if (code == LV_EVENT_REFR_READY && refresh_start_us != 0) {
        const uint64_t now = (uint64_t)esp_timer_get_time();
        const uint32_t elapsed = (uint32_t)(now - refresh_start_us);
        render_accum_us += elapsed;
        if (elapsed > worst_render_us) worst_render_us = elapsed;
        ++refresh_count;
    }
}

void update_stats() {
    const uint64_t now = (uint64_t)esp_timer_get_time();
    if (stats_start_us == 0) {
        stats_start_us = now;
        return;
    }

    const uint64_t window_us = now - stats_start_us;
    if (window_us < kStatsPeriodUs) return;

    if (refresh_count > 0) {
        const float fps = (float)refresh_count * 1000000.0f / (float)window_us;
        const float frame_ms = 1000.0f / fps;
        const float render_ms = (float)render_accum_us / (float)refresh_count / 1000.0f;
        const float spi_ms = flush_rects > 0
                                 ? (float)flush_accum_us / (float)flush_rects / 1000.0f
                                 : 0.0f;
        const uint32_t avg_pixels =
            (uint32_t)(flush_pixels / (uint64_t)refresh_count);
        const uint16_t avg_rects =
            (uint16_t)(flush_rects / refresh_count);
        app.setPerf(fps, frame_ms, render_ms, (float)worst_render_us / 1000.0f);
        app.setHardwareMeasured(fps, frame_ms, SPI_FREQUENCY, spi_ms, avg_pixels, avg_rects);
        Serial.printf("[stats] fps=%.1f frame=%.1fms render=%.1fms spi=%.1fms dirty=%u/%u "
                      "worst=%.1fms heap=%u\n",
                      fps, frame_ms, render_ms, spi_ms, (unsigned)avg_pixels,
                      (unsigned)avg_rects, (float)worst_render_us / 1000.0f,
                      (unsigned)ESP.getFreeHeap());
    } else {
        Serial.printf("[stats] no LVGL refresh in %llu ms\n", (unsigned long long)(window_us / 1000));
    }

    refresh_count = 0;
    render_accum_us = 0;
    worst_render_us = 0;
    flush_accum_us = 0;
    flush_pixels = 0;
    flush_rects = 0;
    stats_start_us = now;
}

void dispatch_line() {
    while (line_len > 0 && std::isspace((unsigned char)line_buf[line_len - 1])) --line_len;
    line_buf[line_len] = '\0';
    if (line_len == 0) return;

    if (line_len == 1) {
        const char c = line_buf[0];
        moto::Command shortcut;
        if (c == 't') {
            shortcut.type = moto::CommandType::NextTheme;
            app.postCommand(shortcut);
            line_len = 0;
            return;
        }
        if (c == 'd') {
            shortcut.type = moto::CommandType::DragRun;
            app.postCommand(shortcut);
            line_len = 0;
            return;
        }
        if (c == '?') {
            print_help();
            line_len = 0;
            return;
        }
    }

    moto::Command cmd;
    if (moto::parse_command(line_buf, cmd)) {
        app.postCommand(cmd);
    } else {
        Serial.printf("[cmd] unknown: %s\n", line_buf);
    }
    line_len = 0;
}

void poll_serial() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        const uint32_t now_ms = millis();
        if (c == '\r' || c == '\n') {
            dispatch_line();
            continue;
        }
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (line_len < sizeof(line_buf) - 1) {
            line_buf[line_len++] = c;
            line_last_ms = now_ms;
        }
    }

    if (line_len == 1 && (uint32_t)(millis() - line_last_ms) >= 250) {
        dispatch_line();
    }
}

void push_boot_rect(const uint16_t* framebuffer, moto::boot::Rect rect) {
    if (rect.empty()) return;
    tft.startWrite();
    tft.setAddrWindow(rect.x, rect.y, rect.w, rect.h);
    for (uint16_t row = 0; row < rect.h; ++row) {
        const uint16_t* source =
            framebuffer + (uint32_t)(rect.y + row) * moto::boot::kWidth + rect.x;
        tft.pushColors(const_cast<uint16_t*>(source), rect.w, true);
    }
    tft.endWrite();
}

bool play_boot_animation() {
    uint16_t* framebuffer =
        static_cast<uint16_t*>(ps_malloc(moto::boot::Player::memoryBytes()));
    if (framebuffer == nullptr) {
        framebuffer = static_cast<uint16_t*>(malloc(moto::boot::Player::memoryBytes()));
    }
    if (framebuffer == nullptr) {
        Serial.println("[boot] no framebuffer, skipping animation");
        return false;
    }

    moto::boot::Player player;
    if (!player.begin(framebuffer, moto::boot::kWidth * moto::boot::kHeight) ||
        !player.reset()) {
        Serial.println("[boot] animation decode failed");
        free(framebuffer);
        return false;
    }

    Serial.printf("[boot] frames=%u ticks=%u fallback=%s\n",
                  (unsigned)player.frameCount(), (unsigned)player.tickCount(),
                  player.fallback() ? "yes" : "no");
    push_boot_rect(framebuffer, player.dirty());

    uint32_t last_us = (uint32_t)esp_timer_get_time();
    while (!player.finished()) {
        const uint32_t now_us = (uint32_t)esp_timer_get_time();
        float dt = (float)(now_us - last_us) / 1000000.0f;
        last_us = now_us;
        if (dt > 0.1f) dt = 0.1f;

        if (player.update(dt)) push_boot_rect(framebuffer, player.dirty());
        delay(1);
    }

    free(framebuffer);
    return true;
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(150);
    Serial.printf("GODSPEED boot: chip=%s rev=%u flash=%uMB psram=%uMB heap=%u\n",
                  ESP.getChipModel(), (unsigned)ESP.getChipRevision(),
                  (unsigned)(ESP.getFlashChipSize() / (1024U * 1024U)),
                  (unsigned)(ESP.getPsramSize() / (1024U * 1024U)),
                  (unsigned)ESP.getFreeHeap());

#if TFT_BL >= 0
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON == HIGH ? LOW : HIGH);
#endif
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
#if TFT_BL >= 0
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif

    lv_init();
    lv_tick_set_cb(tick_cb);

    display = lv_display_create(kWidth, kHeight);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, flush_cb);
    lv_display_set_buffers(display, draw_buf, nullptr, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_add_event_cb(display, display_event_cb, LV_EVENT_REFR_START, nullptr);
    lv_display_add_event_cb(display, display_event_cb, LV_EVENT_REFR_READY, nullptr);

    app.init();
    play_boot_animation();
    ble.begin();

    Serial.println("GODSPEED ready: 320x240 ILI9341");
    print_help();
}

void loop() {
    static uint32_t last_us = 0;
    const uint32_t now_us = (uint32_t)esp_timer_get_time();
    if (last_us == 0) last_us = now_us;
    float dt = (float)(now_us - last_us) / 1000000.0f;
    last_us = now_us;
    if (dt > 0.1f) dt = 0.1f;

    poll_serial();

    bool linked = false;
    if (ble.pollLinkChange(linked)) {
        app.setBleLinked(linked, millis());
    }
    moto::platform::BleIncoming incoming;
    while (ble.poll(incoming)) {
        if (incoming.kind == moto::platform::BleIncoming::Kind::kTelemetry) {
            app.applyBleTelemetry(incoming.telemetry, millis());
        } else {
            app.applyBleNavigation(incoming.navigation, millis());
        }
    }
    app.tickBle(millis());

    app.update(dt);
    lv_timer_handler();
    update_stats();
    delay(2);
}

#endif
