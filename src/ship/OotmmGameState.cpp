#include "libultraship/bridge/OotmmGameState.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Ship {

namespace {

using json = nlohmann::json;

bool ReadJsonFile(const std::string& path, json& result, std::string& error) {
    std::ifstream stream(path);
    if (!stream) {
        error = "cannot open " + path;
        return false;
    }

    result = json::parse(stream, nullptr, false);
    if (result.is_discarded() || !result.is_object()) {
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
    if (it == root.end() || !it->is_number_unsigned()) {
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
    if (!ReadJsonFile(path, root, mLastError)) {
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
    if (mBoot.LogicalSlot.has_value() && *mBoot.LogicalSlot > 2) {
        mLastError = "boot config logicalSlot must be between 0 and 2";
        return false;
    }

    const auto parentWindow = root.find("parentWindow");
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
    mStartingGame.reset();
    mBoolSettings.clear();
    mStringSettings.clear();
    mNumberSettings.clear();
    mWorldFlags.clear();
    mAllWorldFlags.clear();
    mPreCompletedDungeons.clear();
    mEntrances.clear();
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

std::optional<OotmmGame> OotmmGameState::GetStartingGame() const {
    return mStartingGame;
}

bool OotmmGameState::GetBoolSetting(const std::string& name, bool fallback) const {
    const auto it = mBoolSettings.find(name);
    return it == mBoolSettings.end() ? fallback : it->second;
}

std::string OotmmGameState::GetStringSetting(const std::string& name, std::string fallback) const {
    const auto it = mStringSettings.find(name);
    return it == mStringSettings.end() ? std::move(fallback) : it->second;
}

int OotmmGameState::GetIntSetting(const std::string& name, int fallback) const {
    const auto it = mNumberSettings.find(name);
    return it == mNumberSettings.end() ? fallback : static_cast<int>(it->second);
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

const std::vector<OotmmEntranceMapping>& OotmmGameState::GetEntrances() const {
    return mEntrances;
}

const OotmmEntranceMapping* OotmmGameState::FindEntrance(OotmmGame game, uint32_t nativeId) const {
    const auto mapping = std::find_if(mEntrances.begin(), mEntrances.end(), [game, nativeId](const auto& entrance) {
        return entrance.FromGame == game && entrance.FromNativeId == nativeId;
    });
    return mapping == mEntrances.end() ? nullptr : &*mapping;
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

    uint32_t playerId = 1;
    uint32_t playerCount = 1;
    uint32_t sessionEpoch = 1;
    if (const auto it = root.find("playerId"); it != root.end() && it->is_number_unsigned()) {
        playerId = it->get<uint32_t>();
    }
    if (const auto it = root.find("playerCount"); it != root.end() && it->is_number_unsigned()) {
        playerCount = it->get<uint32_t>();
    }
    if (const auto it = root.find("sessionEpoch"); it != root.end() && it->is_number_unsigned()) {
        sessionEpoch = it->get<uint32_t>();
    }
    if (playerCount > 1) {
        mNativeSaveTag += "_p" + std::to_string(playerId);
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

    const auto preCompleted = root.find("preCompletedDungeons");
    if (preCompleted != root.end() && preCompleted->is_array()) {
        for (const auto& dungeon : *preCompleted) {
            if (dungeon.is_string()) {
                mPreCompletedDungeons.emplace(dungeon.get<std::string>());
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
