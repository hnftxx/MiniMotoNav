#pragma once

#include <cstdint>

namespace moto {
namespace boot {

constexpr uint16_t kWidth = 320;
constexpr uint16_t kHeight = 240;
constexpr uint16_t kFps = 30;

struct Frame {
    uint32_t offset;
    uint32_t size;
    uint16_t ticks;
    uint16_t reserved;
};

extern const uint32_t kAnimationData[];
extern const uint32_t kAnimationDataSize;
extern const Frame kAnimationFrames[];
extern const uint16_t kAnimationFrameCount;
extern const uint16_t kAnimationTickCount;

extern const uint32_t kLogoData[];
extern const uint32_t kLogoDataSize;

} // namespace boot
} // namespace moto
