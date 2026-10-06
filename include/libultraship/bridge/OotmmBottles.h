#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace Ship {

enum class OotmmBottleContent : uint32_t {
    None,
    Empty,
    PotionRed,
    PotionGreen,
    PotionBlue,
    Fairy,
    Fish,
    Milk,
    MilkHalf,
    RutoLetter,
    BlueFire,
    Bugs,
    Poe,
    BigPoe,
    Mushroom,
    Chateau,
    GoldDust,
    Seahorse,
    DekuPrincess,
    SpringWater,
    SpringWaterHot,
    ZoraEgg,
    KotakeDrink,
    Count,
};

inline constexpr std::array<std::string_view, 7> kOotmmSharedBottleValues = {
    "bottle1", "bottle2", "bottle3", "bottle4", "bottle5", "bottle6", "rutoLetter",
};

inline constexpr int kOotmmBottleCount = 6;
inline constexpr uint32_t kOotmmRutoLetterUsedOot = 1;
inline constexpr uint32_t kOotmmRutoLetterUsedMm = 2;

inline int OotmmBottleIndex(std::string_view key) {
    for (int i = 0; i < kOotmmBottleCount; i++) {
        if (kOotmmSharedBottleValues[i] == key) {
            return i;
        }
    }
    return -1;
}

inline bool OotmmIsProgressiveBottle(std::string_view id) {
    return id.ends_with("_BOTTLE_RUTO_LETTER") || id.ends_with("_BOTTLED_GOLD_DUST");
}

inline bool OotmmIsSharedBottleReward(std::string_view id) {
    static constexpr std::array<std::string_view, 11> kRefills = {
        "SHARED_POTION_RED", "SHARED_POTION_GREEN", "SHARED_POTION_BLUE", "SHARED_FAIRY",
        "SHARED_POE",        "SHARED_BIG_POE",      "SHARED_BLUE_FIRE",   "SHARED_MILK",
        "SHARED_CHATEAU",    "SHARED_BUGS",         "SHARED_FISH",
    };
    if (id.starts_with("SHARED_BOTTLE_") || id.starts_with("SHARED_BOTTLED_")) {
        return true;
    }
    for (const std::string_view refill : kRefills) {
        if (id == refill) {
            return true;
        }
    }
    return false;
}

} // namespace Ship
