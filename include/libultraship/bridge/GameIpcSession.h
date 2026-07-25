#pragma once

#include "libultraship/bridge/IpcClient.h"
#include "libultraship/bridge/OotmmInventory.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

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
inline constexpr const char* DeliverItem = "deliverItem";
inline constexpr const char* ItemApplied = "itemApplied";
inline constexpr const char* InventorySnapshot = "inventorySnapshot";
inline constexpr const char* InventoryDelta = "inventoryDelta";
inline constexpr const char* InventoryDebugSet = "inventoryDebugSet";
inline constexpr const char* DebugPresentItem = "debugPresentItem";
inline constexpr const char* PresentItem = "presentItem";
inline constexpr const char* LocalPlayerPose = "localPlayerPose";
inline constexpr const char* RemotePlayerPoses = "remotePlayerPoses";
inline constexpr const char* ModelReady = "modelReady";
inline constexpr const char* ModelMounted = "modelMounted";
inline constexpr const char* CrossGameTransition = "crossGameTransition";
inline constexpr const char* TransitionAccepted = "transitionAccepted";
} // namespace GameIpcMessageType

struct GameIpcSettings {
    uint64_t Revision = 0;
    bool ShowNames = true;
    bool Pvp = false;
    bool DeathLink = false;
    int SpeedPercent = 100;
    bool SpeedSmooth = false;
    std::string TransportState = "healthy";
    uint32_t PendingChecks = 0;
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
    bool Send(std::string message);
    bool TryReceive(std::string& message);
    bool IsConnected() const;
    std::string GetLastError() const;

  private:
    void SendHello();
    void HandleMessage(const std::string& message, GameIpcSettings& settings, OotmmInventory* inventory,
                       bool& settingsChanged);

    IpcClient mClient;
    std::string mToken;
    std::string mGame;
    std::string mBuild;
    std::string mProtocolError;
    std::deque<std::string> mUnhandled;
    std::deque<GameIpcItemPresentation> mPresentations;
    bool mWasConnected = false;
    bool mWelcomed = false;
    bool mTransitionAccepted = false;
};

} // namespace Ship
