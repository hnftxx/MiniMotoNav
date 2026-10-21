#include "app/moto_telemetry.h"

#include <algorithm>
#include <cmath>

namespace moto {
namespace {

constexpr float kMaxSpeed = 160.0f;
constexpr float kMaxRpm = 9500.0f;
constexpr float kIdleRpm = 1300.0f;

float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

// Frame-rate independent exponential approach.
float approach(float current, float target, float tau, float dt) {
    if (tau <= 0.0001f) return target;
    const float k = 1.0f - std::exp(-dt / tau);
    return current + (target - current) * k;
}

float moveToward(float current, float target, float max_rate, float dt) {
    const float delta = target - current;
    const float step = max_rate * dt;
    if (std::fabs(delta) <= step) return target;
    return current + (delta > 0.0f ? step : -step);
}

} // namespace

void TelemetrySim::reset() {
    data_ = Telemetry{};
    time_s_ = 0.0f;
    smooth_target_ = 0.0f;
    drag_time_s_ = 0.0f;
    drag_mode_ = false;
    manual_speed_ = 78.0f;
    manual_throttle_ = 62.0f;
    manual_rpm_ = 5340.0f;
}

void TelemetrySim::setAutoDrive(bool automatic) {
    data_.auto_drive = automatic;
    if (!automatic) {
        manual_speed_ = data_.speed_kph;
        manual_throttle_ = data_.throttle;
        manual_rpm_ = data_.rpm;
    } else {
        time_s_ = 0.0f;
        smooth_target_ = data_.speed_kph;
    }
}

void TelemetrySim::setDragMode(bool enabled) {
    if (drag_mode_ == enabled) return;
    drag_mode_ = enabled;
    data_.auto_drive = false;
    if (enabled) {
        beginDragRun();
        data_.running = false;
    } else {
        time_s_ = 0.0f;
        smooth_target_ = data_.speed_kph;
    }
}

void TelemetrySim::beginDragRun() {
    drag_time_s_ = 0.0f;
    data_.speed_kph = 0.0f;
    data_.target_kph = 0.0f;
    data_.throttle = 0.0f;
    data_.rpm = kIdleRpm;
    data_.gear = 1;
    data_.engine_load = 0.0f;
}

void TelemetrySim::setManualSpeed(float kph) {
    manual_speed_ = clampf(kph, 0.0f, kMaxSpeed);
    data_.auto_drive = false;
}

void TelemetrySim::setManualThrottle(float percent) {
    manual_throttle_ = clampf(percent, 0.0f, 100.0f);
    data_.auto_drive = false;
}

void TelemetrySim::setManualRpm(float rpm) {
    manual_rpm_ = clampf(rpm, kIdleRpm, kMaxRpm);
    data_.auto_drive = false;
}

// A repeating city-drive profile: launch, cruise, brake, accelerate, high-speed cruise.
float TelemetrySim::scenarioTarget(float t) const {
    const float cycle = std::fmod(t, 124.0f);
    if (cycle < 7.0f) return 34.0f;
    if (cycle < 22.0f) return 78.0f;
    if (cycle < 31.0f) return 78.0f;
    if (cycle < 40.0f) return 48.0f;
    if (cycle < 49.0f) return 48.0f;
    if (cycle < 63.0f) return 96.0f;
    if (cycle < 74.0f) return 96.0f;
    if (cycle < 83.0f) return 62.0f;
    if (cycle < 92.0f) return 62.0f;
    if (cycle < 106.0f) return 122.0f;
    if (cycle < 116.0f) return 122.0f;
    return 70.0f;
}

void TelemetrySim::tick(float dt) {
    if (!data_.running) {
        data_.engine_load = approach(data_.engine_load, 0.0f, 0.35f, dt);
        return;
    }
    dt = clampf(dt, 0.0f, 0.1f);
    time_s_ += dt;

    if (drag_mode_) {
        updateDrag(dt);
    } else if (data_.auto_drive) {
        updateAuto(dt);
    } else {
        updateManual(dt);
    }

    data_.speed_kph = clampf(data_.speed_kph, 0.0f, kMaxSpeed);
    data_.trip_km += data_.speed_kph * dt / 3600.0f;

    const float speed_ratio = data_.speed_kph / kMaxSpeed;
    data_.engine_load = clampf(data_.throttle / 100.0f * 0.75f + speed_ratio * 0.25f, 0.0f, 1.0f);
}

void TelemetrySim::updateAuto(float dt) {
    const float profile = scenarioTarget(time_s_);
    // Move the target itself smoothly so every scenario change ramps instead of stepping.
    smooth_target_ = moveToward(smooth_target_, profile, 18.0f, dt);
    data_.target_kph = smooth_target_;

    const float error = smooth_target_ - data_.speed_kph;
    const float accel = clampf(error * 1.35f, -16.0f, 10.5f);
    data_.speed_kph += accel * dt;

    float wanted_throttle;
    if (error > 1.0f) {
        wanted_throttle = 16.0f + error * 2.4f + std::max(0.0f, accel) * 2.2f;
    } else if (error < -2.0f) {
        wanted_throttle = 3.0f;
    } else {
        wanted_throttle = 20.0f + data_.speed_kph * 0.11f;
    }
    wanted_throttle = clampf(wanted_throttle, 2.0f, 98.0f);
    data_.throttle = approach(data_.throttle, wanted_throttle, 0.09f, dt);

    // Six-speed gearbox: rpm follows the current gear's road-speed window.
    static const float kGearTop[] = {0.0f, 26.0f, 48.0f, 72.0f, 98.0f, 128.0f, 178.0f};
    int gear = 1;
    for (int i = 1; i <= 6; ++i) {
        if (data_.speed_kph <= kGearTop[i]) {
            gear = i;
            break;
        }
        gear = 6;
    }
    data_.gear = gear;

    float wanted_rpm;
    if (data_.speed_kph < 1.0f) {
        wanted_rpm = kIdleRpm + data_.throttle * 6.0f;
    } else {
        const float lo = kGearTop[gear - 1];
        const float hi = kGearTop[gear];
        const float within = clampf((data_.speed_kph - lo) / std::max(1.0f, hi - lo), 0.0f, 1.0f);
        wanted_rpm = 2100.0f + within * 6100.0f + data_.throttle * 7.0f;
    }
    data_.rpm = approach(data_.rpm, clampf(wanted_rpm, kIdleRpm, kMaxRpm), 0.10f, dt);
}

void TelemetrySim::updateDrag(float dt) {
    drag_time_s_ += dt;
    const float speed = clampf(data_.speed_kph, 0.0f, 176.0f);
    const float accel_mps2 = 7.4f * (1.0f - speed / 185.0f);
    data_.speed_kph = clampf(speed + accel_mps2 * 3.6f * dt, 0.0f, 176.0f);
    data_.target_kph = 176.0f;
    data_.throttle = approach(data_.throttle, 99.0f, 0.05f, dt);

    static const float kGearTop[] = {0.0f, 26.0f, 48.0f, 72.0f, 98.0f, 128.0f, 186.0f};
    int gear = 1;
    for (int i = 1; i <= 6; ++i) {
        if (data_.speed_kph <= kGearTop[i]) {
            gear = i;
            break;
        }
        gear = 6;
    }
    data_.gear = gear;

    const float lo = kGearTop[gear - 1];
    const float hi = kGearTop[gear];
    const float within = clampf((data_.speed_kph - lo) / std::max(1.0f, hi - lo), 0.0f, 1.0f);
    const float wanted_rpm = 2100.0f + within * 6800.0f + data_.throttle * 5.0f;
    data_.rpm = approach(data_.rpm, clampf(wanted_rpm, kIdleRpm, kMaxRpm), 0.08f, dt);
}

void TelemetrySim::updateManual(float dt) {
    data_.target_kph = manual_speed_;
    data_.speed_kph = approach(data_.speed_kph, manual_speed_, 0.16f, dt);
    data_.throttle = approach(data_.throttle, manual_throttle_, 0.07f, dt);
    data_.rpm = approach(data_.rpm, manual_rpm_, 0.07f, dt);

    static const float kGearTop[] = {0.0f, 26.0f, 48.0f, 72.0f, 98.0f, 128.0f, 178.0f};
    int gear = 1;
    for (int i = 1; i <= 6; ++i) {
        if (data_.speed_kph <= kGearTop[i]) {
            gear = i;
            break;
        }
        gear = 6;
    }
    data_.gear = gear;
}

} // namespace moto
