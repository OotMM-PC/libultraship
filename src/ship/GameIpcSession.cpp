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
constexpr size_t MaxQueuedHits = 64;
constexpr char kRosterPrefix[] = "{\"type\":\"remotePlayerPoses\"";

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
    mPendingCheckPresentations.clear();
    mReceivedCheckPresentations.clear();
    mWasConnected = false;
    mWelcomed = false;
    mCheckRewardSupported = false;
    mTransitionAccepted = false;
    mRemoteRoster.clear();
    mRemoteHits.clear();
    mRemoteRosterFresh = false;
    mPresenceUntil = {};
    mLastPoseSent.clear();
    mCompletedChecks.clear();
    mPendingCheckReports.clear();
    mCompletedRevision = 0;
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

bool GameIpcSession::PresenceActive() const {
    return std::chrono::steady_clock::now() < mPresenceUntil;
}

bool GameIpcSession::SendPlayerPose(const OotmmPlayerPose& pose) {
    std::string body = OotmmPresenceIo::SerializePose(pose);
    if (body == mLastPoseSent) {
        return true;
    }

    std::string envelope;
    envelope.reserve(body.size() + 32);
    envelope.append("{\"type\":\"").append(GameIpcMessageType::LocalPlayerPose).append("\",\"pose\":");
    envelope.append(body).append("}");
    if (!Send(std::move(envelope))) {
        return false;
    }

    mLastPoseSent = std::move(body);
    return true;
}

bool GameIpcSession::SendPvpHit(const OotmmPvpHit& hit) {
    const std::string body = OotmmPresenceIo::SerializePvpHit(hit);
    std::string envelope;
    envelope.reserve(body.size() + 32);
    envelope.append("{\"type\":\"").append(GameIpcMessageType::PvpHit).append("\",\"hit\":");
    envelope.append(body).append("}");
    return Send(std::move(envelope));
}

bool GameIpcSession::SendCheckCollected(const std::string& game, const std::string& checkId, bool present) {
    const std::string key = game + ":" + checkId;
    if (present && mReceivedCheckPresentations.count(key) == 0) {
        mPendingCheckPresentations.emplace(key);
    }
    if (mCompletedChecks.count(key) != 0 && mPendingCheckPresentations.count(key) == 0) {
        return true;
    }
    if (std::find(mPendingCheckReports.begin(), mPendingCheckReports.end(), key)
        == mPendingCheckReports.end()) {
        mPendingCheckReports.push_back(key);
    }
    return SendCheckReport(key);
}

bool GameIpcSession::IsCheckPresentationPending(const std::string& key) const {
    return mPendingCheckPresentations.count(key) != 0;
}

void GameIpcSession::CompleteCheckPresentation(const std::string& key) {
    mPendingCheckPresentations.erase(key);
}

bool GameIpcSession::IsCheckCompleted(const std::string& game, const std::string& checkId) const {
    return mCompletedChecks.count(game + ":" + checkId) != 0;
}

uint64_t GameIpcSession::CompletedChecksRevision() const {
    return mCompletedRevision;
}

bool GameIpcSession::SendCheckReport(const std::string& key) {
    if (!mWelcomed) {
        return false;
    }
    nlohmann::json report{ { "type", GameIpcMessageType::CheckCollected }, { "check", key } };
    if (mPendingCheckPresentations.count(key) != 0) {
        if (!mCheckRewardSupported) {
            mProtocolError = "Launcher does not support correlated check rewards; update the launcher";
            return false;
        }
        report["present"] = true;
    }
    return Send(report.dump());
}

