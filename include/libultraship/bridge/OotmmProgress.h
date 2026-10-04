#pragma once

#include "libultraship/bridge/OotmmGameState.h"

#include <cstdint>
#include <string_view>

namespace Ship {

uint32_t OotmmMaxSmallKeys(const OotmmGameState& state, OotmmGame game, std::string_view code);
bool OotmmIsMasterQuest(const OotmmGameState& state, std::string_view code);
uint32_t OotmmClockMask(const OotmmGameState& state, const OotmmInventory& inventory);

} // namespace Ship
