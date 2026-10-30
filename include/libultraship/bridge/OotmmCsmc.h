#pragma once

#include "libultraship/bridge/OotmmGameState.h"
#include "libultraship/bridge/OotmmInventory.h"

#include <cstdint>
#include <string>

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

bool OotmmCsmc_MmHasAgony(const OotmmGameState& state, const OotmmInventory& inventory);

OotmmCsmcClass OotmmCsmc_Classify(const OotmmGameState& state, OotmmGame game,
                                  const OotmmPlacement& placement, bool revealed);

uint32_t OotmmCsmc_GlitterColor(OotmmCsmcClass itemClass, bool enabled);

const uint8_t* OotmmCsmc_GlitterFrame(uint32_t frameCount);

bool OotmmCsmc_IsRenewable(const std::string& itemId);

} // namespace Ship
