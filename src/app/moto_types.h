#pragma once

#include <cstdint>

namespace moto {

enum class AppMode { Speedo, Nav, Drag, GpsClock };

struct PerfStats {
    float fps = 0.0f;
    float frame_ms = 0.0f;
    float render_ms = 0.0f;
    float worst_ms = 0.0f;
    float hw_fps = 0.0f;
    float hw_frame_ms = 0.0f;
    float hw_spi_ms = 0.0f;
    float hw_cpu_ms = 0.0f;
    float hw_jitter_ms = 0.0f;
    float hw_drop_pct = 0.0f;
    uint32_t dirty_pixels = 0;
    uint32_t spi_hz = 0;
    uint16_t dirty_rects = 0;
    bool hw_valid = false;
    bool hw_dma = false;
    bool hw_measured = false;
    bool visible = true;
};

struct Clock {
    int hour = 14;
    int minute = 32;

    void addMinutes(int m) {
        total += m;
        total %= (24 * 60);
        if (total < 0) total += 24 * 60;
        hour = total / 60;
        minute = total % 60;
    }

    int hour_minute() const { return hour * 60 + minute; }

    int total = 14 * 60 + 32;
};

} // namespace moto
