#pragma once

#include <cstdint>

#include "app/moto_types.h"

namespace moto {

enum class CommandType {
    Invalid,
    NextMode,
    SetMode,
    NextTheme,
    SetTheme,
    TogglePause,
    ToggleAuto,
    Reset,
    ToggleControls,
    ToggleFps,
    AdjustSpeed,
    AdjustThrottle,
    AdjustRpm,
    DragRun,
    DragReset,
    SetDragTarget,
    SetDragSplit,
    SetDragCountdown,
};

struct Command {
    CommandType type = CommandType::Invalid;
    float value = 0.0f;
    AppMode mode = AppMode::Speedo;
    uint8_t theme = 0;
};

bool parse_command(const char* text, Command& out);

class CommandQueue {
public:
    static constexpr int kCapacity = 12;

    bool post(const Command& command);
    bool pop(Command& out);
    bool empty() const { return count_ == 0; }
    int count() const { return count_; }

private:
    Command items_[kCapacity]{};
    int head_ = 0;
    int count_ = 0;
};

} // namespace moto
