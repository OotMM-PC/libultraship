#include "libultraship/bridge/OotmmCsmc.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Ship {

namespace {

#include "OotmmCsmcTable.inc"
#include "OotmmGlitterTexture.inc"

const std::unordered_map<std::string, OotmmCsmcClass>& TypeTable() {
    static const std::unordered_map<std::string, OotmmCsmcClass> sTable = [] {
        std::unordered_map<std::string, OotmmCsmcClass> table;
        table.reserve(std::size(kOotmmCsmcItemType));
        for (const auto& entry : kOotmmCsmcItemType) {
            table.emplace(entry.first, entry.second);
        }
        return table;
    }();
    return sTable;
}

OotmmCsmcClass LookupItem(OotmmGame game, const std::string& itemId) {
    const auto& table = TypeTable();
    if (itemId.rfind("SHARED_", 0) == 0) {
        const std::string suffix = itemId.substr(7);
        const char* self = game == OotmmGame::Oot ? "OOT_" : "MM_";
        const char* other = game == OotmmGame::Oot ? "MM_" : "OOT_";
        if (const auto entry = table.find(self + suffix); entry != table.end()) {
            return entry->second;
        }
        if (const auto entry = table.find(other + suffix); entry != table.end()) {
            return entry->second;
        }
        return OotmmCsmcClass::Normal;
    }
    const auto entry = table.find(itemId);
    return entry == table.end() ? OotmmCsmcClass::Normal : entry->second;
}

bool IsBagFirstBombchu(const OotmmGameState& state, const std::string& itemId) {
    static const char* kOotChus[] = { "OOT_BOMBCHU_5", "OOT_BOMBCHU_10", "OOT_BOMBCHU_20" };
    static const char* kMmChus[] = { "MM_BOMBCHU", "MM_BOMBCHU_5", "MM_BOMBCHU_10", "MM_BOMBCHU_20" };
    for (const char* id : kOotChus) {
        if (itemId == id) {
            return state.GetStringSetting("bombchuBehaviorOot", "free") == "bagFirst";
        }
    }
    for (const char* id : kMmChus) {
        if (itemId == id) {
            return state.GetStringSetting("bombchuBehaviorMm", "free") == "bagFirst";
        }
    }
    if (itemId.rfind("SHARED_BOMBCHU", 0) == 0) {
        return state.GetStringSetting("bombchuBehaviorOot", "free") == "bagFirst" ||
               state.GetStringSetting("bombchuBehaviorMm", "free") == "bagFirst";
    }
    return false;
}

} // namespace

bool OotmmCsmc_MmHasAgony(const OotmmGameState& state, const OotmmInventory& inventory) {
    if (state.GetBoolSetting("sharedStoneAgony", false)) {
        return inventory.Has("SHARED_STONE_OF_AGONY");
    }
    return state.GetBoolSetting("stoneAgonyMm", false) ? inventory.Has("MM_STONE_OF_AGONY")
                                                       : inventory.Has("OOT_STONE_OF_AGONY");
}

OotmmCsmcMode OotmmCsmc_Mode(const OotmmGameState& state) {
    const std::string mode = state.GetStringSetting("csmc", "always");
    if (mode == "never") {
        return OotmmCsmcMode::Never;
    }
    if (mode == "agony") {
        return OotmmCsmcMode::Agony;
    }
    return OotmmCsmcMode::Always;
}

OotmmCsmcClass OotmmCsmc_Classify(const OotmmGameState& state, OotmmGame game,
                                  const OotmmPlacement& placement, bool revealed) {
    if (!revealed) {
        return OotmmCsmcClass::Major;
    }

    const std::string& visual =
        placement.CloakItemId.empty() ? placement.ItemId : placement.CloakItemId;
    if (visual.find("_TRAP_") != std::string::npos && state.GetBoolSetting("cloakTraps", false)) {
        return OotmmCsmcClass::Major;
    }
    if (IsBagFirstBombchu(state, visual)) {
        return OotmmCsmcClass::Major;
    }

    const OotmmCsmcClass itemClass = LookupItem(game, visual);
    if (itemClass == OotmmCsmcClass::Heart && !state.GetBoolSetting("csmcHearts", true)) {
        return OotmmCsmcClass::Normal;
    }
    if (itemClass == OotmmCsmcClass::MapCompass && !state.GetBoolSetting("csmcMapCompass", true)) {
        return OotmmCsmcClass::Normal;
    }
    return itemClass;
}

