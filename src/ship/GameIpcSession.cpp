#include "libultraship/bridge/GameIpcSession.h"

#include <algorithm>
#include <cstdlib>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace Ship {

namespace {

constexpr int ProtocolMajor = 1;
constexpr int ProtocolMinor = 0;
constexpr size_t MaxUnhandledMessages = 1024;

uint32_t ProcessId() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<uint32_t>(getpid());
#endif
}

} // namespace

bool GameIpcSession::Start(std::string game, std::string build) {
    Stop();
    const char* endpoint = std::getenv("OOTMM_IPC_ENDPOINT");
    const char* token = std::getenv("OOTMM_IPC_TOKEN");
    if (endpoint == nullptr || endpoint[0] == '\0' || token == nullptr || token[0] == '\0') {
        return false;
    }

    mToken = token;
    mGame = std::move(game);
    mBuild = std::move(build);
    return mClient.Start(endpoint);
}

void GameIpcSession::Stop() {
    mClient.Stop();
    mToken.clear();
    mGame.clear();
    mBuild.clear();
    mProtocolError.clear();
    mUnhandled.clear();
    mPresentations.clear();
    mWasConnected = false;
    mWelcomed = false;
    mTransitionAccepted = false;
}

bool GameIpcSession::Pump(GameIpcSettings& settings, OotmmInventory* inventory) {
    const bool connected = mClient.IsConnected();
    if (connected && !mWasConnected) {
        mWelcomed = false;
        SendHello();
    } else if (!connected) {
        mWelcomed = false;
    }
    mWasConnected = connected;

    bool settingsChanged = false;
    std::string message;
    while (mClient.TryReceive(message)) {
        HandleMessage(message, settings, inventory, settingsChanged);
    }
    return settingsChanged;
}

bool GameIpcSession::SetDebugInventoryValue(const std::string& id, uint32_t value) {
    nlohmann::json update{
        { "type", GameIpcMessageType::InventoryDebugSet },
        { "itemId", id },
        { "value", value },
    };
    return Send(update.dump());
}

bool GameIpcSession::RequestDebugItemPresentation(const std::string& id, uint32_t recipientPlayer) {
    nlohmann::json request{
        { "type", GameIpcMessageType::DebugPresentItem },
        { "itemId", id },
        { "recipientPlayer", recipientPlayer },
    };
    return Send(request.dump());
}

bool GameIpcSession::TryPopItemPresentation(GameIpcItemPresentation& presentation) {
    if (mPresentations.empty()) {
        return false;
    }
    presentation = std::move(mPresentations.front());
    mPresentations.pop_front();
    return true;
}

bool GameIpcSession::AcknowledgeSettings(uint64_t revision) {
    nlohmann::json applied{
        { "type", GameIpcMessageType::SettingsApplied },
        { "revision", revision },
    };
    return Send(applied.dump());
}

bool GameIpcSession::RequestCrossGameTransition(std::string fromGame, std::string toGame,
                                                std::string toEntrance, std::optional<uint32_t> toNativeId,
                                                std::optional<uint32_t> ootAge) {
    nlohmann::json transition{
        { "type", GameIpcMessageType::CrossGameTransition },
        { "fromGame", std::move(fromGame) },
        { "toGame", std::move(toGame) },
        { "toEntrance", std::move(toEntrance) },
    };
    if (toNativeId.has_value()) {
        transition["toNativeId"] = *toNativeId;
    }
    if (ootAge.has_value()) {
        transition["ootAge"] = *ootAge;
    }
    mTransitionAccepted = false;
    return Send(transition.dump());
}

bool GameIpcSession::ConsumeTransitionAccepted() {
    const bool accepted = mTransitionAccepted;
    mTransitionAccepted = false;
    return accepted;
}

bool GameIpcSession::Send(std::string message) {
    return mWelcomed && mClient.Send(std::move(message));
}

bool GameIpcSession::TryReceive(std::string& message) {
    if (mUnhandled.empty()) {
        return false;
    }

    message = std::move(mUnhandled.front());
    mUnhandled.pop_front();
    return true;
}

bool GameIpcSession::IsConnected() const {
    return mWelcomed && mClient.IsConnected();
}

std::string GameIpcSession::GetLastError() const {
    return mProtocolError.empty() ? mClient.GetLastError() : mProtocolError;
}

