#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace Ship {

enum class OotmmBombDrop {
    None,
    Bomb,
    Bombchu,
    Random,
};

constexpr OotmmBombDrop ResolveOotmmBombDrop(bool hasBombBag, bool hasBombchuBag,
                                           uint32_t bombs, uint32_t bombchus) {
    if (!hasBombchuBag) {
        return hasBombBag ? OotmmBombDrop::Bomb : OotmmBombDrop::None;
    }
    if (!hasBombBag) {
        return OotmmBombDrop::Bombchu;
    }
    if (bombs < 15 || bombchus < 15) {
        return bombchus < bombs ? OotmmBombDrop::Bombchu : OotmmBombDrop::Bomb;
    }
    return OotmmBombDrop::Random;
}

constexpr std::optional<uint8_t> ResolveOotmmBombchuCapacity(std::string_view behavior,
                                                           uint32_t bombBag, uint32_t bombchuBag) {
    if (bombBag > 3 || bombchuBag > 3) {
        return std::nullopt;
    }
    constexpr std::array<uint8_t, 4> capacities = {0, 20, 30, 40};
    if (behavior == "free") {
        return 50;
    }
    if (behavior == "bombBag") {
        return capacities[bombBag];
    }
    if (behavior == "bagFirst") {
        return bombchuBag == 0 ? 0 : 50;
    }
    if (behavior == "bagSeparate") {
        return capacities[bombchuBag];
    }
    return std::nullopt;
}

}
