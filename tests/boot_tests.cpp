#include <cassert>
#include <cstdio>
#include <vector>

#include "boot/boot_player.h"

int main() {
    using namespace moto::boot;

    std::vector<uint16_t> pixels((size_t)kWidth * kHeight);
    Player player;
    assert(player.begin(pixels.data(), (uint32_t)pixels.size()));
    assert(player.reset());
    assert(!player.finished());
    assert(player.frameIndex() == 0);
    assert(player.frameCount() == 56);
    assert(player.tickCount() == 92);
    assert(!player.dirty().empty());

    int non_black = 0;
    for (uint16_t value : pixels) {
        if (value != 0) ++non_black;
    }
    assert(non_black > 100);

    assert(!player.update(0.0f));
    for (int i = 0; i < 300 && !player.finished(); ++i) {
        player.update(1.0f / 60.0f);
    }
    assert(player.finished());
    assert(player.frameIndex() + 1 == player.frameCount());

    Player logo;
    assert(logo.begin(pixels.data(), (uint32_t)pixels.size()));
    assert(logo.showLogo());
    assert(logo.finished());
    assert(logo.fallback());
    assert(!logo.dirty().empty());
    non_black = 0;
    for (uint16_t value : pixels) {
        if (value != 0) ++non_black;
    }
    assert(non_black > 100);

    std::printf("boot_tests: %u frames, %u ticks, %u bytes/frame\n",
                (unsigned)player.frameCount(), (unsigned)player.tickCount(),
                (unsigned)Player::memoryBytes());
    return 0;
}
