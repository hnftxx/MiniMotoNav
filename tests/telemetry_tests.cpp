#include <cassert>
#include <cmath>
#include <cstdio>

#include "app/moto_telemetry.h"

namespace {

bool nearly(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

void test_defaults() {
    moto::TelemetrySim sim;
    sim.reset();
    const moto::Telemetry& t = sim.data();
    assert(!t.running);
    assert(t.auto_drive);
    assert(t.speed_kph == 0.0f);
    assert(t.gear == 1);
    assert(t.trip_km == 0.0f);
}

void test_auto_drive_reaches_scenario() {
    moto::TelemetrySim sim;
    sim.reset();
    sim.setRunning(true);
    for (int i = 0; i < 60 * 12; ++i) sim.tick(1.0f / 60.0f);
    const moto::Telemetry& t = sim.data();
    assert(t.running);
    assert(t.auto_drive);
    assert(t.speed_kph > 20.0f);
    assert(t.speed_kph <= 160.0f);
    assert(t.throttle >= 0.0f && t.throttle <= 100.0f);
    assert(t.rpm >= 1300.0f && t.rpm <= 9500.0f);
    assert(t.gear >= 1 && t.gear <= 6);
    assert(t.trip_km > 0.0f);
    assert(std::isfinite(t.speed_kph));
    assert(std::isfinite(t.rpm));
    assert(std::isfinite(t.engine_load));
}

void test_manual_mode() {
    moto::TelemetrySim sim;
    sim.reset();
    sim.setRunning(true);
    sim.setManualSpeed(120.0f);
    sim.setManualThrottle(70.0f);
    sim.setManualRpm(6000.0f);
    for (int i = 0; i < 60 * 4; ++i) sim.tick(1.0f / 60.0f);
    const moto::Telemetry& t = sim.data();
    assert(!t.auto_drive);
    assert(nearly(t.speed_kph, 120.0f, 1.0f));
    assert(nearly(t.throttle, 70.0f, 1.0f));
    assert(nearly(t.rpm, 6000.0f, 50.0f));
    assert(t.gear == 5);
    assert(t.trip_km > 0.1f);
}

void test_pause_freezes_speed() {
    moto::TelemetrySim sim;
    sim.reset();
    sim.setRunning(true);
    for (int i = 0; i < 120; ++i) sim.tick(1.0f / 60.0f);
    const float speed = sim.data().speed_kph;
    sim.setRunning(false);
    for (int i = 0; i < 120; ++i) sim.tick(1.0f / 60.0f);
    assert(nearly(sim.data().speed_kph, speed, 0.0001f));
    assert(sim.data().engine_load < 1.0f);
}

void test_dt_clamp() {
    moto::TelemetrySim sim;
    sim.reset();
    sim.setRunning(true);
    sim.tick(10.0f);
    const moto::Telemetry& t = sim.data();
    assert(std::isfinite(t.speed_kph));
    assert(t.speed_kph <= 160.0f);
    assert(t.trip_km >= 0.0f && t.trip_km < 1.0f);
}

void test_deterministic() {
    moto::TelemetrySim a;
    moto::TelemetrySim b;
    a.reset();
    b.reset();
    a.setRunning(true);
    b.setRunning(true);
    for (int i = 0; i < 600; ++i) {
        a.tick(1.0f / 60.0f);
        b.tick(1.0f / 60.0f);
    }
    assert(nearly(a.data().speed_kph, b.data().speed_kph, 0.0001f));
    assert(nearly(a.data().rpm, b.data().rpm, 0.0001f));
    assert(a.data().gear == b.data().gear);
}

} // namespace

int main() {
    test_defaults();
    test_auto_drive_reaches_scenario();
    test_manual_mode();
    test_pause_freezes_speed();
    test_dt_clamp();
    test_deterministic();
    std::printf("all telemetry tests passed\n");
    return 0;
}
