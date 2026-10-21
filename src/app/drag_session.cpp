#include "app/drag_session.h"

#include <algorithm>
#include <cmath>

namespace moto {
namespace {

constexpr float kMinDistance = 10.0f;
constexpr float kMaxDistance = 10000.0f;
constexpr float kMaxSpeed = 300.0f;
constexpr float kGpsUnreliableS = 0.5f;

float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

float target_speed(AccelTest test) {
    switch (test) {
    case AccelTest::ZeroTo60: return 60.0f;
    case AccelTest::ZeroTo100: return 100.0f;
    case AccelTest::None:
    default: return 0.0f;
    }
}

bool same_target(float a, float b) { return std::fabs(a - b) < 0.5f; }

} // namespace

void DragSession::resetSession() {
    phase_ = DragPhase::Idle;
    phase_start_us_ = 0;
    last_us_ = 0;
    countdown_us_ = 0;
    elapsed_s_ = 0.0f;
    distance_m_ = 0.0f;
    speed_kph_ = 0.0f;
    max_speed_kph_ = 0.0f;
    invalid_s_ = 0.0f;
    accel_time_s_ = 0.0f;
    gps_fix_ = true;
    accel_complete_ = false;
    new_best_ = false;
    split_count_ = 0;
    result_ = DragResult{};
}

void DragSession::clearHistory() {
    history_count_ = 0;
    result_ = DragResult{};
    new_best_ = false;
}

void DragSession::setTargetDistance(float metres) {
    settings_.target_distance_m = clampf(metres, kMinDistance, kMaxDistance);
}

void DragSession::setCountdownMs(uint32_t ms) {
    settings_.countdown_ms = std::min<uint32_t>(ms, 10000);
}

void DragSession::setSplitInterval(float metres) {
    settings_.split_interval_m = clampf(metres, 0.0f, 10000.0f);
}

void DragSession::setAccelTest(AccelTest test) { settings_.accel_test = test; }

bool DragSession::arm(uint64_t now_us) {
    if (phase_ == DragPhase::Running || phase_ == DragPhase::Paused) return false;
    resetSession();
    phase_ = DragPhase::Countdown;
    phase_start_us_ = now_us;
    last_us_ = now_us;
    countdown_us_ = (uint64_t)settings_.countdown_ms * 1000ULL;
    if (countdown_us_ == 0) beginRun(now_us);
    return true;
}

void DragSession::beginRun(uint64_t now_us) {
    phase_ = DragPhase::Running;
    phase_start_us_ = now_us;
    last_us_ = now_us;
    elapsed_s_ = 0.0f;
    distance_m_ = 0.0f;
    max_speed_kph_ = 0.0f;
    invalid_s_ = 0.0f;
    accel_time_s_ = 0.0f;
    gps_fix_ = true;
    accel_complete_ = false;
    split_count_ = 0;
    result_ = DragResult{};
}

bool DragSession::pause(uint64_t now_us) {
    if (phase_ != DragPhase::Running) return false;
    phase_ = DragPhase::Paused;
    last_us_ = now_us;
    return true;
}

bool DragSession::resume(uint64_t now_us) {
    if (phase_ != DragPhase::Paused) return false;
    phase_ = DragPhase::Running;
    last_us_ = now_us;
    return true;
}

void DragSession::cancel() { resetSession(); }

void DragSession::tick(uint64_t now_us, float speed_kph, bool gps_fix) {
    if (phase_ == DragPhase::Countdown) {
        last_us_ = now_us;
        if (now_us < phase_start_us_) return;
        const uint64_t elapsed_us = now_us - phase_start_us_;
        if (elapsed_us < countdown_us_) return;

        const uint64_t run_start_us = phase_start_us_ + countdown_us_;
        beginRun(run_start_us);
        last_us_ = now_us;
        elapsed_s_ = (float)((now_us - run_start_us) / 1000000.0);
        speed_kph_ = clampf(speed_kph, 0.0f, kMaxSpeed);
        gps_fix_ = gps_fix;
        return;
    }

    if (phase_ != DragPhase::Running) {
        if (phase_ != DragPhase::Paused) last_us_ = now_us;
        return;
    }

    if (now_us <= last_us_) return;
    const float dt = (float)((now_us - last_us_) / 1000000.0);
    last_us_ = now_us;
    if (!std::isfinite(dt) || dt <= 0.0f) return;

    integrate(dt, speed_kph, gps_fix);
}

void DragSession::integrate(float dt, float speed_kph, bool gps_fix) {
    const float prev_elapsed = elapsed_s_;
    const float prev_distance = distance_m_;
    const float prev_speed = speed_kph_;
    elapsed_s_ += dt;
    gps_fix_ = gps_fix;

    if (!gps_fix || !std::isfinite(speed_kph)) {
        invalid_s_ += dt;
        speed_kph_ = 0.0f;
        return;
    }

    const float speed = clampf(speed_kph, 0.0f, kMaxSpeed);
    speed_kph_ = speed;
    if (speed <= 0.0f) return;

    if (speed > max_speed_kph_) max_speed_kph_ = speed;

    const float threshold = target_speed(settings_.accel_test);
    if (!accel_complete_ && threshold > 0.0f && speed >= threshold) {
        float fraction = 1.0f;
        if (speed > prev_speed && prev_speed < threshold) {
            fraction = (threshold - prev_speed) / (speed - prev_speed);
        }
        accel_time_s_ = prev_elapsed + dt * clampf(fraction, 0.0f, 1.0f);
        accel_complete_ = true;
    }

    const float step_m = speed / 3.6f * dt;
    if (step_m <= 0.0f) return;

    const float target = settings_.target_distance_m;
    distance_m_ = std::min(target, distance_m_ + step_m);
    addSplits(prev_distance, distance_m_, prev_elapsed, dt);

    if (distance_m_ >= target - 0.0001f) {
        float fraction = 1.0f;
        if (step_m > 0.0001f) {
            fraction = (target - prev_distance) / step_m;
        }
        finish(prev_elapsed + dt * clampf(fraction, 0.0f, 1.0f));
    }
}

void DragSession::addSplits(float from_m, float to_m, float from_s, float dt) {
    const float interval = settings_.split_interval_m;
    if (interval <= 1.0f || to_m <= from_m) return;
    const float target = settings_.target_distance_m;

    float next = ((float)split_count_ + 1.0f) * interval;
    while (next < target - 0.01f && next <= to_m && split_count_ < 64) {
        if (next > from_m) {
            const float fraction = (next - from_m) / (to_m - from_m);
            DragSplit& split = splits_[split_count_++];
            split.distance_m = next;
            split.elapsed_s = from_s + dt * clampf(fraction, 0.0f, 1.0f);
        }
        next += interval;
    }
}

void DragSession::finish(float elapsed_s) {
    phase_ = DragPhase::Finished;
    elapsed_s_ = elapsed_s;
    distance_m_ = settings_.target_distance_m;
    result_ = DragResult{};
    result_.elapsed_s = elapsed_s;
    result_.target_distance_m = settings_.target_distance_m;
    result_.distance_m = distance_m_;
    result_.max_speed_kph = max_speed_kph_;
    result_.avg_speed_kph = elapsed_s > 0.0001f
                                ? settings_.target_distance_m / elapsed_s * 3.6f
                                : 0.0f;
    result_.accel_time_s = accel_time_s_;
    result_.gps_reliable = invalid_s_ < kGpsUnreliableS;
    result_.accel_complete = accel_complete_;
    result_.split_count = split_count_;
    for (uint32_t i = 0; i < split_count_; ++i) {
        result_.splits[i] = splits_[i];
    }
    submitResult();
}

void DragSession::submitResult() {
    const DragResult* best = personalBest();
    new_best_ = best == nullptr || result_.elapsed_s < best->elapsed_s;
    if (history_count_ < kMaxHistory) {
        history_[history_count_++] = result_;
        return;
    }
    for (size_t i = 1; i < kMaxHistory; ++i) history_[i - 1] = history_[i];
    history_[kMaxHistory - 1] = result_;
}

float DragSession::elapsed_s() const {
    if (phase_ == DragPhase::Finished) return result_.elapsed_s;
    if (phase_ == DragPhase::Countdown || phase_ == DragPhase::Idle) return 0.0f;
    return elapsed_s_;
}

float DragSession::countdown_remaining_s() const {
    if (phase_ != DragPhase::Countdown) return 0.0f;
    if (last_us_ <= phase_start_us_) return (float)settings_.countdown_ms / 1000.0f;
    const double elapsed = (double)(last_us_ - phase_start_us_) / 1000000.0;
    return std::max(0.0f, (float)((double)settings_.countdown_ms / 1000.0 - elapsed));
}

int DragSession::countdown_digit() const {
    if (phase_ != DragPhase::Countdown) return 0;
    return std::max(1, (int)std::ceil(countdown_remaining_s()));
}

float DragSession::progress() const {
    if (settings_.target_distance_m <= 0.0f) return 0.0f;
    return clampf(distance_m_ / settings_.target_distance_m, 0.0f, 1.0f);
}

bool DragSession::gpsReliable() const {
    if (phase_ == DragPhase::Finished) return result_.gps_reliable;
    return invalid_s_ < kGpsUnreliableS;
}

const DragResult* DragSession::personalBest() const {
    const DragResult* best = nullptr;
    for (size_t i = 0; i < history_count_; ++i) {
        const DragResult& candidate = history_[i];
        if (!same_target(candidate.target_distance_m, settings_.target_distance_m)) continue;
        if (best == nullptr || candidate.elapsed_s < best->elapsed_s) best = &candidate;
    }
    return best;
}

float DragSession::bestDeltaS() const {
    if (phase_ != DragPhase::Finished) return 0.0f;
    const DragResult* best = personalBest();
    if (best == nullptr) return 0.0f;
    return result_.elapsed_s - best->elapsed_s;
}

const DragSplit* DragSession::lastSplit() const {
    if (phase_ == DragPhase::Finished) {
        if (result_.split_count == 0) return nullptr;
        return &result_.splits[result_.split_count - 1];
    }
    if (split_count_ == 0) return nullptr;
    return &splits_[split_count_ - 1];
}

size_t DragSession::splitCount() const {
    if (phase_ == DragPhase::Finished) return result_.split_count;
    return split_count_;
}

const DragResult* DragSession::historyAt(size_t index) const {
    if (index >= history_count_) return nullptr;
    return &history_[index];
}

} // namespace moto
