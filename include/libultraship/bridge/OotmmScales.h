#pragma once

#include <cstdint>

#include "OotmmGameState.h"

namespace Ship {

class OotmmInventory;

enum class OotmmScaleTier : uint8_t {
    None,
    Bronze,
    Silver,
    Golden,
};

bool OotmmSwimNeedsScale(const OotmmGameState& state, OotmmGame game);

OotmmScaleTier OotmmScaleTierOf(const OotmmGameState& state, const OotmmInventory& inventory, OotmmGame game,
                                uint32_t nativeDive);

} // namespace Ship
