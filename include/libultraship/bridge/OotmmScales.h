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

/// True when the seed makes swimming an item, so deep water voids the player until a scale is owned.
bool OotmmSwimNeedsScale(const OotmmGameState& state, OotmmGame game);

/// nativeDive is the game's own dive upgrade (0 none, 1 silver, 2 golden); it outranks the seed
/// ladder because reaching it proves the bronze rung was already passed.
OotmmScaleTier OotmmScaleTierOf(const OotmmGameState& state, const OotmmInventory& inventory, OotmmGame game,
                                uint32_t nativeDive);

} // namespace Ship
