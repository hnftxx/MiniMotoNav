#pragma once

#include <cstdint>

#include "boot/boot_assets.h"

namespace moto {
namespace boot {

struct Rect {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    bool empty() const { return w == 0 || h == 0; }
};

class Player {
public:
    bool begin(uint16_t* pixels, uint32_t pixel_count);
    bool reset();
    bool update(float dt);
    bool showLogo();

    bool finished() const { return finished_; }
    bool fallback() const { return fallback_; }
    const uint16_t* pixels() const { return pixels_; }
    Rect dirty() const { return dirty_; }
    uint16_t frameIndex() const { return frame_index_; }
    uint16_t frameCount() const { return kAnimationFrameCount; }
    uint16_t tickCount() const { return kAnimationTickCount; }

    static constexpr uint32_t memoryBytes() { return (uint32_t)kWidth * kHeight * 2u; }

private:
    bool decode(const uint8_t* data, uint32_t size, Rect& dirty);
    bool decodeAnimationFrame(uint16_t index, Rect& dirty);
    bool validAnimation() const;

    uint16_t* pixels_ = nullptr;
    uint32_t pixel_count_ = 0;
    uint16_t frame_index_ = 0;
    float accumulator_ = 0.0f;
    bool finished_ = false;
    bool fallback_ = false;
    Rect dirty_{};
};

} // namespace boot
} // namespace moto
