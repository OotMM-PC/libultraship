#include "libultraship/bridge/OotmmGameState.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <type_traits>

namespace Ship {

namespace {

using json = nlohmann::json;

bool ReadJsonFile(const std::string& path, json& result, std::string& error, bool uniqueKeys = false) {
    std::ifstream stream(path);
    if (!stream) {
        error = "cannot open " + path;
        return false;
    }

    bool valid = true;
    std::vector<std::set<std::string>> keys;
    result = json::parse(stream, [&](int depth, json::parse_event_t event, json& value) {
        if (!uniqueKeys) {
            return true;
        }
        if (depth > 16) {
            valid = false;
        }
        if (event == json::parse_event_t::object_start) {
            keys.emplace_back();
        } else if (event == json::parse_event_t::key) {
            if (keys.empty() || !keys.back().insert(value.get<std::string>()).second) {
                valid = false;
            }
        } else if (event == json::parse_event_t::object_end && !keys.empty()) {
            keys.pop_back();
        }
        return true;
    }, false);
    if (!valid || result.is_discarded() || !result.is_object()) {
        error = path + " is not a JSON object";
        return false;
    }
    return true;
}

std::string ReadString(const json& root, const char* key) {
    const auto it = root.find(key);
    return it != root.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::optional<uint32_t> ReadU32(const json& root, const char* key) {
    const auto it = root.find(key);
    if (it == root.end() || !it->is_number_unsigned() || it->get<uint64_t>() > UINT32_MAX) {
        return std::nullopt;
    }
    return it->get<uint32_t>();
}

std::optional<OotmmGame> ParseGame(const json& value) {
    if (!value.is_string()) {
        return std::nullopt;
    }
    const std::string game = value.get<std::string>();
    if (game == "oot") {
        return OotmmGame::Oot;
    }
    if (game == "mm") {
        return OotmmGame::Mm;
    }
    return std::nullopt;
}

std::string SanitizePathComponent(const std::string& source, size_t limit) {
    std::string result;
    result.reserve(std::min(source.size(), limit));
    for (const char c : source) {
        const bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                           (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (valid) {
            result.push_back(c);
        }
        if (result.size() == limit) {
            break;
        }
    }
    return result;
}

} // namespace

bool OotmmEntranceMapping::IsCrossGame() const {
    return FromGame != ToGame;
}

bool OotmmGameState::LoadFromEnvironment() {
    const char* path = std::getenv("OOTMM_BOOT_CONFIG");
    if (path == nullptr || path[0] == '\0') {
        Reset();
        return false;
    }
    return LoadFromFile(path);
}

bool OotmmGameState::LoadFromFile(const std::string& path) {
    Reset();

    json root;
    if (!ReadJsonFile(path, root, mLastError, true)) {
        return false;
    }

    mBoot.SaveId = ReadString(root, "saveId");
    mBoot.SeedPath = ReadString(root, "seedPath");
    mBoot.StatePath = ReadString(root, "statePath");
    mBoot.InboxPath = ReadString(root, "inboxPath");
    mBoot.OutboxPath = ReadString(root, "outboxPath");
    mBoot.PresenceOutPath = ReadString(root, "presenceOutPath");
    mBoot.PresenceInPath = ReadString(root, "presenceInPath");
    mBoot.MmSaveDir = ReadString(root, "mmSaveDir");
    mBoot.NativeSaveSubdir = SanitizePathComponent(ReadString(root, "nativeSaveSubdir"), 32);

    const auto allowDebug = root.find("allowDebugMenus");
    if (allowDebug != root.end() && allowDebug->is_boolean()) {
        mBoot.AllowDebugMenus = allowDebug->get<bool>();
    }
    if (const auto fileSelect = root.find("openFileSelect"); fileSelect != root.end()) {
        if (!fileSelect->is_boolean()) {
            mLastError = "boot config openFileSelect must be boolean";
            return false;
        }
        mBoot.OpenFileSelect = fileSelect->get<bool>();
    }
    const auto protocol = root.find("multiworldProtocol");
    if (protocol != root.end() && protocol->is_number_integer()) {
        const int value = protocol->get<int>();
        if (value >= 0) {
            mBoot.MultiworldProtocol = value;
        }
    }
    const auto bootGame = root.find("bootGame");
    if (bootGame != root.end()) {
        if (const auto game = ParseGame(*bootGame)) {
            mBoot.BootGame = *game;
        }
    }

    mBoot.BootEntrance = ReadU32(root, "bootEntrance");
    mBoot.OotAge = ReadU32(root, "ootAge");
    mBoot.LogicalSlot = ReadU32(root, "logicalSlot");
    if (root.contains("logicalSlot") && (!mBoot.LogicalSlot.has_value() || *mBoot.LogicalSlot > 2)) {
        mLastError = "boot config logicalSlot must be between 0 and 2";
        return false;
    }

    if (const auto recovery = root.find("mmRecovery"); recovery != root.end()) {
        if (mBoot.BootGame != OotmmGame::Mm || !mBoot.LogicalSlot.has_value() || mBoot.SaveId.size() != 32 ||
            !std::all_of(mBoot.SaveId.begin(), mBoot.SaveId.end(), [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            }) || !(mBoot.MmRecovery = OotmmRecoveryRequest::Parse(recovery->dump())).has_value()) {
            mLastError = "boot config has invalid MM recovery request";
            return false;
        }
    }

    const auto parentWindow = root.find("parentWindow");
    if (const auto shared = root.find("sharedResources"); shared != root.end()) {
        if (!mBoot.LogicalSlot.has_value() ||
            !(mBoot.SharedResources = OotmmSharedResources::Parse(shared->dump(), mBoot.SaveId,
                                                                 *mBoot.LogicalSlot)).has_value()) {
            mLastError = "boot config has invalid shared resources";
            return false;
        }
    }
    if (parentWindow != root.end()) {
        if (parentWindow->is_number_unsigned()) {
            mBoot.ParentWindow = parentWindow->get<uint64_t>();
        } else if (parentWindow->is_string()) {
            try {
                mBoot.ParentWindow = std::stoull(parentWindow->get<std::string>());
            } catch (const std::exception&) {
                mBoot.ParentWindow.reset();
            }
        }
    }

    if (!mBoot.SeedPath.empty() && !LoadSeed()) {
        return false;
    }

    mActive = true;
    return true;
}

void OotmmGameState::Reset() {
    mBoot = {};
    mSeedId.clear();
    mNativeSaveTag.clear();
    mPlayerId = 1;
    mPlayerCount = 1;
    mStartingGame.reset();
    mBoolSettings.clear();
    mStringSettings.clear();
    mNumberSettings.clear();
    mSpecialConds.clear();
    mWorldFlags.clear();
    mAllWorldFlags.clear();
    mPreCompletedDungeons.clear();
    mCustomItems.Reset();
    mSeedItems.clear();
    mEntrances.clear();
    mEntrancesSpawn.clear();
    mEntrancesOwl.clear();
    mEntrancesSong.clear();
    mPrices.clear();
    mBossIds.clear();
    mDungeonEntrances.clear();
    mDungeonWarps.clear();
    mSongEventsOot.clear();
    mSongEventsMm.clear();
    mHints = {};
    mPlacements.clear();
    mPlacementsById.clear();
    mPlacementsByLocation.clear();
    mPlacementsByNativeId.clear();
    mLastError.clear();
    mActive = false;
    mHasSeed = false;
}

bool OotmmGameState::IsActive() const {
    return mActive;
}

bool OotmmGameState::HasSeed() const {
    return mHasSeed;
}

const OotmmBootConfig& OotmmGameState::GetBootConfig() const {
    return mBoot;
}

const std::string& OotmmGameState::GetSeedId() const {
    return mSeedId;
}

const std::string& OotmmGameState::GetNativeSaveTag() const {
    return mNativeSaveTag;
}

uint32_t OotmmGameState::GetPlayerId() const {
    return mPlayerId;
}

uint32_t OotmmGameState::GetPlayerCount() const {
    return mPlayerCount;
}

std::optional<OotmmGame> OotmmGameState::GetStartingGame() const {
    return mStartingGame;
}

bool OotmmGameState::GetBoolSetting(const std::string& name, bool fallback) const {
    const auto it = mBoolSettings.find(name);
    return it == mBoolSettings.end() ? fallback : it->second;
}

uint16_t OotmmGameState::GetBlastMaskCooldownFrames() const {
    const std::string value = GetStringSetting("blastMaskCooldown", "default");
    if (value == "instant") {
        return 0x001;
    }
    if (value == "veryshort") {
        return 0x020;
    }
    if (value == "short") {
        return 0x080;
    }
    if (value == "long") {
        return 0x200;
    }
    if (value == "verylong") {
        return 0x400;
    }
    return 0x136;
}

std::string OotmmGameState::GetStringSetting(const std::string& name, std::string fallback) const {
    const auto it = mStringSettings.find(name);
    return it == mStringSettings.end() ? std::move(fallback) : it->second;
}

int OotmmGameState::GetIntSetting(const std::string& name, int fallback) const {
    const auto it = mNumberSettings.find(name);
    return it == mNumberSettings.end() ? fallback : static_cast<int>(it->second);
}

bool OotmmGameState::IsSpecialCondMet(const std::string& name, const OotmmInventory& inventory) const {
    const auto it = mSpecialConds.find(name);
    if (it == mSpecialConds.end()) {
        return false;
    }
    uint64_t total = 0;
    for (const auto& item : it->second.Items) {
        total += inventory.Count(item);
    }
    for (const auto& item : it->second.ItemsUnique) {
        total += inventory.Has(item) ? 1 : 0;
    }
    return total >= it->second.Count;
}

bool OotmmGameState::HasWorldFlag(const std::string& name) const {
    return mAllWorldFlags.contains(name) || mWorldFlags.contains(name);
}

bool OotmmGameState::WorldFlagContains(const std::string& name, const std::string& member) const {
    if (mAllWorldFlags.contains(name)) {
        return true;
    }
    const auto flag = mWorldFlags.find(name);
    return flag != mWorldFlags.end() && flag->second.contains(member);
}

bool OotmmGameState::IsDungeonPreCompleted(const std::string& dungeon) const {
    return mPreCompletedDungeons.contains(dungeon);
}

const OotmmItemCatalog& OotmmGameState::GetCustomItemCatalog() const {
    return mCustomItems;
}

const std::vector<OotmmSeedItem>& OotmmGameState::GetSeedItems() const {
    return mSeedItems;
}

const std::vector<OotmmEntranceMapping>& OotmmGameState::GetEntrances() const {
    return mEntrances;
}

const OotmmEntranceMapping* OotmmGameState::FindEntrance(OotmmGame game, uint32_t nativeId) const {
    const auto mapping = std::find_if(mEntrances.begin(), mEntrances.end(), [game, nativeId](const auto& entrance) {
        return entrance.FromGame == game && entrance.FromNativeId == nativeId;
    });
    return mapping == mEntrances.end() ? nullptr : &*mapping;
}

std::optional<uint32_t> OotmmGameState::GetOwlEntrance(uint32_t owl) const {
    return owl < mEntrancesOwl.size() ? std::optional<uint32_t>(mEntrancesOwl[owl]) : std::nullopt;
}

std::optional<uint16_t> OotmmGameState::GetPrice(uint32_t index) const {
    return index < mPrices.size() ? std::optional<uint16_t>(mPrices[index]) : std::nullopt;
}

std::optional<uint32_t> OotmmGameState::GetSongEntrance(uint32_t song) const {
    return song < mEntrancesSong.size() ? std::optional<uint32_t>(mEntrancesSong[song]) : std::nullopt;
}

std::optional<uint32_t> OotmmGameState::GetSpawnEntrance(uint32_t ootAge) const {
    if (ootAge >= 2 || mEntrancesSpawn.size() != 2) {
        return std::nullopt;
    }
    return mEntrancesSpawn[ootAge];
}

std::optional<uint32_t> OotmmGameState::GetBossDungeon(uint32_t bossId) const {
    constexpr uint32_t kBossCount = 12;
    if (bossId >= kBossCount || (!mBossIds.empty() && mBossIds.size() != kBossCount)) {
        return std::nullopt;
    }
    return mBossIds.empty() ? bossId : mBossIds[bossId];
}

std::optional<uint32_t> OotmmGameState::ResolveBossWarp(uint32_t bossId, bool woodfallTempleRaised) const {
    constexpr uint32_t kBossCount = 12;
    constexpr uint32_t kForeignEntrance = 0x80000000;
    if (bossId >= kBossCount || mDungeonWarps.size() != kBossCount) {
        return std::nullopt;
    }

    if (!mBossIds.empty() && mBossIds.size() != kBossCount) {
        return std::nullopt;
    }
    const uint32_t dungeonId = mBossIds.empty() ? bossId : mBossIds[bossId];
    if (dungeonId >= mDungeonWarps.size()) {
        return std::nullopt;
    }

    switch (const uint32_t destination = mDungeonWarps[dungeonId]) {
        case 0x0209:
            return 0x0457;
        case 0x0215:
            return 0x0600;
        case 0x024A:
            return 0x04F6;
        case 0x021D:
            return 0x0604;
        case 0x0205:
            return 0x0568;
        case kForeignEntrance | 0x8610:
            return kForeignEntrance | (woodfallTempleRaised ? 0x3010 : 0x8600);
        case kForeignEntrance | 0xB210:
            return kForeignEntrance | 0x9A70;
        case kForeignEntrance | 0xAA20:
        case kForeignEntrance | 0xAC10:
            return kForeignEntrance | 0x2070;
        default:
            return destination;
    }
}

const std::vector<uint8_t>& OotmmGameState::GetSongEventsOot() const {
    return mSongEventsOot;
}

const std::vector<uint8_t>& OotmmGameState::GetSongEventsMm() const {
    return mSongEventsMm;
}

std::string OotmmHintLayout(std::string text) {
    constexpr int kMaxLineLength = 37;
    int lastSpace = -1;
    int lineLength = 0;
    int lineCount = 0;
    for (int i = 0; i < static_cast<int>(text.size()); ++i) {
        const char c = text[i];
        if (c == '%' && i + 1 < static_cast<int>(text.size())) {
            ++i;
            continue;
        }
        if (c >= ' ' && c <= '~') {
            ++lineLength;
            if (lineLength >= kMaxLineLength && lastSpace != -1) {
                lineLength = i - lastSpace;
                if (lineCount < 3) {
                    text[lastSpace] = '\n';
                    ++lineCount;
                } else {
                    text[lastSpace] = '\f';
                    lineCount = 0;
                }
                lastSpace = -1;
            }
        }
        if (c == ' ') {
            lastSpace = i;
        } else if (c == '\n') {
            lastSpace = i;
            lineLength = 999;
        } else if (c == '\f') {
            lastSpace = -1;
            lineLength = 0;
            lineCount = 0;
        }
    }
    return text;
}

const std::string& OotmmHints::StaticReward(OotmmStaticReward reward) const {
    static const std::string kNone;
    const auto index = static_cast<size_t>(reward);
    return index < StaticRewards.size() ? StaticRewards[index] : kNone;
}

namespace {
constexpr const char* kRegionNamesOot[][2] = {
    { "in", "the %ySacred Realm" },
    { "inside", "the %gDeku Tree" },
    { "inside", "%rDodongo's Cavern" },
    { "inside", "%bJabu-Jabu" },
    { "in", "the %gForest Temple" },
    { "in", "the %rFire Temple" },
    { "in", "the %bWater Temple" },
    { "in", "the %ySpirit Temple" },
    { "in", "the %pShadow Temple" },
    { "at", "the %pBottom of the Well" },
    { "in", "the %cIce Cavern" },
    { "in", "%yGerudo Training Grounds" },
    { "in", "the %yThieve's Hideout" },
    { "inside", "%rGanon's Castle" },
    { "in", "the %gKokiri Forest" },
    { "in", "%yHyrule Field" },
    { "in", "%yHyrule's Market" },
    { "in", "%yLon Lon Ranch" },
    { "in", "%yHyrule's Castle" },
    { "in", "%rGanon's Castle Exterior" },
    { "in", "the %gLost Woods" },
    { "in", "the %gSacred Meadow" },
    { "in", "%rKakariko" },
    { "inside", "%pKakariko's Graveyard" },
    { "on", "%rDeath Mountain's Trail" },
    { "in", "%rDeath Mountain's Crater" },
    { "in", "%rGoron City" },
    { "in", "%bZora's River" },
    { "in", "%bZora's Domain" },
    { "in", "%bZora's Fountain" },
    { "around", "%bLake Hylia" },
    { "in", "the %yTemple of Time" },
    { "in", "%yGerudo Valley" },
    { "around", "%yGerudo Fortress" },
    { "in", "the %yHaunted Wastelands" },
    { "around", "the %yDesert Colossus" },
    { "inside", "an %yEgg" },
    { "inside", "%rGanon's Castle Tower" },
};
constexpr const char* kRegionNamesMm[][2] = {
    { "in", "%gWoodfall Temple" },
    { "in", "%cSnowhead Temple" },
    { "in", "%bGreat Bay Temple" },
    { "in", "%yStone Tower Temple" },
    { "in", "%ySouth Clock Town" },
    { "in", "%yNorth Clock Town" },
    { "in", "%yEast Clock Town" },
    { "in", "%yWest Clock Town" },
    { "in", "the %yLaundry Pool" },
    { "inside", "the %pGiant's Dream" },
    { "on", "%yClock Tower Roof" },
    { "in", "the %rStock Pot Inn" },
    { "in", "%rTermina Field" },
    { "on", "the %gRoad to the Swamp" },
    { "in", "the %gSouthern Swamp" },
    { "in", "%gDeku Palace" },
    { "in", "%gWoodfall" },
    { "on", "the %rPath to Mountain Village" },
    { "in", "the %rMountain Village" },
    { "on", "the %rPath to Snowhead" },
    { "around", "%rTwin Islands" },
    { "around", "%rGoron Village" },
    { "in", "%rSnowhead" },
    { "around", "the %pMilk Road" },
    { "around", "%yRomani Ranch" },
    { "around", "%bGreat Bay Coast" },
    { "in", "the %bPirate's Fortress Exterior" },
    { "in", "the %bPirate's Fortress Sewers" },
    { "in", "the %bPirate's Fortress Interior" },
    { "around", "%bZora Cape" },
    { "in", "%bZora Hall" },
    { "in", "%bPinnacle Rock" },
    { "on", "the %yRoad to Ikana" },
    { "in", "%pIkana's Graveyard" },
    { "in", "%yIkana Canyon" },
    { "in", "the %yAncient Castle of Ikana" },
    { "somewhere", "%pBeneath The Well" },
    { "in", "a %ySecret Shrine" },
    { "on", "the %yStone Tower" },
    { "on", "the %rMoon" },
    { "in", "the %gSwamp Spider House" },
    { "in", "the %bOcean Spider House" },
    { "from", "%gTingle" },
    { "in", "%yInverted Stone Tower Temple" },
    { "inside", "the %gButler Race" },
    { "inside", "the %rGoron Racetrack" },
};
constexpr const char* kDungeonNames[] = {
    "%gDeku Tree%w",
    "%rDodongo's Cavern%w",
    "%bJabu-Jabu%w",
    "%gForest Temple%w",
    "%rFire Temple%w",
    "%bWater Temple%w",
    "%pShadow Temple%w",
    "%ySpirit Temple%w",
    "%gWoodfall Temple%w",
    "%rSnowhead Temple%w",
    "%bGreat Bay Temple%w",
    "%yStone Tower Temple (Inverted)%w",
    "%yStone Tower Temple%w",
    "%gSwamp Spider House%w",
    "%bOcean Spider House%w",
    "%pBottom of the Well%w",
    "%bIce Cavern%w",
    "%yGerudo Training Grounds%w",
    "%pBeneath The Well%w",
    "%yIkana Castle%w",
    "%ySecret Shrine%w",
    "%pBeneath The Well (End)%w",
    "%bPirate Fortress%w",
    "%rGanon's Castle%w",
    "%rGanon's Tower%w",
    "%yClock Tower Roof%w",
};
constexpr const char* kBossNames[] = {
    "%gGohma%w",
    "%rKing Dodongo%w",
    "%bBarinade%w",
    "%gPhantom Ganon%w",
    "%rVolvagia%w",
    "%bMorpha%w",
    "%pBongo Bongo%w",
    "%yTwinrova%w",
    "%gOdolwa%w",
    "%rGoht%w",
    "%bGyorg%w",
    "%yTwinmold%w",
};

constexpr uint32_t kDungeonStoneTowerInverted = 0x0B;
constexpr uint32_t kDungeonStoneTower = 0x0C;

int DungeonOfItem(const std::string& suffix) {
    static const std::unordered_map<std::string, int> kDungeons = {
        { "OOT_DT", 0x00 },     { "OOT_DC", 0x01 },     { "OOT_JJ", 0x02 },    { "OOT_FOREST", 0x03 },
        { "OOT_FIRE", 0x04 },   { "OOT_WATER", 0x05 },  { "OOT_SHADOW", 0x06 }, { "OOT_SPIRIT", 0x07 },
        { "OOT_BOTW", 0x0F },   { "OOT_IC", 0x10 },     { "MM_WF", 0x08 },     { "MM_SH", 0x09 },
        { "MM_GB", 0x0A },      { "MM_ST", 0x0C },
    };
    const auto found = kDungeons.find(suffix);
    return found == kDungeons.end() ? -1 : found->second;
}

std::string RegionText(uint32_t regionId) {
    const uint8_t id = static_cast<uint8_t>(regionId);
    if (id == 0x00) {
        return "%pnowhere%w";
    }
    if (id == 0xFF) {
        return "inside %yLink's Pocket%w";
    }
    if (id == 0xFE) {
        return "in a %pNameless Place%w";
    }
    const size_t index = static_cast<size_t>(id & 0x7F) - 1;
    if (id & 0x80) {
        if (index >= std::size(kRegionNamesMm)) {
            return {};
        }
        return std::string(kRegionNamesMm[index][0]) + " " + kRegionNamesMm[index][1] + "%w";
    }
    if (index >= std::size(kRegionNamesOot)) {
        return {};
    }
    return std::string(kRegionNamesOot[index][0]) + " " + kRegionNamesOot[index][1] + "%w";
}
} // namespace

std::string OotmmGameState::DungeonEntranceText(uint32_t dungeonId) const {
    if (dungeonId >= mDungeonEntrances.size()) {
        return {};
    }
    const OotmmDungeonEntrance& entry = mDungeonEntrances[dungeonId];
    if (!entry.Replace) {
        return RegionText(entry.Value);
    }
    return entry.Value < std::size(kDungeonNames) ? std::string("at ") + kDungeonNames[entry.Value] : std::string();
}

std::string OotmmGameState::DungeonItemText(const std::string& itemId) const {
    const auto gameEnd = itemId.find('_');
    if (gameEnd == std::string::npos) {
        return {};
    }
    const std::string game = itemId.substr(0, gameEnd);
    const std::string kind = itemId.substr(gameEnd + 1);
    if (kind.rfind("MAP_", 0) == 0) {
        const int dungeon = DungeonOfItem(game + "_" + kind.substr(4));
        if (dungeon < 0 || GetStringSetting("erDungeons", "none") == "none") {
            return {};
        }
        if (static_cast<uint32_t>(dungeon) == kDungeonStoneTower) {
            return "\nThe entrances are " + DungeonEntranceText(kDungeonStoneTower) + " and " +
                   DungeonEntranceText(kDungeonStoneTowerInverted) + ".";
        }
        return "\nThe entrance is " + DungeonEntranceText(static_cast<uint32_t>(dungeon)) + ".";
    }
    if (kind.rfind("COMPASS_", 0) == 0) {
        int dungeon = DungeonOfItem(game + "_" + kind.substr(8));
        if (dungeon < 0 || GetStringSetting("erBoss", "none") == "none") {
            return {};
        }
        if (static_cast<uint32_t>(dungeon) == kDungeonStoneTower) {
            dungeon = kDungeonStoneTowerInverted;
        }
        for (size_t boss = 0; boss < mBossIds.size() && boss < std::size(kBossNames); boss++) {
            if (mBossIds[boss] == dungeon) {
                return std::string("\nIt points at ") + kBossNames[boss] + ".";
            }
        }
    }
    return {};
}

const OotmmHints& OotmmGameState::GetHints() const {
    return mHints;
}

std::string OotmmGameState::GetGossipHint(OotmmGame game, uint32_t key) const {
    const auto& stones = game == OotmmGame::Oot ? mHints.GossipOot : mHints.GossipMm;
    if (const auto it = stones.find(key); it != stones.end() && !it->second.empty()) {
        return it->second;
    }
    if (mHints.Junk.empty()) {
        return {};
    }
    return mHints.Junk[key % mHints.Junk.size()];
}

const std::vector<OotmmPlacement>& OotmmGameState::GetPlacements() const {
    return mPlacements;
}

const OotmmPlacement* OotmmGameState::FindCheckById(OotmmGame game, const std::string& checkId) const {
    const char gameTag = game == OotmmGame::Oot ? 'o' : 'm';
    const auto it = mPlacementsById.find(gameTag + checkId);
    return it == mPlacementsById.end() ? nullptr : &mPlacements[it->second];
}

const OotmmPlacement* OotmmGameState::FindCheck(OotmmGame game, uint32_t scene, const std::string& type,
                                                uint32_t flag) const {
    const char gameTag = game == OotmmGame::Oot ? 'o' : 'm';
    const auto it = mPlacementsByLocation.find(gameTag + std::to_string(scene) + ':' + type + ':'
                                               + std::to_string(flag));
    return it == mPlacementsByLocation.end() ? nullptr : &mPlacements[it->second];
}

const OotmmPlacement* OotmmGameState::FindCheck(OotmmGame game, const std::string& type,
                                                const std::string& nativeId) const {
    const char gameTag = game == OotmmGame::Oot ? 'o' : 'm';
    const auto it = mPlacementsByNativeId.find(gameTag + type + ':' + nativeId);
    return it == mPlacementsByNativeId.end() ? nullptr : &mPlacements[it->second];
}

const std::string& OotmmGameState::GetLastError() const {
    return mLastError;
}

bool OotmmGameState::LoadSeed() {
    json root;
    if (!ReadJsonFile(mBoot.SeedPath, root, mLastError)) {
        return false;
    }
    if (ReadString(root, "format") != "ootmm.pc.seed.v2") {
        mLastError = "seed is not compatible with ootmm.pc.seed.v2";
        return false;
    }
    if (ReadString(root, "generatorVersion") != "31.1") {
        mLastError = "seed was not generated by OoTMM 31.1";
        return false;
    }

    mSeedId = ReadString(root, "seedId");
    if (mSeedId.empty()) {
        mLastError = "seedId is missing";
        return false;
    }

    const std::string settingsHash = ReadString(root, "settingsHash");
    mNativeSaveTag = SanitizePathComponent(settingsHash.empty() ? mSeedId : settingsHash, 32);
    if (mNativeSaveTag.empty()) {
        mLastError = "seed does not have a filesystem-safe save tag";
        return false;
    }

    uint32_t sessionEpoch = 1;
    if (const auto it = root.find("playerId"); it != root.end() && it->is_number_unsigned()) {
        mPlayerId = it->get<uint32_t>();
    }
    if (const auto it = root.find("playerCount"); it != root.end() && it->is_number_unsigned()) {
        mPlayerCount = it->get<uint32_t>();
    }
    if (const auto it = root.find("sessionEpoch"); it != root.end()) {
        if (it->is_number_unsigned()) {
            sessionEpoch = it->get<uint32_t>();
        } else if (it->is_string()) {
            const std::string text = it->get<std::string>();
            if (!text.empty() && text.find_first_not_of("0123456789") == std::string::npos) {
                sessionEpoch = static_cast<uint32_t>(std::strtoull(text.c_str(), nullptr, 10));
            }
        }
    }
    mPlayerCount = std::max(mPlayerCount, 1u);
    mPlayerId = std::clamp(mPlayerId, 1u, mPlayerCount);
    if (mPlayerCount > 1) {
        mNativeSaveTag += "_p" + std::to_string(mPlayerId);
    }
    if (sessionEpoch > 1) {
        mNativeSaveTag += "_e" + std::to_string(sessionEpoch);
    }

    const auto startingGame = root.find("startingGame");
    if (startingGame != root.end()) {
        mStartingGame = ParseGame(*startingGame);
    }

    const auto settings = root.find("settings");
    if (settings != root.end() && settings->is_object()) {
        for (auto it = settings->begin(); it != settings->end(); ++it) {
            if (it->is_boolean()) {
                mBoolSettings.emplace(it.key(), it->get<bool>());
            } else if (it->is_string()) {
                mStringSettings.emplace(it.key(), it->get<std::string>());
            } else if (it->is_number()) {
                mNumberSettings.emplace(it.key(), it->get<double>());
            }
        }
    }

    const auto logic = root.find("logic");
    if (logic != root.end() && logic->is_object()) {
        const auto specials = logic->find("specials");
        if (specials != logic->end() && specials->is_object()) {
            for (auto it = specials->begin(); it != specials->end(); ++it) {
                if (!it->is_object()) {
                    continue;
                }
                OotmmSpecialCond cond;
                const auto readItems = [&](const char* key, std::vector<std::string>& out) {
                    const auto items = it->find(key);
                    if (items != it->end() && items->is_array()) {
                        for (const auto& item : *items) {
                            if (item.is_string()) {
                                out.push_back(item.get<std::string>());
                            }
                        }
                    }
                };
                readItems("items", cond.Items);
                readItems("itemsUnique", cond.ItemsUnique);
                const auto count = it->find("count");
                if (count != it->end() && count->is_number_unsigned()) {
                    cond.Count = count->get<uint32_t>();
                }
                mSpecialConds.emplace(it.key(), std::move(cond));
            }
        }
    }

    const auto worldFlags = root.find("worldFlags");
    if (worldFlags != root.end() && worldFlags->is_object()) {
        for (auto it = worldFlags->begin(); it != worldFlags->end(); ++it) {
            if (it->is_string() && it->get<std::string>() == "all") {
                mAllWorldFlags.emplace(it.key());
            } else if (it->is_array()) {
                auto& members = mWorldFlags[it.key()];
                for (const auto& member : *it) {
                    if (member.is_string()) {
                        members.emplace(member.get<std::string>());
                    }
                }
            }
        }
    }

    const auto dungeonEntrances = root.find("dungeonEntrances");
    if (dungeonEntrances != root.end() && dungeonEntrances->is_array()) {
        for (const auto& entry : *dungeonEntrances) {
            OotmmDungeonEntrance parsed;
            parsed.Replace = ReadString(entry, "type") == "replace";
            parsed.Value = entry.value(parsed.Replace ? "entranceIndex" : "regionId", 0u);
            mDungeonEntrances.push_back(parsed);
        }
    }

    const auto preCompleted = root.find("preCompletedDungeons");
    if (preCompleted != root.end() && preCompleted->is_array()) {
        for (const auto& dungeon : *preCompleted) {
            if (dungeon.is_string()) {
                mPreCompletedDungeons.emplace(dungeon.get<std::string>());
            }
        }
    }

    std::unordered_set<std::string> seedItemIds;
    const auto addItems = [this, &seedItemIds](const json& entries) {
        if (!entries.is_array()) {
            return;
        }
        for (const auto& entry : entries) {
            if (entry.is_object()) {
                const std::string id = ReadString(entry, "itemId");
                const std::string name = ReadString(entry, "itemName");
                const std::string game = ReadString(entry, "itemGame");
                mCustomItems.Add(id, name);
                if (!id.empty() && seedItemIds.emplace(id).second) {
                    mSeedItems.push_back({ id, name.empty() ? id : name, game });
                }
            }
        }
    };
    if (const auto placements = root.find("placements"); placements != root.end()) {
        addItems(*placements);
        if (placements->is_array()) {
            const auto readU32 = [](const json& entry, const char* key) -> std::optional<uint32_t> {
                const auto it = entry.find(key);
                if (it == entry.end() || !it->is_number()) {
                    return std::nullopt;
                }
                return it->get<uint32_t>();
            };
            for (const auto& entry : *placements) {
                if (!entry.is_object()) {
                    continue;
                }
                const auto checkGameIt = entry.find("checkGame");
                std::optional<OotmmGame> checkGame;
                if (checkGameIt != entry.end()) {
                    checkGame = ParseGame(*checkGameIt);
                }
                OotmmPlacement placement;
                placement.CheckId = ReadString(entry, "checkId");
                if (!checkGame.has_value() || placement.CheckId.empty()) {
                    continue;
                }
                placement.CheckGame = *checkGame;
                placement.CheckName = ReadString(entry, "checkName");
                placement.CheckScene = readU32(entry, "checkScene");
                placement.CheckType = ReadString(entry, "checkType");
                if (const auto nativeIt = entry.find("checkNativeId"); nativeIt != entry.end()) {
                    if (nativeIt->is_string()) {
                        placement.CheckNativeId = nativeIt->get<std::string>();
                    } else if (nativeIt->is_number_unsigned()) {
                        placement.CheckNativeId = std::to_string(nativeIt->get<uint64_t>());
                    }
                }
                placement.CheckFlag = readU32(entry, "checkFlag");
                placement.ItemGame = ReadString(entry, "itemGame");
                placement.ItemId = ReadString(entry, "itemId");
                placement.ItemName = ReadString(entry, "itemName");
                placement.CloakItemId = ReadString(entry, "cloakItemId");
                placement.CloakItemName = ReadString(entry, "cloakItemName");
                placement.OwnerPlayer = readU32(entry, "ownerPlayer").value_or(0);
                placement.SourcePlayer = readU32(entry, "sourcePlayer").value_or(0);
                placement.Major = entry.value("major", false);

                const auto index = mPlacements.size();
                const char gameTag = placement.CheckGame == OotmmGame::Oot ? 'o' : 'm';
                mPlacementsById.emplace(gameTag + placement.CheckId, index);
                if (placement.CheckScene.has_value() && placement.CheckFlag.has_value()) {
                    mPlacementsByLocation.emplace(
                        gameTag + std::to_string(*placement.CheckScene) + ':' + placement.CheckType
                            + ':' + std::to_string(*placement.CheckFlag),
                        index);
                }
                if (!placement.CheckNativeId.empty()) {
                    mPlacementsByNativeId.emplace(gameTag + placement.CheckType + ':' + placement.CheckNativeId,
                                                  index);
                }
                mPlacements.push_back(std::move(placement));
            }
        }
    }
    if (const auto startingItems = root.find("startingItems"); startingItems != root.end()) {
        addItems(*startingItems);
    }

    const auto readU32Array = [this, &root](const char* key, auto& out, size_t count, uint64_t maximum) {
        const auto values = root.find(key);
        if (values == root.end()) {
            return true;
        }
        if (!values->is_array() || values->size() != count) {
            mLastError = std::string(key) + " must contain exactly " + std::to_string(count) + " entries";
            return false;
        }
        for (const auto& value : *values) {
            using Value = typename std::decay_t<decltype(out)>::value_type;
            if (!value.is_number_unsigned() || value.get<uint64_t>() > maximum) {
                mLastError = std::string(key) + " contains an invalid entry at index " + std::to_string(out.size());
                return false;
            }
            out.push_back(static_cast<Value>(value.get<uint32_t>()));
        }
        return true;
    };
    if (!readU32Array("prices", mPrices, 141, std::numeric_limits<uint16_t>::max()) ||
        !readU32Array("entrancesOwl", mEntrancesOwl, 10, std::numeric_limits<uint32_t>::max()) ||
        !readU32Array("entrancesSong", mEntrancesSong, 6, std::numeric_limits<uint32_t>::max()) ||
        !readU32Array("bossIds", mBossIds, 12, 11) ||
        !readU32Array("entrancesSpawn", mEntrancesSpawn, 2, std::numeric_limits<uint32_t>::max()) ||
        !readU32Array("dungeonWarps", mDungeonWarps, 12, std::numeric_limits<uint32_t>::max())) {
        return false;
    }
    if (std::unordered_set<uint8_t>(mBossIds.begin(), mBossIds.end()).size() != mBossIds.size()) {
        mLastError = "bossIds must assign each dungeon exactly once";
        return false;
    }

    const auto readSongEvents = [&root](const char* key, std::vector<uint8_t>& out) {
        const auto events = root.find(key);
        if (events == root.end() || !events->is_array()) {
            return;
        }
        for (const auto& value : *events) {
            if (value.is_number_unsigned()) {
                out.push_back(static_cast<uint8_t>(value.get<uint32_t>()));
            }
        }
    };
    readSongEvents("songEventsOot", mSongEventsOot);
    readSongEvents("songEventsMm", mSongEventsMm);

    if (const auto hints = root.find("hints"); hints != root.end() && hints->is_object()) {
        const auto readRegion = [](const json& value) {
            OotmmHintRegion region;
            if (value.is_object()) {
                region.RegionId = value.value("regionId", 0u);
                region.Text = ReadString(value, "text");
            }
            return region;
        };
        const auto readRegions = [&](const char* key) {
            std::vector<OotmmHintRegion> regions;
            if (const auto values = hints->find(key); values != hints->end() && values->is_array()) {
                for (const auto& value : *values) {
                    regions.push_back(readRegion(value));
                }
            }
            return regions;
        };
        const auto readStrings = [&](const char* key) {
            std::vector<std::string> strings;
            if (const auto values = hints->find(key); values != hints->end() && values->is_array()) {
                for (const auto& value : *values) {
                    strings.push_back(value.is_string() ? value.get<std::string>() : std::string());
                }
            }
            return strings;
        };
        if (const auto gossip = hints->find("gossip"); gossip != hints->end() && gossip->is_array()) {
            for (const auto& entry : *gossip) {
                if (!entry.is_object() || !entry.contains("stoneKey") || !entry["stoneKey"].is_number_unsigned()) {
                    continue;
                }
                const auto game = ParseGame(entry.value("stoneGame", json()));
                if (!game.has_value()) {
                    continue;
                }
                auto& stones = *game == OotmmGame::Oot ? mHints.GossipOot : mHints.GossipMm;
                stones[entry["stoneKey"].get<uint32_t>()] = ReadString(entry, "text");
            }
        }
        mHints.Junk = readStrings("junk");
        mHints.DungeonRewards = readRegions("dungeonRewards");
        mHints.OathToOrder = readRegions("oathToOrder");
        mHints.StaticRewards = readStrings("staticRewards");
        if (const auto altar = readStrings("altarRewards"); mHints.StaticRewards.empty() && altar.size() >= 2) {
            mHints.StaticRewards.resize(static_cast<size_t>(OotmmStaticReward::SongOfTime) + 1);
            mHints.StaticRewards[static_cast<size_t>(OotmmStaticReward::OcarinaOfTime)] = altar[0];
            mHints.StaticRewards[static_cast<size_t>(OotmmStaticReward::SongOfTime)] = altar[1];
        }
        if (const auto value = hints->find("lightArrow"); value != hints->end()) {
            mHints.LightArrow = readRegion(*value);
        }
        if (const auto value = hints->find("ganonBossKey"); value != hints->end()) {
            mHints.GanonBossKey = readRegion(*value);
        }
        if (const auto conds = hints->find("specialConds"); conds != hints->end() && conds->is_object()) {
            for (auto it = conds->begin(); it != conds->end(); ++it) {
                if (it->is_string()) {
                    mHints.SpecialConds[it.key()] = it->get<std::string>();
                }
            }
        }
    }

    const auto entrances = root.find("entrances");
    if (entrances != root.end() && entrances->is_array()) {
        for (const auto& value : *entrances) {
            if (!value.is_object()) {
                continue;
            }
            const auto fromGame = value.find("fromGame");
            const auto toGame = value.find("toGame");
            if (fromGame == value.end() || toGame == value.end()) {
                continue;
            }
            const auto parsedFromGame = ParseGame(*fromGame);
            const auto parsedToGame = ParseGame(*toGame);
            if (!parsedFromGame.has_value() || !parsedToGame.has_value()) {
                continue;
            }

            OotmmEntranceMapping mapping;
            mapping.FromGame = *parsedFromGame;
            mapping.From = ReadString(value, "from");
            mapping.FromNativeId = ReadU32(value, "fromNativeId");
            mapping.ToGame = *parsedToGame;
            mapping.To = ReadString(value, "to");
            mapping.ToNativeId = ReadU32(value, "toNativeId");
            mEntrances.emplace_back(std::move(mapping));
        }
    }

    mHasSeed = true;
    return true;
}

} // namespace Ship