bool GameIpcSession::TakeRemotePresence(std::vector<OotmmPlayerPose>& poses, std::vector<OotmmPvpHit>& hits) {
    hits.assign(mRemoteHits.begin(), mRemoteHits.end());
    mRemoteHits.clear();
    if (!mRemoteRosterFresh) {
        return !hits.empty();
    }

    mRemoteRosterFresh = false;
    OotmmPresenceIo::ParseRoster(mRemoteRoster, poses);
    return true;
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
    if (message.compare(0, sizeof(kRosterPrefix) - 1, kRosterPrefix) == 0) {
        mPresenceUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        mRemoteRoster = message;
        mRemoteRosterFresh = true;
        return;
    }

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
        mCheckRewardSupported = root.value("checkRewardProtocol", 0) == 1;
        mWelcomed = true;
        for (const auto& key : mPendingCheckReports) {
            SendCheckReport(key);
        }
        for (const auto& key : mPendingCheckPresentations) {
            if (mReceivedCheckPresentations.count(key) == 0 &&
                std::find(mPendingCheckReports.begin(), mPendingCheckReports.end(), key) == mPendingCheckReports.end()) {
                SendCheckReport(key);
            }
        }
        return;
    }

    if (type == GameIpcMessageType::CheckAccepted) {
        if (!mWelcomed) {
            return;
        }
        const std::string key = root.value("check", "");
        if (!key.empty() && mCompletedChecks.emplace(key).second) {
            mCompletedRevision++;
        }
        std::erase(mPendingCheckReports, key);
        return;
    }

    if (type == GameIpcMessageType::CompletedChecks) {
        if (!mWelcomed) {
            return;
        }
        const auto checks = root.find("checks");
        if (checks == root.end() || !checks->is_array()) {
            return;
        }
        for (const auto& value : *checks) {
            if (value.is_string() && mCompletedChecks.emplace(value.get<std::string>()).second) {
                mCompletedRevision++;
            }
        }
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
        if (values.contains("zTargetHold") && values["zTargetHold"].is_boolean()) {
            settings.ZTargetHold = values["zTargetHold"].get<bool>() ? 1 : 0;
        }
        if (values.contains("traps") && values["traps"].is_object()) {
            const auto& traps = values["traps"];
            GameIpcTrapTuning tuning;
            tuning.IceBreakoutPercent = std::clamp(traps.value("iceBreakoutPercent", tuning.IceBreakoutPercent), 25, 400);
            tuning.IceDamage = std::clamp(traps.value("iceDamage", tuning.IceDamage), 0, 320);
            tuning.FireDamage = std::clamp(traps.value("fireDamage", tuning.FireDamage), 0, 320);
            tuning.FireDuration = std::clamp(traps.value("fireDuration", tuning.FireDuration), 0, 600);
            tuning.ShockDamage = std::clamp(traps.value("shockDamage", tuning.ShockDamage), 0, 320);
            tuning.ShockDuration = std::clamp(traps.value("shockDuration", tuning.ShockDuration), 1, 200);
            uint32_t total = 0;
            if (traps.contains("knockback") && traps["knockback"].is_array()) {
                for (const auto& row : traps["knockback"]) {
                    if (!row.is_object() || !row.contains("percent") || !row["percent"].is_number_unsigned() ||
                        !row.contains("distance") || !row["distance"].is_number()) {
                        continue;
                    }
                    const uint32_t percent = row["percent"].get<uint32_t>();
                    const float distance = row["distance"].get<float>();
                    if (percent == 0 || total + percent > 100 || distance < 0.0f || distance > 2000.0f) {
                        continue;
                    }
                    total += percent;
                    tuning.Knockback.push_back({ percent, distance });
                }
            }
            settings.Traps = std::move(tuning);
        }
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
        const auto check = root.find("check");
        if (check != root.end() && !check->is_string() && !check->is_null()) {
            mProtocolError = "Launcher sent an invalid check presentation identity";
            return;
        }
        const std::string checkKey = check != root.end() && check->is_string()
            ? check->get<std::string>() : "";
        if (!checkKey.empty()) {
            if (mPendingCheckPresentations.count(checkKey) == 0 ||
                mReceivedCheckPresentations.count(checkKey) != 0) {
                return;
            }
        }
        GameIpcItemPresentation presentation{
            .ItemId = itemId,
            .ItemName = itemName,
            .ItemGame = itemGame,
            .DestinationGame = destinationGame,
            .SourcePlayer = root.value("sourcePlayer", uint32_t{ 0 }),
            .RecipientPlayer = root.value("recipientPlayer", uint32_t{ 0 }),
            .ViewerPlayer = root.value("viewerPlayer", uint32_t{ 0 }),
            .Debug = root.value("debug", false),
            .CheckKey = checkKey,
            .Background = root.value("background", false),
            .Quiet = root.value("quiet", false),
        };
        mPresentations.push_back(std::move(presentation));
        if (!checkKey.empty()) {
            mReceivedCheckPresentations.emplace(checkKey);
        }
        return;
    }

    if (type == GameIpcMessageType::PvpHit) {
        if (const auto body = root.find("hit"); body != root.end() && body->is_object()) {
            if (auto hit = OotmmPresenceIo::ParsePvpHit(body->dump());
                hit.has_value() && mRemoteHits.size() < MaxQueuedHits) {
                mRemoteHits.push_back(*hit);
            }
        }
        return;
    }

    if (mUnhandled.size() < MaxUnhandledMessages) {
        mUnhandled.push_back(message);
    }
} catch (const nlohmann::json::exception&) {
    mProtocolError = "Launcher sent an invalid IPC message";
}

} // namespace Ship
