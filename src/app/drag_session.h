#pragma once

#include <cstddef>
#include <cstdint>

namespace moto {

enum class DragPhase { Idle, Countdown, Running, Paused, Finished };
enum class AccelTest { None, ZeroTo60, ZeroTo100 };

struct DragSettings {
    float target_distance_m = 400.0f;
    uint32_t countdown_ms = 3000;
    float split_interval_m = 1000.0f;
    AccelTest accel_test = AccelTest::None;
};

struct DragSplit {
    float distance_m = 0.0f;
    float elapsed_s = 0.0f;
};

struct DragResult {
    float elapsed_s = 0.0f;
    float target_distance_m = 0.0f;
    float distance_m = 0.0f;
    float max_speed_kph = 0.0f;
    float avg_speed_kph = 0.0f;
    float accel_time_s = 0.0f;
    bool gps_reliable = true;
    bool accel_complete = false;
    uint32_t split_count = 0;
    DragSplit splits[64]{};
};

class DragSession {
public:
    static constexpr size_t kMaxHistory = 12;

    void resetSession();
    void clearHistory();

    void setTargetDistance(float metres);
    void setCountdownMs(uint32_t ms);
    void setSplitInterval(float metres);
    void setAccelTest(AccelTest test);

    bool arm(uint64_t now_us);
    bool pause(uint64_t now_us);
    bool resume(uint64_t now_us);
    void cancel();
    void tick(uint64_t now_us, float speed_kph, bool gps_fix);

    DragPhase phase() const { return phase_; }
    const DragSettings& settings() const { return settings_; }
    float elapsed_s() const;
    float countdown_remaining_s() const;
    int countdown_digit() const;
    float distance_m() const { return distance_m_; }
    float progress() const;
    float current_speed_kph() const { return speed_kph_; }
    float max_speed_kph() const { return max_speed_kph_; }
    float split_interval_m() const { return settings_.split_interval_m; }
    bool accelComplete() const { return accel_complete_; }
    float accelTimeS() const { return accel_time_s_; }
    bool gpsFix() const { return gps_fix_; }
    bool gpsReliable() const;

    const DragResult& result() const { return result_; }
    bool newBest() const { return new_best_; }
    const DragResult* personalBest() const;
    float bestDeltaS() const;
    size_t splitCount() const;
    const DragSplit* lastSplit() const;

    size_t historyCount() const { return history_count_; }
    const DragResult* historyAt(size_t index) const;

private:
    void beginRun(uint64_t now_us);
    void integrate(float dt, float speed_kph, bool gps_fix);
    void addSplits(float from_m, float to_m, float from_s, float dt);
    void finish(float elapsed_s);
    void submitResult();

    DragSettings settings_{};
    DragPhase phase_ = DragPhase::Idle;

    uint64_t phase_start_us_ = 0;
    uint64_t last_us_ = 0;
    uint64_t countdown_us_ = 0;

    float elapsed_s_ = 0.0f;
    float distance_m_ = 0.0f;
    float speed_kph_ = 0.0f;
    float max_speed_kph_ = 0.0f;
    float invalid_s_ = 0.0f;
    float accel_time_s_ = 0.0f;
    bool gps_fix_ = true;
    bool accel_complete_ = false;
    bool new_best_ = false;

    DragResult result_{};
    DragSplit splits_[64]{};
    uint32_t split_count_ = 0;
    DragResult history_[kMaxHistory]{};
    size_t history_count_ = 0;
};

} // namespace moto
