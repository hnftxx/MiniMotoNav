#include "boot/boot_player.h"

#include <cstring>

namespace moto {
namespace boot {
namespace {

uint16_t read16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

} // namespace

bool Player::begin(uint16_t* pixels, uint32_t pixel_count) {
    if (pixels == nullptr || pixel_count < (uint32_t)kWidth * kHeight) return false;
    pixels_ = pixels;
    pixel_count_ = (uint32_t)kWidth * kHeight;
    return true;
}

bool Player::validAnimation() const {
    if (kAnimationDataSize == 0 || kAnimationFrameCount == 0 || kAnimationTickCount == 0) {
        return false;
    }
    uint32_t total_ticks = 0;
    for (uint16_t i = 0; i < kAnimationFrameCount; ++i) {
        const Frame& frame = kAnimationFrames[i];
        if (frame.ticks == 0) return false;
        if (frame.offset > kAnimationDataSize) return false;
        if (frame.size > kAnimationDataSize - frame.offset) return false;
        total_ticks += frame.ticks;
    }
    return total_ticks == kAnimationTickCount;
}

bool Player::decode(const uint8_t* data, uint32_t size, Rect& dirty) {
    if (data == nullptr || pixels_ == nullptr) return false;

    uint32_t index = 0;
    uint32_t pos = 0;
    uint16_t x0 = kWidth;
    uint16_t y0 = kHeight;
    uint16_t x1 = 0;
    uint16_t y1 = 0;
    bool any = false;

    while (pos + 4 <= size) {
        const uint16_t skip = read16(data + pos);
        const uint16_t run = read16(data + pos + 2);
        pos += 4;

        if (skip == 0 && run == 0) break;
        if (skip > 0) {
            if (skip > pixel_count_ - index) return false;
            index += skip;
        }
        if (run == 0) continue;

        const bool rle = (run & 0x8000) != 0;
        const uint16_t count = run & 0x7fff;
        if (count == 0) continue;
        if (count > pixel_count_ - index) return false;

        if (rle) {
            if (pos + 2 > size) return false;
            const uint16_t value = read16(data + pos);
            pos += 2;
            for (uint16_t i = 0; i < count; ++i) {
                pixels_[index] = value;
                const uint16_t x = (uint16_t)(index % kWidth);
                const uint16_t y = (uint16_t)(index / kWidth);
                if (!any) {
                    x0 = x; x1 = x; y0 = y; y1 = y; any = true;
                } else {
                    if (x < x0) x0 = x;
                    if (x > x1) x1 = x;
                    if (y < y0) y0 = y;
                    if (y > y1) y1 = y;
                }
                ++index;
            }
        } else {
            if ((uint32_t)count * 2u > size - pos) return false;
            for (uint16_t i = 0; i < count; ++i) {
                pixels_[index] = read16(data + pos);
                pos += 2;
                const uint16_t x = (uint16_t)(index % kWidth);
                const uint16_t y = (uint16_t)(index / kWidth);
                if (!any) {
                    x0 = x; x1 = x; y0 = y; y1 = y; any = true;
                } else {
                    if (x < x0) x0 = x;
                    if (x > x1) x1 = x;
                    if (y < y0) y0 = y;
                    if (y > y1) y1 = y;
                }
                ++index;
            }
        }
    }

    if (any) {
        dirty.x = x0;
        dirty.y = y0;
        dirty.w = (uint16_t)(x1 - x0 + 1);
        dirty.h = (uint16_t)(y1 - y0 + 1);
    } else {
        dirty = Rect{};
    }
    return true;
}

bool Player::decodeAnimationFrame(uint16_t index, Rect& dirty) {
    if (index >= kAnimationFrameCount) return false;
    const Frame& frame = kAnimationFrames[index];
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(kAnimationData);
    return decode(bytes + frame.offset, frame.size, dirty);
}

bool Player::reset() {
    if (pixels_ == nullptr) return false;
    std::memset(pixels_, 0, (size_t)pixel_count_ * sizeof(uint16_t));
    frame_index_ = 0;
    accumulator_ = 0.0f;
    finished_ = false;
    fallback_ = false;
    dirty_ = Rect{};

    if (!validAnimation()) return showLogo();
    if (!decodeAnimationFrame(0, dirty_)) return showLogo();
    return true;
}

bool Player::update(float dt) {
    if (pixels_ == nullptr || finished_) return false;
    if (dt < 0.0f) dt = 0.0f;
    accumulator_ += dt * (float)kFps;

    Rect combined{};
    bool changed = false;
    while (!finished_) {
        const Frame& frame = kAnimationFrames[frame_index_];
        if (accumulator_ < (float)frame.ticks) break;
        accumulator_ -= (float)frame.ticks;

        if ((uint32_t)frame_index_ + 1u >= kAnimationFrameCount) {
            finished_ = true;
            break;
        }

        ++frame_index_;
        Rect next{};
        if (!decodeAnimationFrame(frame_index_, next)) {
            showLogo();
            return true;
        }
        if (next.empty()) continue;
        if (!changed) {
            combined = next;
        } else {
            const uint16_t x0 = combined.x < next.x ? combined.x : next.x;
            const uint16_t y0 = combined.y < next.y ? combined.y : next.y;
            const uint16_t x1 = (uint16_t)(combined.x + combined.w > next.x + next.w
                                               ? combined.x + combined.w
                                               : next.x + next.w);
            const uint16_t y1 = (uint16_t)(combined.y + combined.h > next.y + next.h
                                               ? combined.y + combined.h
                                               : next.y + next.h);
            combined.x = x0;
            combined.y = y0;
            combined.w = (uint16_t)(x1 - x0);
            combined.h = (uint16_t)(y1 - y0);
        }
        changed = true;
    }

    dirty_ = changed ? combined : Rect{};
    return changed;
}

bool Player::showLogo() {
    if (pixels_ == nullptr) return false;
    std::memset(pixels_, 0, (size_t)pixel_count_ * sizeof(uint16_t));
    frame_index_ = 0;
    accumulator_ = 0.0f;
    fallback_ = true;
    finished_ = true;
    if (kLogoDataSize == 0) {
        dirty_ = Rect{};
        return false;
    }
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(kLogoData);
    return decode(bytes, kLogoDataSize, dirty_);
}

} // namespace boot
} // namespace moto
