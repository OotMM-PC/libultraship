#include "libultraship/bridge/OotmmCsmc.h"

#include <string>
#include <unordered_map>
#include <utility>

namespace Ship {

namespace {

#include "OotmmCsmcTable.inc"

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
    // Shared items resolve to the hosting game's entry, falling back to the other game's.
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

// With bagFirst behavior the first bombchus are the progressive bag, so they read as major.
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

} // namespace Ship
