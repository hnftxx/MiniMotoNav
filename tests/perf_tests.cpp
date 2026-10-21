#include "app/perf_model.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using namespace moto;

namespace {

bool near(float a, float b, float eps = 0.05f) {
    return std::fabs(a - b) <= eps;
}

void test_profiles() {
    const HardwareProfile c = make_profile(PerfProfile::Conservative);
    const HardwareProfile b = make_profile(PerfProfile::Balanced);
    const HardwareProfile o = make_profile(PerfProfile::Optimistic);
    assert(c.spi_hz == 20000000u);
    assert(b.spi_hz == 40000000u);
    assert(o.spi_hz == 80000000u);
    assert(c.jitter_ratio > b.jitter_ratio);
    assert(b.jitter_ratio > o.jitter_ratio);
    assert(std::string(profile_name(PerfProfile::Balanced)) == "Balanced");
}

void test_zero_work() {
    const HardwareEstimate e = estimate_frame(make_profile(PerfProfile::Balanced), {});
    assert(!e.valid);
    assert(e.fps == 0.0f);
    assert(e.flushes == 0);
}

void test_blocking_full_frame() {
    const HardwareProfile p = make_profile(PerfProfile::Balanced);
    const HardwareEstimate e = estimate_frame(p, {320u * 240u, 1}, 0);
    assert(e.valid);
    assert(e.flushes == 6);
    assert(near(e.spi_ms, 30.76f, 0.02f));
    assert(near(e.cpu_ms, 4.64f, 0.02f));
    assert(e.frame_ms > 35.5f && e.frame_ms < 35.7f);
    assert(e.fps > 28.0f && e.fps < 28.5f);
    assert(e.drop_pct > 50.0f);
}

void test_dma_overlap() {
    HardwareProfile p = make_profile(PerfProfile::Balanced);
    p.dma = true;
    p.double_buffer = true;
    const HardwareEstimate e = estimate_frame(p, {320u * 240u, 1}, 0);
    assert(e.dma);
    assert(e.frame_ms > 30.9f && e.frame_ms < 31.3f);
    assert(e.frame_ms < 35.5f);

    p.double_buffer = false;
    const HardwareEstimate no_db = estimate_frame(p, {320u * 240u, 1}, 0);
    assert(!no_db.dma);
    assert(no_db.frame_ms > 35.5f);
}

void test_dirty_area_scales() {
    const HardwareProfile p = make_profile(PerfProfile::Balanced);
    const HardwareEstimate full = estimate_frame(p, {320u * 240u, 1}, 0);
    const HardwareEstimate dirty = estimate_frame(p, {320u * 40u, 2}, 0);
    assert(dirty.valid);
    assert(dirty.frame_ms < full.frame_ms * 0.3f);
    assert(dirty.fps > full.fps);
}

void test_jitter_deterministic() {
    const HardwareProfile p = make_profile(PerfProfile::Conservative);
    const FrameWork w{320u * 40u, 2};
    const HardwareEstimate a = estimate_frame(p, w, 123);
    const HardwareEstimate b = estimate_frame(p, w, 123);
    const HardwareEstimate c = estimate_frame(p, w, 124);
    assert(a.frame_ms == b.frame_ms);
    assert(a.jitter_ms == b.jitter_ms);
    assert(std::fabs(a.frame_ms - c.frame_ms) > 0.0001f);
    assert(a.jitter_ms <= a.frame_ms * p.jitter_ratio);
}

} // namespace

int main() {
    test_profiles();
    test_zero_work();
    test_blocking_full_frame();
    test_dma_overlap();
    test_dirty_area_scales();
    test_jitter_deterministic();
    std::puts("perf_tests: ok");
    return 0;
}
