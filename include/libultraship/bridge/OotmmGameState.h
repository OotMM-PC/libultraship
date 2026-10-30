#pragma once

#include "libultraship/bridge/OotmmInventory.h"
#include "libultraship/bridge/OotmmSharedResources.h"
#include "libultraship/bridge/OotmmRecoveryRequest.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Ship {

enum class OotmmGame {
    Oot,
    Mm,
};

struct OotmmEntranceMapping {
    OotmmGame FromGame = OotmmGame::Oot;
    std::string From;
    std::optional<uint32_t> FromNativeId;
    OotmmGame ToGame = OotmmGame::Oot;
    std::string To;
    std::optional<uint32_t> ToNativeId;

    bool IsCrossGame() const;
};

struct OotmmSeedItem {
    std::string Id;
    std::string Name;
    std::string Game;
};

struct OotmmPlacement {
    OotmmGame CheckGame = OotmmGame::Oot;
    std::string CheckId;
    std::string CheckName;
    std::optional<uint32_t> CheckScene;
    std::string CheckType;
    std::string CheckNativeId;
    std::optional<uint32_t> CheckFlag;
    std::string ItemGame;
    std::string ItemId;
    std::string ItemName;
    std::string CloakItemId;
    std::string CloakItemName;
    uint32_t OwnerPlayer = 0;
    uint32_t SourcePlayer = 0;
    bool Major = false;

    bool IsFor(uint32_t player) const {
        return OwnerPlayer == 0 || OwnerPlayer == player;
    }
};

struct OotmmHintRegion {
    uint32_t RegionId = 0;
    std::string Text;
};

enum class OotmmStaticReward : size_t {
    Gs100,
    Gs10,
    Gs20,
    Gs30,
    Gs40,
    Gs50,
    OcarinaOfTime,
    SongOfTime,
    ZoraEggs,
    FierceDeity,
    GreatFairyWoodfall,
    GreatFairySnowhead,
    GreatFairyGreatBay,
    GreatFairyIkana,
    PowderKeg,
    Beaver1,
    Beaver2,
    SpiderHouseSwamp,
    SpiderHouseOcean,
    GreatFairyTown,
    GreatFairyTownAlt,
};

struct OotmmHints {
    std::unordered_map<uint32_t, std::string> GossipOot;
    std::unordered_map<uint32_t, std::string> GossipMm;
    std::vector<std::string> Junk;
    std::vector<OotmmHintRegion> DungeonRewards;
    OotmmHintRegion LightArrow;
    std::vector<OotmmHintRegion> OathToOrder;
    OotmmHintRegion GanonBossKey;
    std::vector<std::string> StaticRewards;
    std::unordered_map<std::string, std::string> SpecialConds;

    const std::string& StaticReward(OotmmStaticReward reward) const;
};

std::string OotmmHintLayout(std::string text);

struct OotmmSpecialCond {
    std::vector<std::string> Items;
    std::vector<std::string> ItemsUnique;
    uint32_t Count = 0;
};

struct OotmmBootConfig {
    std::string SaveId;
    std::string SeedPath;
    std::string StatePath;
    std::string InboxPath;
    std::string OutboxPath;
    std::string PresenceOutPath;
    std::string PresenceInPath;
    std::string MmSaveDir;
    std::string NativeSaveSubdir;
    bool AllowDebugMenus = false;
    bool OpenFileSelect = false;
    int MultiworldProtocol = 3;
    OotmmGame BootGame = OotmmGame::Oot;
    std::optional<uint32_t> BootEntrance;
    std::optional<uint32_t> OotAge;
    std::optional<uint32_t> LogicalSlot;
    std::optional<OotmmSharedResources> SharedResources;
    std::optional<OotmmRecoveryRequest> MmRecovery;
    std::optional<uint64_t> ParentWindow;
};

class OotmmGameState {
  public:
    bool LoadFromEnvironment();
    bool LoadFromFile(const std::string& path);
    void Reset();

