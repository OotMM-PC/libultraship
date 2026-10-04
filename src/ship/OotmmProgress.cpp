#include "libultraship/bridge/OotmmProgress.h"

#include <array>
#include <string>
#include <utility>

namespace Ship {

namespace {

constexpr std::array<std::pair<std::string_view, std::string_view>, 12> kMasterQuestMembers = { {
    { "DT", "DT" },
    { "DC", "DC" },
    { "JJ", "JJ" },
    { "FOREST", "Forest" },
    { "FIRE", "Fire" },
    { "WATER", "Water" },
    { "SPIRIT", "Spirit" },
    { "SHADOW", "Shadow" },
    { "BOTW", "BotW" },
    { "IC", "IC" },
    { "GTG", "GTG" },
    { "GANON", "Ganon" },
} };

uint32_t OotDungeonSmallKeys(const OotmmGameState& state, std::string_view code) {
    const bool mq = OotmmIsMasterQuest(state, code);
    if (code == "FOREST") {
        return mq ? 6 : 5;
    }
    if (code == "FIRE") {
        return mq ? 5 : (state.GetStringSetting("smallKeyShuffleOot", "ownDungeon") == "anywhere" ? 8 : 7);
    }
    if (code == "WATER") {
        return mq ? 2 : 5;
    }
    if (code == "SPIRIT") {
        return mq ? 7 : 5;
    }
    if (code == "SHADOW") {
        return mq ? 6 : 5;
    }
    if (code == "BOTW") {
        return mq ? 2 : 3;
    }
    if (code == "GTG") {
        return mq ? 3 : 9;
    }
    return 0;
}

uint32_t OotSmallKeys(const OotmmGameState& state, std::string_view code) {
    if (code == "GANON") {
        return OotmmIsMasterQuest(state, code) ? 3 : 2;
    }
    if (code == "GF") {
        const std::string fortress = state.GetStringSetting("gerudoFortress", "vanilla");
        return fortress == "open" ? 0 : (fortress == "single" ? 1 : 4);
    }
    if (code == "TCG") {
        return state.GetStringSetting("smallKeyShuffleChestGame", "vanilla") == "vanilla" ? 0 : 6;
    }
    if (state.GetStringSetting("smallKeyShuffleOot", "ownDungeon") == "removed") {
        return 0;
    }
    return OotDungeonSmallKeys(state, code);
}

uint32_t MmSmallKeys(const OotmmGameState& state, std::string_view code) {
    if (state.GetStringSetting("smallKeyShuffleMm", "ownDungeon") == "removed") {
        return 0;
    }
    if (code == "WF" || code == "GB") {
        return 1;
    }
    if (code == "SH") {
        return 3;
    }
    return code == "ST" ? 4 : 0;
}

} // namespace

bool OotmmIsMasterQuest(const OotmmGameState& state, std::string_view code) {
    for (const auto& [dungeon, member] : kMasterQuestMembers) {
        if (dungeon == code) {
            return state.WorldFlagContains("mqDungeons", std::string(member));
        }
    }
    return false;
}

uint32_t OotmmMaxSmallKeys(const OotmmGameState& state, OotmmGame game, std::string_view code) {
    return game == OotmmGame::Oot ? OotSmallKeys(state, code) : MmSmallKeys(state, code);
}

uint32_t OotmmClockMask(const OotmmGameState& state, const OotmmInventory& inventory) {
    constexpr uint32_t kAllHalfDays = 0x3F;
    constexpr std::array<const char*, 6> kClockIds = {
        "MM_CLOCK1", "MM_CLOCK2", "MM_CLOCK3", "MM_CLOCK4", "MM_CLOCK5", "MM_CLOCK6",
    };

    uint32_t mask = 0;
    for (size_t i = 0; i < kClockIds.size(); i++) {
        if (inventory.Has(kClockIds[i])) {
            mask |= 1u << i;
        }
    }
    const std::string mode = state.GetStringSetting("progressiveClocks", "ascending");
    if (mode == "separate") {
        return mask & kAllHalfDays;
    }

    uint32_t count = inventory.Count("MM_CLOCK");
    if (count > 5) {
        count = 5;
    }
    if (mode == "descending") {
        mask |= ((1u << (count + 1)) - 1) << (5 - count);
    } else {
        mask |= (1u << (count + 1)) - 1;
    }
    return mask & kAllHalfDays;
}

} // namespace Ship
