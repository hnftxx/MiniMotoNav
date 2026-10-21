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
    bool gps_fix = true;
    int gps_sats = 11;
    float gps_accuracy_m = 2.4f;
    float altitude_m = 1240.0f;
    float heading_deg = 32.0f;
};

class TelemetrySim {
public:
    void reset();
    void tick(float dt);

    void setRunning(bool run) { data_.running = run; }
    bool running() const { return data_.running; }

    void setAutoDrive(bool automatic);
    bool autoDrive() const { return data_.auto_drive; }

    void setDragMode(bool enabled);
    bool dragMode() const { return drag_mode_; }
    void beginDragRun();

    void setManualSpeed(float kph);
    void setManualThrottle(float percent);
    void setManualRpm(float rpm);

    const Telemetry& data() const { return data_; }

private:
    float scenarioTarget(float t) const;
    void updateAuto(float dt);
    void updateManual(float dt);
    void updateDrag(float dt);

    Telemetry data_;
    float time_s_ = 0.0f;
    float manual_speed_ = 78.0f;
    float manual_throttle_ = 62.0f;
    float manual_rpm_ = 5340.0f;
    float smooth_target_ = 0.0f;
    float drag_time_s_ = 0.0f;
    bool drag_mode_ = false;
};

} // namespace moto
