#include "libultraship/bridge/GameIpcSession.h"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

using json = nlohmann::json;
using namespace std::chrono_literals;

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class Harness {
  public:
    Harness(const std::string& game = "mm") {
        Endpoint = "\\\\.\\pipe\\ootmm-ipc-regression-" + std::to_string(GetCurrentProcessId()) + "-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        Open();
        _putenv_s("OOTMM_IPC_ENDPOINT", Endpoint.c_str());
        _putenv_s("OOTMM_IPC_TOKEN", "local-regression");
        Require(Session.Start(game, "regression"), "Client did not start");
        Receive("hello");
    }

    ~Harness() {
        Session.Stop();
        Close();
        _putenv_s("OOTMM_IPC_ENDPOINT", "");
        _putenv_s("OOTMM_IPC_TOKEN", "");
    }

    void Open() {
        Pipe = CreateNamedPipeA(Endpoint.c_str(), PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT, 1, 65536, 65536, 0, nullptr);
        Require(Pipe != INVALID_HANDLE_VALUE, "Could not create isolated test pipe");
        ConnectNamedPipe(Pipe, nullptr);
    }

    void Close() {
        if (Pipe != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(Pipe);
            CloseHandle(Pipe);
            Pipe = INVALID_HANDLE_VALUE;
        }
        Incoming.clear();
        Messages.clear();
    }

    void Send(const json& message) {
        const auto body = message.dump();
        const auto size = static_cast<uint32_t>(body.size());
        std::string frame(4, '\0');
        for (int i = 0; i < 4; ++i) frame[i] = static_cast<char>(size >> (8 * i));
        frame += body;
        DWORD written = 0;
        Require(WriteFile(Pipe, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr)
            && written == frame.size(), "Could not send test frame");
        Trace.push_back({{"direction", "launcher"}, {"message", message}});
    }

    template <class Predicate> void Until(Predicate predicate) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        do {
            Session.Pump(Settings);
            Poll();
            if (predicate()) return;
            std::this_thread::sleep_for(2ms);
        } while (std::chrono::steady_clock::now() < deadline);
        throw std::runtime_error("IPC condition timed out: " + Session.GetLastError());
    }

    json Receive(const std::string& type) {
        Until([&] { return std::any_of(Messages.begin(), Messages.end(), [&](const json& message) {
            return message.value("type", "") == type;
        }); });
        const auto found = std::find_if(Messages.begin(), Messages.end(), [&](const json& message) {
            return message.value("type", "") == type;
        });
        auto result = *found;
        Messages.erase(found);
        return result;
    }

    void Welcome(int rewardProtocol = 1) {
        Send({{"type", "welcome"}, {"protocolMajor", 1}, {"checkRewardProtocol", rewardProtocol}});
        Until([&] { return Session.IsConnected(); });
    }

    void Barrier() {
        Send({{"type", "heartbeat"}, {"probe", true}});
        Receive("heartbeat");
    }

    size_t Reports() const {
        return std::count_if(Messages.begin(), Messages.end(), [](const json& message) {
            return message.value("type", "") == "checkCollected";
        });
    }

    void Reconnect() {
        Close();
        Until([&] { return !Session.IsConnected(); });
        Open();
        Receive("hello");
        Welcome();
    }

    void Evidence(const std::filesystem::path& path) const {
        std::ofstream(path) << json(Trace).dump(2);
    }

    Ship::GameIpcSession Session;

  private:
    void Poll() {
        if (Pipe == INVALID_HANDLE_VALUE) return;
        char buffer[4096];
        DWORD read = 0;
        while (ReadFile(Pipe, buffer, sizeof(buffer), &read, nullptr) && read != 0) {
            Incoming.append(buffer, read);
        }
        while (Incoming.size() >= 4) {
            uint32_t size = 0;
            for (int i = 0; i < 4; ++i) size |= uint32_t(static_cast<uint8_t>(Incoming[i])) << (8 * i);
            Require(size < 65536, "Unexpected frame size");
            if (Incoming.size() < size + 4) break;
            auto message = json::parse(Incoming.substr(4, size));
            Trace.push_back({{"direction", "game"}, {"message", message}});
            Messages.push_back(std::move(message));
            Incoming.erase(0, size + 4);
        }
    }

    std::string Endpoint;
    HANDLE Pipe = INVALID_HANDLE_VALUE;
    Ship::GameIpcSettings Settings;
    std::string Incoming;
    std::vector<json> Messages;
    std::vector<json> Trace;
};

