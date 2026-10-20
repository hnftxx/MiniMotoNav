#pragma once

#include <cstdint>

namespace moto {

struct PerfStats {
    float fps = 0.0f;
    float frame_ms = 0.0f;
    float render_ms = 0.0f;
    float worst_ms = 0.0f;
    bool visible = true;
};

struct Clock {
    int hour = 14;
    int minute = 32;

    void addMinutes(int m) {
        total += m;
        total %= (24 * 60);
        if (total < 0) total += 24 * 60;
        hour = total / 60;
        minute = total % 60;
    }

    int hour_minute() const { return hour * 60 + minute; }

    int total = 14 * 60 + 32;
};

} // namespace moto
