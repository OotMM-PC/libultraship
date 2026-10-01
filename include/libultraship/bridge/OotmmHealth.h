#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

namespace Ship {
struct OotmmHealthResult {
    int16_t Capacity;
    uint8_t Pieces;
    int16_t Healing;
};

inline std::optional<OotmmHealthResult> ResolveOotmmHealth(
    int16_t capacity, int16_t health, uint8_t pieces,
    uint32_t containers, uint32_t addedPieces, uint32_t recoveryHearts) {
    if (capacity < 0 || health < 0 || health > capacity || pieces > 3) {
        return std::nullopt;
    }
    const uint64_t totalPieces = uint64_t(pieces) + addedPieces;
    const uint64_t nextCapacity = uint64_t(capacity) + (uint64_t(containers) + totalPieces / 4) * 16;
    if (nextCapacity > uint64_t(std::numeric_limits<int16_t>::max())) {
        return std::nullopt;
    }
    const uint64_t healing = (containers != 0 || addedPieces != 0) ? 320 : uint64_t(recoveryHearts) * 16;
    return OotmmHealthResult{static_cast<int16_t>(nextCapacity), static_cast<uint8_t>(totalPieces % 4),
        static_cast<int16_t>(std::min(healing, nextCapacity - uint64_t(health)))};
}

inline std::optional<OotmmHealthResult> ResolveOotmmSavedHealth(
    int16_t capacity, int16_t health, uint8_t pieces, uint64_t containers, uint64_t earnedPieces) {
    constexpr uint64_t maxPieces = ((uint64_t(std::numeric_limits<int16_t>::max()) / 16) - 3) * 4 + 3;
    if (capacity < 48 || capacity % 16 != 0 || pieces > 3 ||
        containers > maxPieces / 4 || earnedPieces > maxPieces || containers * 4 + earnedPieces > maxPieces) {
        return std::nullopt;
    }
    const uint64_t earned = containers * 4 + earnedPieces;
    const uint64_t saved = uint64_t((capacity - 48) / 16) * 4 + pieces;
    return ResolveOotmmHealth(capacity, health, pieces, 0,
        earned > saved ? static_cast<uint32_t>(earned - saved) : 0, 0);
}
}