uint32_t OotmmCsmc_GlitterColor(OotmmCsmcClass itemClass, bool enabled) {
    if (!enabled) {
        return 0xFFFFFF;
    }
    switch (itemClass) {
        case OotmmCsmcClass::Normal:
            return 0x4A230D;
        case OotmmCsmcClass::BossKey:
            return 0x0000FF;
        case OotmmCsmcClass::Major:
            return 0xFFFF00;
        case OotmmCsmcClass::Key:
            return 0x222222;
        case OotmmCsmcClass::Fairy:
            return 0xFA89EF;
        case OotmmCsmcClass::Heart:
            return 0xFF0000;
        case OotmmCsmcClass::Soul:
            return 0x5E0CC9;
        case OotmmCsmcClass::MapCompass:
            return 0xC75000;
        default:
            return 0xFFFFFF;
    }
}

const uint8_t* OotmmCsmc_GlitterFrame(uint32_t frameCount) {
    static constexpr uint8_t kFrames[] = { 0, 1, 2, 3, 4, 3, 2, 1 };
    return kOotmmGlitterI4 + 512 * kFrames[frameCount & 7];
}

bool OotmmCsmc_IsRenewable(const std::string& itemId) {
    static const std::unordered_set<std::string> kOot = {
        "RECOVERY_HEART", "STICK", "STICKS_5", "STICKS_10", "NUTS_5", "NUTS_5_ALT", "NUTS_10", "BOMB", "BOMBS_5",
        "BOMBS_10", "BOMBS_20", "BOMBS_30", "ARROWS_5", "ARROWS_10", "ARROWS_30", "DEKU_SEEDS_5", "DEKU_SEEDS_30",
        "BOMBCHU_5", "BOMBCHU_10", "BOMBCHU_20", "SHIELD_DEKU", "SHIELD_HYLIAN", "MILK", "POTION_RED",
        "POTION_GREEN", "POTION_BLUE", "FAIRY", "FISH", "BUGS", "BLUE_FIRE", "POE", "MAGIC_JAR_SMALL",
        "MAGIC_JAR_LARGE", "FAIRY_BIG", "BOTTLE_MILK", "BOTTLE_POTION_RED", "BOTTLE_POTION_GREEN",
        "BOTTLE_POTION_BLUE", "BOTTLE_FAIRY", "BOTTLE_POE", "BOTTLE_BIG_POE", "BOTTLE_BLUE_FIRE",
    };
    static const std::unordered_set<std::string> kMm = {
        "FAIRY_BIG", "RECOVERY_HEART", "STICK", "NUT", "NUTS_5", "NUTS_10", "BOMB", "BOMBS_5", "BOMBS_10",
        "BOMBS_20", "BOMBS_30", "BOMBCHU", "BOMBCHU_5", "BOMBCHU_10", "BOMBCHU_20", "ARROWS_10", "ARROWS_30",
        "ARROWS_40", "DEKU_SEEDS_5", "DEKU_SEEDS_30", "SHIELD_HERO", "MILK", "CHATEAU", "FISH", "BUGS", "FAIRY",
        "POTION_RED", "POTION_GREEN", "POTION_BLUE", "MAGIC_BEAN", "POE", "MAGIC_JAR_SMALL", "MAGIC_JAR_LARGE",
        "BOTTLE_POTION_RED", "BOTTLE_POTION_GREEN", "BOTTLE_POTION_BLUE", "BOTTLE_CHATEAU", "BOTTLE_MILK",
        "BOTTLE_FAIRY", "BOTTLE_POE", "BOTTLE_BIG_POE",
    };
    if (itemId.rfind("OOT_", 0) == 0) {
        return kOot.count(itemId.substr(4)) != 0;
    }
    if (itemId.rfind("MM_", 0) == 0) {
        return kMm.count(itemId.substr(3)) != 0;
    }
    if (itemId.rfind("SHARED_", 0) == 0) {
        const std::string suffix = itemId.substr(7);
        return kOot.count(suffix) != 0 || kMm.count(suffix) != 0;
    }
    return false;
}

} // namespace Ship
