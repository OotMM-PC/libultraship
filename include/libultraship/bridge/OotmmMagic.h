#pragma once

#include "OotmmItemGrant.h"
#include <algorithm>
#include <cstdint>

namespace Ship {
struct OotmmMagicUpgrade {
    uint8_t Tier;
    bool Refill;
};

inline OotmmMagicUpgrade ResolveOotmmMagicUpgrade(uint32_t gameCount, uint32_t sharedCount,
                                                 uint32_t itemCount, uint32_t applied,
                                                 bool acquired, bool doubleMagic) {
    const uint8_t owned = static_cast<uint8_t>(std::min<uint64_t>(2, uint64_t(gameCount) + sharedCount));
    const uint8_t native = doubleMagic ? 2 : acquired ? 1 : 0;
    return {std::max(owned, native),
            owned != 0 && (itemCount > applied || !acquired || owned > native)};
}

inline int16_t ResolveOotmmMagicJarAmount(OotmmGrantGame game, bool sharedMagic,
                                        bool large, uint32_t delta) {
    if (game == OotmmGrantGame::Shared) {
        return 0;
    }
    const uint32_t jarUnits = game == OotmmGrantGame::Mm || sharedMagic ? 24 : 12;
    return static_cast<int16_t>(std::min<uint64_t>(96, uint64_t(delta) * jarUnits * (large ? 2 : 1)));
}

inline int16_t ResolveOotmmMagicSaveValue(int16_t value, int16_t refillTarget,
                                        bool acquired, bool doubleMagic, bool pendingRefill) {
    if (!acquired || !pendingRefill) {
        return value;
    }
    return std::clamp<int16_t>(refillTarget, 0, doubleMagic ? 96 : 48);
}
}
