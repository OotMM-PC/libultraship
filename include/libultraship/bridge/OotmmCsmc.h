#pragma once

#include "libultraship/bridge/OotmmGameState.h"
#include "libultraship/bridge/OotmmInventory.h"

namespace Ship {

enum class OotmmCsmcClass {
    Normal,
    BossKey,
    Major,
    Key,
    Spider,
    Fairy,
    Heart,
    Soul,
    MapCompass,
};

enum class OotmmCsmcMode {
    Never,
    Agony,
    Always,
};

OotmmCsmcMode OotmmCsmc_Mode(const OotmmGameState& state);

/// MM's agony gate, an inventory condition: the shared stone when the seed shares it,
/// MM's own when it exists, otherwise OoT's. (OoT reads its save's quest bit instead.)
bool OotmmCsmc_MmHasAgony(const OotmmGameState& state, const OotmmInventory& inventory);

/// The appearance class for a check's contents. Unrevealed checks (agony mode without
/// the stone) read as Major, so undiscovered containers all stay worth opening.
OotmmCsmcClass OotmmCsmc_Classify(const OotmmGameState& state, OotmmGame game,
                                  const OotmmPlacement& placement, bool revealed);

} // namespace Ship