void GameIpcSession::SendHello() {
    nlohmann::json hello{
        { "type", GameIpcMessageType::Hello },
        { "protocolMajor", ProtocolMajor },
        { "protocolMinor", ProtocolMinor },
        { "token", mToken },
        { "game", mGame },
        { "pid", ProcessId() },
        { "build", mBuild },
    };
    mClient.Send(hello.dump());
}

void GameIpcSession::HandleMessage(const std::string& message, GameIpcSettings& settings, OotmmInventory* inventory,
                                   bool& settingsChanged) try {
    const auto root = nlohmann::json::parse(message, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        mProtocolError = "Launcher sent invalid IPC JSON";
        return;
    }

    const auto type = root.value("type", "");
    if (type == GameIpcMessageType::Welcome) {
        if (root.value("protocolMajor", 0) != ProtocolMajor) {
            mProtocolError = "Launcher IPC protocol is incompatible";
            return;
        }
        mProtocolError.clear();
        mWelcomed = true;
        return;
    }

    if (type == GameIpcMessageType::ProtocolError) {
        mProtocolError = root.value("message", "Launcher rejected the IPC connection");
        return;
    }

    if (type == GameIpcMessageType::Heartbeat) {
        if (mWelcomed) {
            mClient.Send(message);
        }
        return;
    }

    if (type == GameIpcMessageType::TransitionAccepted) {
        mTransitionAccepted = mWelcomed;
        return;
    }

    if (type == GameIpcMessageType::SettingsSnapshot || type == GameIpcMessageType::SettingsDelta) {
        if (!mWelcomed || !root.contains("settings") || !root["settings"].is_object()) {
            return;
        }

        const uint64_t revision = root.value("revision", uint64_t{ 0 });
        if (revision <= settings.Revision) {
            return;
        }

        const auto& values = root["settings"];
        settings.ShowNames = values.value("showNames", settings.ShowNames);
        settings.Pvp = values.value("pvp", settings.Pvp);
        settings.DeathLink = values.value("deathLink", settings.DeathLink);
        settings.SpeedPercent = std::clamp(values.value("speedPercent", settings.SpeedPercent), 0, 1000);
        settings.SpeedSmooth = values.value("speedSmooth", settings.SpeedSmooth);
        settings.TransportState = values.value("transportState", settings.TransportState);
        settings.PendingChecks = values.value("pendingChecks", settings.PendingChecks);
        settings.Revision = revision;
        settingsChanged = true;
        return;
    }

    if ((type == GameIpcMessageType::InventorySnapshot || type == GameIpcMessageType::InventoryDelta) &&
        mWelcomed && inventory != nullptr && root.contains("items") && root["items"].is_object()) {
        std::unordered_map<std::string, uint32_t> values;
        for (auto item = root["items"].begin(); item != root["items"].end(); ++item) {
            if (item.value().is_number_unsigned()) {
                values.emplace(item.key(), item.value().get<uint32_t>());
            }
        }
        const uint64_t revision = root.value("revision", uint64_t{ 0 });
        if (type == GameIpcMessageType::InventorySnapshot) {
            inventory->ApplySnapshot(revision, std::move(values));
        } else {
            inventory->ApplyDelta(revision, values);
        }
        return;
    }

    if (type == GameIpcMessageType::PresentItem && mWelcomed) {
        const auto itemId = root.value("itemId", "");
        const auto itemName = root.value("itemName", "");
        const auto itemGame = root.value("itemGame", "");
        const auto destinationGame = root.value("destinationGame", "");
        if (itemId.empty() || itemName.empty() ||
            (itemGame != "oot" && itemGame != "mm" && itemGame != "shared") ||
            (destinationGame != "oot" && destinationGame != "mm")) {
            mProtocolError = "Launcher sent an invalid item presentation";
            return;
        }
        mPresentations.push_back({
            .ItemId = itemId,
            .ItemName = itemName,
            .ItemGame = itemGame,
            .DestinationGame = destinationGame,
            .SourcePlayer = root.value("sourcePlayer", uint32_t{ 0 }),
            .RecipientPlayer = root.value("recipientPlayer", uint32_t{ 0 }),
            .ViewerPlayer = root.value("viewerPlayer", uint32_t{ 0 }),
            .Debug = root.value("debug", false),
        });
        return;
    }

    if (mUnhandled.size() < MaxUnhandledMessages) {
        mUnhandled.push_back(message);
    }
} catch (const nlohmann::json::exception&) {
    mProtocolError = "Launcher sent an invalid IPC message";
}

} // namespace Ship
