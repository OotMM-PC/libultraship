#include "libultraship/bridge/OotmmScales.h"

#include "libultraship/bridge/OotmmInventory.h"

#include <algorithm>
#include <string>

namespace Ship {

bool OotmmSwimNeedsScale(const OotmmGameState& state, OotmmGame game) {
    if (!state.GetBoolSetting("bronzeScale", false)) {
        return false;
    }
    return game == OotmmGame::Oot || state.GetBoolSetting("scalesMm", false);
}

OotmmScaleTier OotmmScaleTierOf(const OotmmGameState& state, const OotmmInventory& inventory, OotmmGame game,
                                uint32_t nativeDive) {
    if (nativeDive != 0) {
        return nativeDive >= 2 ? OotmmScaleTier::Golden : OotmmScaleTier::Silver;
    }

    uint32_t count = inventory.Count(game == OotmmGame::Mm ? "MM_SCALE" : "OOT_SCALE");
    if (count == 0) {
        count = inventory.Count("SHARED_SCALE");
    }
    if (count == 0) {
        return OotmmScaleTier::None;
    }
    if (!state.GetBoolSetting("bronzeScale", false)) {
        count++;
    }
    return static_cast<OotmmScaleTier>(std::min<uint32_t>(count, static_cast<uint32_t>(OotmmScaleTier::Golden)));
}

} // namespace Ship
