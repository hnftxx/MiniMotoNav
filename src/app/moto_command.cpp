#include "app/moto_command.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace moto {
namespace {

char lower(char c) { return (char)std::tolower((unsigned char)c); }

bool equals(const char* a, const char* b) {
    while (*a && *b) {
        if (lower(*a) != lower(*b)) return false;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

bool parse_number(const char* text, float& out) {
    if (text == nullptr || *text == '\0') return false;
    errno = 0;
    char* end = nullptr;
    const float value = std::strtof(text, &end);
    if (errno != 0 || end == text) return false;
    while (*end && std::isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return false;
    out = value;
    return true;
}

bool parse_mode(const char* text, AppMode& out) {
    if (equals(text, "speedo")) {
        out = AppMode::Speedo;
        return true;
    }
    if (equals(text, "nav") || equals(text, "navigate")) {
        out = AppMode::Nav;
        return true;
    }
    if (equals(text, "drag")) {
        out = AppMode::Drag;
        return true;
    }
    if (equals(text, "gps") || equals(text, "clock")) {
        out = AppMode::GpsClock;
        return true;
    }
    return false;
}

bool parse_theme(const char* text, uint8_t& out) {
    if (equals(text, "race") || equals(text, "racing")) {
        out = 0;
        return true;
    }
    if (equals(text, "day") || equals(text, "light")) {
        out = 1;
        return true;
    }
    if (equals(text, "night") || equals(text, "dark")) {
        out = 2;
        return true;
    }
    return false;
}

} // namespace

bool parse_command(const char* text, Command& out) {
    out = Command{};
    if (text == nullptr) return false;
    while (*text && std::isspace((unsigned char)*text)) ++text;
    if (*text == '\0') return false;

    char verb[24]{};
    int n = 0;
    while (*text && !std::isspace((unsigned char)*text) && n < (int)sizeof(verb) - 1) {
        verb[n++] = *text++;
    }
    verb[n] = '\0';
    while (*text && std::isspace((unsigned char)*text)) ++text;

    if (equals(verb, "mode") || equals(verb, "m")) {
        AppMode mode;
        if (!parse_mode(text, mode)) return false;
        out.type = CommandType::SetMode;
        out.mode = mode;
        return true;
    }
    if (equals(verb, "theme") || equals(verb, "t")) {
        uint8_t theme;
        if (!parse_theme(text, theme)) return false;
        out.type = CommandType::SetTheme;
        out.theme = theme;
        return true;
    }
    if (equals(verb, "speed")) {
        if (!parse_number(text, out.value)) return false;
        out.type = CommandType::AdjustSpeed;
        return true;
    }
    if (equals(verb, "throttle")) {
        if (!parse_number(text, out.value)) return false;
        out.type = CommandType::AdjustThrottle;
        return true;
    }
    if (equals(verb, "rpm")) {
        if (!parse_number(text, out.value)) return false;
        out.type = CommandType::AdjustRpm;
        return true;
    }
    if (equals(verb, "target")) {
        if (!parse_number(text, out.value)) return false;
        out.type = CommandType::SetDragTarget;
        return true;
    }
    if (equals(verb, "split")) {
        if (!parse_number(text, out.value)) return false;
        out.type = CommandType::SetDragSplit;
        return true;
    }
    if (equals(verb, "countdown")) {
        if (!parse_number(text, out.value)) return false;
        out.type = CommandType::SetDragCountdown;
        return true;
    }
    if (equals(verb, "next") || equals(verb, "n")) {
        out.type = CommandType::NextMode;
        return true;
    }
    if (equals(verb, "pause") || equals(verb, "space")) {
        out.type = CommandType::TogglePause;
        return true;
    }
    if (equals(verb, "auto")) {
        out.type = CommandType::ToggleAuto;
        return true;
    }
    if (equals(verb, "reset") || equals(verb, "r")) {
        out.type = CommandType::Reset;
        return true;
    }
    if (equals(verb, "controls") || equals(verb, "c")) {
        out.type = CommandType::ToggleControls;
        return true;
    }
    if (equals(verb, "fps") || equals(verb, "f")) {
        out.type = CommandType::ToggleFps;
        return true;
    }
    if (equals(verb, "drag") || equals(verb, "start")) {
        out.type = CommandType::DragRun;
        return true;
    }
    if (equals(verb, "dragreset")) {
        out.type = CommandType::DragReset;
        return true;
    }
    return false;
}

bool CommandQueue::post(const Command& command) {
    if (command.type == CommandType::Invalid || count_ >= kCapacity) return false;
    items_[(head_ + count_) % kCapacity] = command;
    ++count_;
    return true;
}

bool CommandQueue::pop(Command& out) {
    if (count_ == 0) return false;
    out = items_[head_];
    head_ = (head_ + 1) % kCapacity;
    --count_;
    return true;
}

} // namespace moto