    bool IsActive() const;
    bool HasSeed() const;
    const OotmmBootConfig& GetBootConfig() const;
    const std::string& GetSeedId() const;
    const std::string& GetNativeSaveTag() const;
    uint32_t GetPlayerId() const;
    uint32_t GetPlayerCount() const;
    std::optional<OotmmGame> GetStartingGame() const;
    bool GetBoolSetting(const std::string& name, bool fallback = false) const;
    std::string GetStringSetting(const std::string& name, std::string fallback = {}) const;
    int GetIntSetting(const std::string& name, int fallback = 0) const;
    uint16_t GetBlastMaskCooldownFrames() const;
    bool IsSpecialCondMet(const std::string& name, const OotmmInventory& inventory) const;
    bool HasWorldFlag(const std::string& name) const;
    bool WorldFlagContains(const std::string& name, const std::string& member) const;
    bool IsDungeonPreCompleted(const std::string& dungeon) const;
    const OotmmItemCatalog& GetCustomItemCatalog() const;
    const std::vector<OotmmSeedItem>& GetSeedItems() const;
    const std::vector<OotmmEntranceMapping>& GetEntrances() const;
    const OotmmEntranceMapping* FindEntrance(OotmmGame game, uint32_t nativeId) const;
    std::optional<uint32_t> GetSpawnEntrance(uint32_t ootAge) const;
    std::optional<uint32_t> GetOwlEntrance(uint32_t owl) const;
    std::optional<uint32_t> GetSongEntrance(uint32_t song) const;
    std::optional<uint16_t> GetPrice(uint32_t index) const;
    std::optional<uint32_t> ResolveBossWarp(uint32_t bossId, bool woodfallTempleRaised) const;
    std::optional<uint32_t> GetBossDungeon(uint32_t bossId) const;
    const std::vector<uint8_t>& GetSongEventsOot() const;
    const std::vector<uint8_t>& GetSongEventsMm() const;
    const std::vector<OotmmPlacement>& GetPlacements() const;
    const OotmmHints& GetHints() const;
    std::string GetGossipHint(OotmmGame game, uint32_t key) const;
    const OotmmPlacement* FindCheckById(OotmmGame game, const std::string& checkId) const;
    const OotmmPlacement* FindCheck(OotmmGame game, uint32_t scene, const std::string& type,
                                    uint32_t flag) const;
    const OotmmPlacement* FindCheck(OotmmGame game, const std::string& type,
                                    const std::string& nativeId) const;
    const std::string& GetLastError() const;

  private:
    bool LoadSeed();

    OotmmBootConfig mBoot;
    std::string mSeedId;
    std::string mNativeSaveTag;
    uint32_t mPlayerId = 1;
    uint32_t mPlayerCount = 1;
    std::optional<OotmmGame> mStartingGame;
    std::unordered_map<std::string, bool> mBoolSettings;
    std::unordered_map<std::string, std::string> mStringSettings;
    std::unordered_map<std::string, double> mNumberSettings;
    std::unordered_map<std::string, OotmmSpecialCond> mSpecialConds;
    std::unordered_map<std::string, std::unordered_set<std::string>> mWorldFlags;
    std::unordered_set<std::string> mAllWorldFlags;
    std::unordered_set<std::string> mPreCompletedDungeons;
    OotmmItemCatalog mCustomItems;
    std::vector<OotmmSeedItem> mSeedItems;
    std::vector<OotmmEntranceMapping> mEntrances;
    std::vector<uint32_t> mEntrancesSpawn;
    std::vector<uint32_t> mEntrancesOwl;
    std::vector<uint32_t> mEntrancesSong;
    std::vector<uint16_t> mPrices;
    std::vector<uint8_t> mBossIds;
    std::vector<uint32_t> mDungeonWarps;
    std::vector<uint8_t> mSongEventsOot;
    std::vector<uint8_t> mSongEventsMm;
    OotmmHints mHints;
    std::vector<OotmmPlacement> mPlacements;
    std::unordered_map<std::string, size_t> mPlacementsById;
    std::unordered_map<std::string, size_t> mPlacementsByLocation;
    std::unordered_map<std::string, size_t> mPlacementsByNativeId;
    std::string mLastError;
    bool mActive = false;
    bool mHasSeed = false;
};

} // namespace Ship
