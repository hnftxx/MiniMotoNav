#include "app/perf_model.h"

#include <algorithm>
#include <cmath>

namespace moto {
namespace {

constexpr float kFrameBudgetMs = 1000.0f / 60.0f;

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace

const char* profile_name(PerfProfile profile) {
    switch (profile) {
    case PerfProfile::Conservative: return "Conservative";
    case PerfProfile::Optimistic: return "Optimistic";
    case PerfProfile::Custom: return "Custom";
    case PerfProfile::Balanced:
    default: return "Balanced";
    }
}

HardwareProfile make_profile(PerfProfile profile) {
    HardwareProfile p;
    p.preset = profile;
    switch (profile) {
    case PerfProfile::Conservative:
        p.spi_hz = 20000000;
        p.cpu_us_per_pixel = 0.060f;
        p.cpu_fixed_us = 1100.0f;
        p.flush_overhead_us = 60.0f;
        p.jitter_ratio = 0.10f;
        break;
    case PerfProfile::Optimistic:
        p.spi_hz = 80000000;
        p.cpu_us_per_pixel = 0.045f;
        p.cpu_fixed_us = 650.0f;
        p.flush_overhead_us = 25.0f;
        p.jitter_ratio = 0.04f;
        break;
    case PerfProfile::Custom:
    case PerfProfile::Balanced:
    default:
        p.spi_hz = 40000000;
        p.cpu_us_per_pixel = 0.050f;
        p.cpu_fixed_us = 800.0f;
        p.flush_overhead_us = 40.0f;
        p.jitter_ratio = 0.07f;
        break;
    }
    return p;
}

HardwareEstimate estimate_frame(const HardwareProfile& profile, const FrameWork& work,
                                uint32_t frame_index) {
    HardwareEstimate out;
    out.spi_hz = std::max<uint32_t>(1000000u, profile.spi_hz);
    out.pixels = work.pixels;
    out.rects = work.rects;
    out.dma = profile.dma && profile.double_buffer;
    if (work.pixels == 0) return out;

    const uint32_t buffer_pixels =
        std::max<uint32_t>(1u, 320u * std::max<uint16_t>(1, profile.buffer_lines));
    const uint32_t chunks = (work.pixels + buffer_pixels - 1u) / buffer_pixels;
    const uint32_t flushes = std::max<uint32_t>(std::max<uint16_t>(1, work.rects), chunks);
    out.flushes = (uint8_t)std::min<uint32_t>(255u, flushes);

    const double spi_us = (double)work.pixels * 16.0 * 1000000.0 / (double)out.spi_hz;
    const double overhead_us = (double)flushes * (double)profile.flush_overhead_us;
    const double transfer_us = spi_us + overhead_us;
    const double cpu_us = (double)profile.cpu_fixed_us +
                          (double)work.pixels * (double)profile.cpu_us_per_pixel;
    const double work_us =
        out.dma ? std::max(transfer_us, cpu_us) : transfer_us + cpu_us;

    const double wave = std::sin((double)frame_index * 2.39996322973);
    const double jitter_us = work_us * (double)profile.jitter_ratio * 0.35 * wave;
    const double sample_us = std::max(1.0, work_us + jitter_us);

    out.valid = true;
    out.frame_ms = (float)(sample_us / 1000.0);
    out.spi_ms = (float)(transfer_us / 1000.0);
    out.cpu_ms = (float)(cpu_us / 1000.0);
    out.jitter_ms = (float)(std::fabs(jitter_us) / 1000.0);
    out.fps = clampf((float)(1000000.0 / sample_us), 0.0f, 60.0f);
    out.drop_pct = sample_us > (double)kFrameBudgetMs * 1000.0
                       ? (float)((1.0 - ((double)kFrameBudgetMs * 1000.0 / sample_us)) * 100.0)
                       : 0.0f;
    return out;
}

} // namespace moto
