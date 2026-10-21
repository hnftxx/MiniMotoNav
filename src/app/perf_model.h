#pragma once

#include <cstdint>

namespace moto {

enum class PerfProfile { Conservative, Balanced, Optimistic, Custom };

struct FrameWork {
    uint32_t pixels = 0;
    uint16_t rects = 0;
};

struct HardwareProfile {
    PerfProfile preset = PerfProfile::Balanced;
    uint32_t spi_hz = 40000000;
    uint16_t buffer_lines = 40;
    float cpu_us_per_pixel = 0.050f;
    float cpu_fixed_us = 800.0f;
    float flush_overhead_us = 40.0f;
    float jitter_ratio = 0.07f;
    bool dma = false;
    bool double_buffer = false;
};

struct HardwareEstimate {
    bool valid = false;
    uint32_t spi_hz = 0;
    uint32_t pixels = 0;
    uint16_t rects = 0;
    uint8_t flushes = 0;
    bool dma = false;
    float fps = 0.0f;
    float frame_ms = 0.0f;
    float spi_ms = 0.0f;
    float cpu_ms = 0.0f;
    float jitter_ms = 0.0f;
    float drop_pct = 0.0f;
};

const char* profile_name(PerfProfile profile);
HardwareProfile make_profile(PerfProfile profile);
HardwareEstimate estimate_frame(const HardwareProfile& profile, const FrameWork& work,
                                uint32_t frame_index = 0);

} // namespace moto