static json Presentation(const std::string& key) {
    return {{"type", "presentItem"}, {"itemId", "MM_DEED_LAND"}, {"itemName", "Land Title Deed"},
        {"itemGame", "mm"}, {"destinationGame", "mm"}, {"sourcePlayer", 1}, {"recipientPlayer", 1},
        {"viewerPlayer", 1}, {"debug", false}, {"check", key}};
}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "Expected evidence directory");
        const auto evidence = std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(evidence);
        {
            Harness h;
            Require(!h.Session.SendCheckCollected("mm", "A", true), "Unwelcomed report was sent");
            h.Welcome();
            auto report = h.Receive("checkCollected");
            Require(report["check"] == "mm:A" && report["present"] == true, "Pending reward was not requested");
            h.Barrier();
            Require(h.Reports() == 0, "Welcome sent the same reward report twice");
            h.Send({{"type", "checkAccepted"}, {"check", "mm:A"}});
            h.Send({{"type", "checkAccepted"}, {"check", "mm:A"}});
            h.Send(Presentation("mm:UNSOLICITED"));
            h.Send(Presentation("mm:A"));
            h.Send(Presentation("mm:A"));
            h.Barrier();
            Require(h.Session.IsCheckCompleted("mm", "A") && h.Session.CompletedChecksRevision() == 1,
                "Acknowledgments changed durable check identity more than once");
            Ship::GameIpcItemPresentation item;
            Require(h.Session.TryPopItemPresentation(item) && item.CheckKey == "mm:A", "Reward did not arrive");
            Require(!h.Session.TryPopItemPresentation(item), "Duplicate or unsolicited reward was queued");
            Require(h.Session.IsCheckPresentationPending("mm:A"), "Ack released actor before its presentation ended");
            h.Session.CompleteCheckPresentation("mm:A");
            Require(!h.Session.IsCheckPresentationPending("mm:A"), "Completed presentation remained pending");
            h.Session.SendCheckCollected("mm", "A", true);
            h.Barrier();
            Require(h.Reports() == 0, "Completed reward was requested again");
            h.Evidence(evidence / "ack-duplicates.json");
        }
        {
            Harness h;
            h.Welcome();
            h.Session.SendCheckCollected("mm", "A", true);
            h.Receive("checkCollected");
            h.Reconnect();
            auto replay = h.Receive("checkCollected");
            Require(replay["check"] == "mm:A" && replay["present"] == true,
                "Unacknowledged check was not re-requested on reconnect");
            h.Barrier();
            Require(h.Reports() == 0, "Reconnect sent duplicate pending reports");
            h.Send(Presentation("mm:A"));
            h.Barrier();
            Ship::GameIpcItemPresentation item;
            Require(h.Session.TryPopItemPresentation(item), "Presentation before acknowledgment was lost");
            h.Session.CompleteCheckPresentation("mm:A");
            Require(!h.Session.IsCheckCompleted("mm", "A"), "Presentation completion accepted a check");
            h.Send({{"type", "checkAccepted"}, {"check", "mm:A"}});
            h.Barrier();
            Require(h.Session.IsCheckCompleted("mm", "A") && h.Session.CompletedChecksRevision() == 1,
                "Late acknowledgment failed after presentation completion");
            h.Evidence(evidence / "unacknowledged-reconnect.json");
        }
        {
            Harness h;
            h.Welcome();
            h.Session.SendCheckCollected("mm", "A", true);
            h.Receive("checkCollected");
            h.Send({{"type", "checkAccepted"}, {"check", "mm:A"}});
            h.Barrier();
            h.Reconnect();
            auto replay = h.Receive("checkCollected");
            Require(replay["check"] == "mm:A" && replay["present"] == true,
                "Acknowledged check lost its pending presentation on reconnect");
            h.Send(Presentation("mm:A"));
            h.Barrier();
            h.Reconnect();
            h.Barrier();
            Require(h.Reports() == 0, "Queued presentation was re-requested on reconnect");
            h.Send(Presentation("mm:A"));
            h.Barrier();
            Ship::GameIpcItemPresentation item;
            Require(h.Session.TryPopItemPresentation(item) && !h.Session.TryPopItemPresentation(item),
                "Reconnect dropped or duplicated queued presentation");
            h.Session.CompleteCheckPresentation("mm:A");
            h.Evidence(evidence / "reconnect.json");
        }
        {
            Harness h;
            h.Welcome();
            h.Session.SendCheckCollected("mm", "A", true);
            h.Receive("checkCollected");
            h.Session.SendCheckCollected("mm", "B", true);
            h.Receive("checkCollected");
            auto malformed = Presentation("mm:A");
            malformed["recipientPlayer"] = "invalid";
            h.Send(malformed);
            h.Send(Presentation("mm:B"));
            h.Send(Presentation("mm:A"));
            h.Barrier();
            Ship::GameIpcItemPresentation item;
            Require(h.Session.TryPopItemPresentation(item) && item.CheckKey == "mm:B", "B reward missing");
            h.Session.CompleteCheckPresentation("mm:B");
            Require(h.Session.IsCheckPresentationPending("mm:A") && !h.Session.IsCheckPresentationPending("mm:B"),
                "One actor completion released a different reward");
            Require(h.Session.TryPopItemPresentation(item) && item.CheckKey == "mm:A",
                "Malformed metadata consumed valid reward correlation");
            h.Session.CompleteCheckPresentation("mm:A");
            auto invalidCheck = Presentation("mm:A");
            invalidCheck["check"] = 7;
            h.Send(invalidCheck);
            h.Barrier();
            Require(!h.Session.TryPopItemPresentation(item), "Invalid check correlation became an uncorrelated preview");
            auto debug = Presentation("");
            debug["check"] = nullptr;
            debug["debug"] = true;
            h.Send(debug);
            h.Barrier();
            Require(h.Session.TryPopItemPresentation(item) && item.Debug && item.CheckKey.empty(),
                "Existing debug preview with null check was rejected");
            h.Session.Stop();
            Require(!h.Session.IsCheckPresentationPending("mm:A") && !h.Session.TryPopItemPresentation(item),
                "Stopping a session retained pending reward state");
            h.Evidence(evidence / "reorder-malformed.json");
        }
        for (const auto& game : {std::string("oot"), std::string("mm")}) {
            Harness h(game);
            h.Welcome();
            for (int i = 0; i < 40; ++i) {
                const auto id = "BURST_" + std::to_string(i);
                Require(h.Session.SendCheckCollected(game, id, true), "Burst report was not sent");
                auto report = h.Receive("checkCollected");
                Require(report["check"] == game + ":" + id, "Burst report had wrong identity");
            }
            for (int i = 0; i < 40; ++i) {
                const auto key = game + ":BURST_" + std::to_string(i);
                h.Send({{"type", "checkAccepted"}, {"check", key}});
                h.Send(Presentation(key));
                h.Send(Presentation(key));
            }
            h.Barrier();
            h.Reconnect();
            h.Barrier();
            Require(h.Reports() == 0 && h.Session.CompletedChecksRevision() == 40,
                "Queued burst reconnect repeated reports or lost check acknowledgments");
            Ship::GameIpcItemPresentation item;
            for (int i = 0; i < 40; ++i) {
                const auto key = game + ":BURST_" + std::to_string(i);
                Require(h.Session.TryPopItemPresentation(item) && item.CheckKey == key,
                    "Burst lost or reordered an undelivered reward");
                Require(h.Session.IsCheckPresentationPending(key), "Burst actor was released before presentation");
                h.Session.CompleteCheckPresentation(key);
                Require(!h.Session.IsCheckPresentationPending(key), "Burst completion did not release its reward");
            }
            Require(!h.Session.TryPopItemPresentation(item), "Duplicate burst presentation survived");
            h.Evidence(evidence / (game + "-burst-reconnect.json"));
        }
        {
            Harness h;
            h.Session.SendCheckCollected("mm", "A", true);
            h.Welcome(0);
            h.Barrier();
            Require(h.Reports() == 0 && h.Session.GetLastError().find("correlated check rewards") != std::string::npos,
                "Incompatible launcher did not clearly reject correlated rewards");
            Require(h.Session.SendCheckCollected("mm", "LEGACY", false), "Legacy check compatibility failed");
            auto report = h.Receive("checkCollected");
            Require(report["check"] == "mm:LEGACY" && !report.contains("present"), "Legacy report changed format");
            h.Evidence(evidence / "compatibility.json");
        }
        std::cout << "Game IPC reward ordering, deduplication, reconnect and compatibility passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
