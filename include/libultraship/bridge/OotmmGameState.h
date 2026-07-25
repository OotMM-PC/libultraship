#pragma once

#include "libultraship/bridge/OotmmInventory.h"

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
    int MultiworldProtocol = 3;
    OotmmGame BootGame = OotmmGame::Oot;
    std::optional<uint32_t> BootEntrance;
    std::optional<uint32_t> OotAge;
    std::optional<uint32_t> LogicalSlot;
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
    bool HasWorldFlag(const std::string& name) const;
    bool WorldFlagContains(const std::string& name, const std::string& member) const;
    bool IsDungeonPreCompleted(const std::string& dungeon) const;
    const OotmmItemCatalog& GetCustomItemCatalog() const;
    const std::vector<OotmmSeedItem>& GetSeedItems() const;
    const std::vector<OotmmEntranceMapping>& GetEntrances() const;
    const OotmmEntranceMapping* FindEntrance(OotmmGame game, uint32_t nativeId) const;
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
    std::unordered_map<std::string, std::unordered_set<std::string>> mWorldFlags;
    std::unordered_set<std::string> mAllWorldFlags;
    std::unordered_set<std::string> mPreCompletedDungeons;
    OotmmItemCatalog mCustomItems;
    std::vector<OotmmSeedItem> mSeedItems;
    std::vector<OotmmEntranceMapping> mEntrances;
    std::string mLastError;
    bool mActive = false;
    bool mHasSeed = false;
};

} // namespace Ship
