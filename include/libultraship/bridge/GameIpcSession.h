#pragma once

#include "libultraship/bridge/IpcClient.h"

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

class GameIpcSession {
  public:
    bool Start(std::string game, std::string build);
    void Stop();
    bool Pump(GameIpcSettings& settings);
    bool AcknowledgeSettings(uint64_t revision);
    bool RequestCrossGameTransition(std::string fromGame, std::string toGame, std::string toEntrance,
                                    std::optional<uint32_t> toNativeId, std::optional<uint32_t> ootAge);
    bool ConsumeTransitionAccepted();
    bool Send(std::string message);
    bool TryReceive(std::string& message);
    bool IsConnected() const;
    std::string GetLastError() const;

  private:
    void SendHello();
    void HandleMessage(const std::string& message, GameIpcSettings& settings, bool& settingsChanged);

    IpcClient mClient;
    std::string mToken;
    std::string mGame;
    std::string mBuild;
    std::string mProtocolError;
    std::deque<std::string> mUnhandled;
    bool mWasConnected = false;
    bool mWelcomed = false;
    bool mTransitionAccepted = false;
};

} // namespace Ship
