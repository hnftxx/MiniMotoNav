#include <cassert>
#include <cmath>
#include <cstdio>

#include "app/drag_session.h"

namespace {

bool near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

void test_countdown() {
    moto::DragSession drag;
    drag.setCountdownMs(3000);
    assert(drag.arm(0));
    assert(drag.phase() == moto::DragPhase::Countdown);

    drag.tick(1000000ULL, 0.0f, true);
    assert(drag.countdown_digit() == 2);
    drag.tick(2999999ULL, 0.0f, true);
    assert(drag.phase() == moto::DragPhase::Countdown);

    drag.tick(3000000ULL, 0.0f, true);
    assert(drag.phase() == moto::DragPhase::Running);
    assert(near(drag.elapsed_s(), 0.0f, 0.0001f));
}

void test_timer_is_monotonic_and_independent_of_frame_rate() {
    moto::DragSession drag;
    drag.setCountdownMs(0);
    assert(drag.arm(1000000ULL));
    assert(drag.phase() == moto::DragPhase::Running);

    drag.tick(3500000ULL, 0.0f, true);
    assert(near(drag.elapsed_s(), 2.5f, 0.0001f));
    drag.tick(9000000ULL, 0.0f, true);
    assert(near(drag.elapsed_s(), 8.0f, 0.0001f));
    assert(near(drag.distance_m(), 0.0f, 0.0001f));
}

void test_finish_at_exact_distance() {
    moto::DragSession drag;
    drag.setTargetDistance(100.0f);
    drag.setCountdownMs(0);
    drag.arm(0);

    for (int i = 1; i <= 9; ++i) {
        drag.tick((uint64_t)i * 1000000ULL, 36.0f, true);
        assert(drag.phase() == moto::DragPhase::Running);
    }
    drag.tick(10000000ULL, 36.0f, true);
    assert(drag.phase() == moto::DragPhase::Finished);
    assert(near(drag.result().elapsed_s, 10.0f, 0.001f));
    assert(near(drag.result().distance_m, 100.0f, 0.001f));
    assert(near(drag.result().avg_speed_kph, 36.0f, 0.01f));
    assert(near(drag.result().max_speed_kph, 36.0f, 0.01f));
}

void test_finish_interpolates_within_frame() {
    moto::DragSession drag;
    drag.setTargetDistance(15.0f);
    drag.setCountdownMs(0);
    drag.arm(0);
    drag.tick(1000000ULL, 72.0f, true);
    assert(drag.phase() == moto::DragPhase::Finished);
    assert(near(drag.result().elapsed_s, 0.75f, 0.001f));
}

void test_splits() {
    moto::DragSession drag;
    drag.setTargetDistance(2500.0f);
    drag.setSplitInterval(1000.0f);
    drag.setCountdownMs(0);
    drag.arm(0);

    for (int i = 1; i <= 100; ++i) drag.tick((uint64_t)i * 1000000ULL, 36.0f, true);
    assert(drag.splitCount() == 1);
    assert(near(drag.lastSplit()->distance_m, 1000.0f, 0.01f));
    assert(near(drag.lastSplit()->elapsed_s, 100.0f, 0.01f));

    for (int i = 101; i <= 250; ++i) drag.tick((uint64_t)i * 1000000ULL, 36.0f, true);
    assert(drag.phase() == moto::DragPhase::Finished);
    assert(drag.result().split_count == 2);
    assert(near(drag.result().splits[1].distance_m, 2000.0f, 0.01f));
    assert(near(drag.result().splits[1].elapsed_s, 200.0f, 0.01f));
}

void test_pause_resume() {
    moto::DragSession drag;
    drag.setCountdownMs(0);
    drag.arm(0);
    drag.tick(2000000ULL, 0.0f, true);
    assert(drag.pause(2000000ULL));
    drag.tick(4000000ULL, 0.0f, true);
    assert(near(drag.elapsed_s(), 2.0f, 0.001f));
    assert(drag.resume(4000000ULL));
    drag.tick(5000000ULL, 0.0f, true);
    assert(near(drag.elapsed_s(), 3.0f, 0.001f));
}

void test_invalid_gps_freezes_distance_not_clock() {
    moto::DragSession drag;
    drag.setTargetDistance(20.0f);
    drag.setCountdownMs(0);
    drag.arm(0);

    drag.tick(1000000ULL, 36.0f, true);
    assert(near(drag.distance_m(), 10.0f, 0.001f));
    drag.tick(2000000ULL, 36.0f, false);
    drag.tick(3000000ULL, 36.0f, false);
    assert(near(drag.distance_m(), 10.0f, 0.001f));
    assert(near(drag.elapsed_s(), 3.0f, 0.001f));
    assert(!drag.gpsReliable());

    drag.tick(4000000ULL, 36.0f, true);
    assert(drag.phase() == moto::DragPhase::Finished);
    assert(!drag.result().gps_reliable);
}

void test_acceleration_test_split() {
    moto::DragSession drag;
    drag.setTargetDistance(400.0f);
    drag.setAccelTest(moto::AccelTest::ZeroTo100);
    drag.setCountdownMs(0);
    drag.arm(0);

    drag.tick(1000000ULL, 50.0f, true);
    assert(!drag.result().accel_complete);
    drag.tick(2000000ULL, 110.0f, true);
    assert(drag.result().accel_complete || drag.accelComplete());
}

void test_personal_best_history() {
    moto::DragSession drag;
    drag.setCountdownMs(0);
    drag.setTargetDistance(100.0f);

    auto run = [&](float speed_kph) {
        drag.cancel();
        drag.arm(10000000ULL);
        const uint64_t start = 10000000ULL;
        for (int i = 1; i <= 240; ++i) {
            drag.tick(start + (uint64_t)i * 1000000ULL, speed_kph, true);
            if (drag.phase() == moto::DragPhase::Finished) break;
        }
        assert(drag.phase() == moto::DragPhase::Finished);
    };

    run(36.0f);
    assert(drag.newBest());
    const float first = drag.result().elapsed_s;
    run(45.0f);
    assert(drag.newBest());
    assert(drag.result().elapsed_s < first);
    assert(near(drag.personalBest()->elapsed_s, drag.result().elapsed_s, 0.001f));
    run(30.0f);
    assert(!drag.newBest());
    assert(drag.historyCount() == 3);
}

} // namespace

int main() {
    test_countdown();
    test_timer_is_monotonic_and_independent_of_frame_rate();
    test_finish_at_exact_distance();
    test_finish_interpolates_within_frame();
    test_splits();
    test_pause_resume();
    test_invalid_gps_freezes_distance_not_clock();
    test_acceleration_test_split();
    test_personal_best_history();
    std::printf("all drag tests passed\n");
    return 0;
}
