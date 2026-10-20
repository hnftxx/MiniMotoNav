#pragma once

#include <cstdint>

namespace moto {

struct Telemetry {
    float speed_kph = 0.0f;
    float target_kph = 0.0f;
    float throttle = 0.0f;   // percent, 0..100
    float rpm = 1300.0f;     // engine rpm
    float trip_km = 0.0f;
    float engine_load = 0.0f; // 0..1, drives the throttle bar shimmer
    int gear = 1;
    bool running = false;
    bool auto_drive = true;
};

class TelemetrySim {
public:
    void reset();
    void tick(float dt);

    void setRunning(bool run) { data_.running = run; }
    bool running() const { return data_.running; }

    void setAutoDrive(bool automatic);
    bool autoDrive() const { return data_.auto_drive; }

    void setManualSpeed(float kph);
    void setManualThrottle(float percent);
    void setManualRpm(float rpm);

    const Telemetry& data() const { return data_; }

private:
    float scenarioTarget(float t) const;
    void updateAuto(float dt);
    void updateManual(float dt);

    Telemetry data_;
    float time_s_ = 0.0f;
    float manual_speed_ = 78.0f;
    float manual_throttle_ = 62.0f;
    float manual_rpm_ = 5340.0f;
    float smooth_target_ = 0.0f;
};

} // namespace moto
