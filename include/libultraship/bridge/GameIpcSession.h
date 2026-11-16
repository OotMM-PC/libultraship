#pragma once

#include "libultraship/bridge/IpcClient.h"
#include "libultraship/bridge/OotmmInventory.h"
#include "libultraship/bridge/OotmmPresence.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace Ship {

namespace GameIpcMessageType {
inline constexpr const char* Hello = "hello";
inline constexpr const char* Welcome = "welcome";
inline constexpr const char* Heartbeat = "heartbeat";
inline constexpr const char* ProtocolError = "protocolError";
inline constexpr const char* SettingsSnapshot = "settingsSnapshot";
inline constexpr const char* SettingsDelta = "settingsDelta";
inline constexpr const char* SettingsApplied = "settingsApplied";
inline constexpr const char* CheckCollected = "checkCollected";
inline constexpr const char* CheckAccepted = "checkAccepted";
inline constexpr const char* CompletedChecks = "completedChecks";
inline constexpr const char* DeliverItem = "deliverItem";
inline constexpr const char* ItemApplied = "itemApplied";
inline constexpr const char* InventorySnapshot = "inventorySnapshot";
inline constexpr const char* InventoryDelta = "inventoryDelta";
inline constexpr const char* InventoryDebugSet = "inventoryDebugSet";
inline constexpr const char* DebugPresentItem = "debugPresentItem";
inline constexpr const char* PresentItem = "presentItem";
inline constexpr const char* LocalPlayerPose = "localPlayerPose";
inline constexpr const char* RemotePlayerPoses = "remotePlayerPoses";
inline constexpr const char* PvpHit = "pvpHit";
inline constexpr const char* ModelReady = "modelReady";
inline constexpr const char* ModelMounted = "modelMounted";
inline constexpr const char* CrossGameTransition = "crossGameTransition";
inline constexpr const char* TransitionAccepted = "transitionAccepted";
inline constexpr const char* SaveRequest = "saveRequest";
inline constexpr const char* SaveCompleted = "saveCompleted";
} // namespace GameIpcMessageType

struct GameIpcKnockbackChance {
    uint32_t Percent = 0;
    float MinDistance = 0.0f;
    float MaxDistance = 0.0f;
};

struct GameIpcTrapTuning {
    int IceBreakoutPercent = 100;
    int IceDamagePercent = 100;
    int FireDamagePercent = 100;
    int FireDuration = 200;
    int ShockDamagePercent = 100;
    int ShockDuration = 20;
    std::vector<GameIpcKnockbackChance> Knockback;

    const GameIpcKnockbackChance* PickKnockback(float roll) const {
        float cumulative = 0.0f;
        for (const auto& chance : Knockback) {
            cumulative += static_cast<float>(chance.Percent);
            if (roll < cumulative) {
                return &chance;
            }
        }
        return nullptr;
    }
};

struct GameIpcSettings {
    uint64_t Revision = 0;
    bool ShowNames = true;
    bool Pvp = false;
    bool DeathLink = false;
    int SpeedPercent = 100;
    bool SpeedSmooth = false;
    std::string TransportState = "healthy";
    uint32_t PendingChecks = 0;
    int ZTargetHold = -1;
    bool FairySpotPads = false;
    bool PotRolling = true;
    GameIpcTrapTuning Traps;
};

struct GameIpcItemPresentation {
    std::string ItemId;
    std::string ItemName;
    std::string ItemGame;
    std::string DestinationGame;
    uint32_t SourcePlayer = 0;
    uint32_t RecipientPlayer = 0;
    uint32_t ViewerPlayer = 0;
    bool Debug = false;
    std::string CheckKey;
    bool Background = false;
    bool Quiet = false;
};

class GameIpcSession {
  public:
    bool Start(std::string game, std::string build);
    void Stop();
    bool Pump(GameIpcSettings& settings, OotmmInventory* inventory = nullptr);
    bool AcknowledgeSettings(uint64_t revision);
    bool SetDebugInventoryValue(const std::string& id, uint32_t value);
    bool RequestDebugItemPresentation(const std::string& id, uint32_t recipientPlayer);
    bool TryPopItemPresentation(GameIpcItemPresentation& presentation);
    bool RequestCrossGameTransition(std::string fromGame, std::string toGame, std::string toEntrance,
                                    std::optional<uint32_t> toNativeId, std::optional<uint32_t> ootAge);
    bool ConsumeTransitionAccepted();
    std::optional<uint64_t> ConsumeSaveRequest();
    bool SendSaveCompleted(uint64_t requestId, bool saved);

    bool SendCheckCollected(const std::string& game, const std::string& checkId, bool present = false);
    bool IsCheckPresentationPending(const std::string& key) const;
    void CompleteCheckPresentation(const std::string& key);
    bool IsCheckCompleted(const std::string& game, const std::string& checkId) const;
    uint64_t CompletedChecksRevision() const;

    bool PresenceActive() const;
    bool SendPlayerPose(const OotmmPlayerPose& pose);
    bool SendPvpHit(const OotmmPvpHit& hit);
    bool TakeRemotePresence(std::vector<OotmmPlayerPose>& poses, std::vector<OotmmPvpHit>& hits);

    bool Send(std::string message);
    bool TryReceive(std::string& message);
    bool IsConnected() const;
    std::string GetLastError() const;

  private:
    void SendHello();
    bool SendCheckReport(const std::string& key);
    void HandleMessage(const std::string& message, GameIpcSettings& settings, OotmmInventory* inventory,
                       bool& settingsChanged);

    IpcClient mClient;
    std::string mToken;
    std::string mGame;
    std::string mBuild;
    std::string mProtocolError;
    std::deque<std::string> mUnhandled;
    std::deque<GameIpcItemPresentation> mPresentations;
    std::unordered_set<std::string> mPendingCheckPresentations;
    std::unordered_set<std::string> mReceivedCheckPresentations;
    bool mWasConnected = false;
    bool mWelcomed = false;
    bool mCheckRewardSupported = false;
    bool mTransitionAccepted = false;
    std::optional<uint64_t> mSaveRequest;

    std::string mRemoteRoster;
    bool mRemoteRosterFresh = false;
    std::deque<OotmmPvpHit> mRemoteHits;
    std::chrono::steady_clock::time_point mPresenceUntil{};
    std::string mLastPoseSent;
    std::unordered_set<std::string> mCompletedChecks;
    std::vector<std::string> mPendingCheckReports;
    uint64_t mCompletedRevision = 0;
};

} // namespace Ship
