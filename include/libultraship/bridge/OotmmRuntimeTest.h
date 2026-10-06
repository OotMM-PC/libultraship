#pragma once

#if !defined(NDEBUG) || defined(OOTMM_RUNTIME_TEST)
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>

namespace Ship {
class OotmmRuntimeTest {
  public:
    struct Pad {
        uint16_t Buttons = 0;
        int8_t X = 0;
        int8_t Y = 0;
    };

    struct Warp {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
        int16_t Yaw = 0;
    };

    static OotmmRuntimeTest& Get() {
        static OotmmRuntimeTest driver;
        return driver;
    }

    bool Enabled() const { return !mDirectory.empty() && mTrace.good(); }

    Pad Advance() {
        ++mFrame;
        if (mRemaining == 0) {
            ReadCommand();
        }
        if (mRemaining == 0) {
            return mHold ? mPad : Pad{};
        }
        --mRemaining;
        return mPad;
    }

    bool ShouldObserve() {
        static const int every = [] {
            const char* value = std::getenv("OOTMM_RUNTIME_OBS_EVERY");
            return value != nullptr && std::atoi(value) > 1 ? std::atoi(value) : 1;
        }();
        const bool finished = mRemaining == 0 && mSequence != mLastFinishedSequence;
        if (every > 1 && !finished && mFrame - mLastObservedFrame < static_cast<uint64_t>(every)) {
            return false;
        }
        mLastObservedFrame = mFrame;
        if (mRemaining == 0) {
            mLastFinishedSequence = mSequence;
        }
        return true;
    }

    std::optional<Warp> TakeWarp() {
        auto warp = mWarp;
        mWarp.reset();
        return warp;
    }

    std::optional<int> TakeRoom() {
        auto room = mRoom;
        mRoom.reset();
        return room;
    }

    void Observe(nlohmann::json state) {
        state["protocol"] = "ootmm.runtime-test.v1";
        state["frame"] = mFrame;
        state["sequence"] = mSequence;
        state["remainingFrames"] = mRemaining;
        Write(state);
    }

  private:
    OotmmRuntimeTest() {
        const char* directory = std::getenv("OOTMM_RUNTIME_TEST_DIR");
        if (directory == nullptr || directory[0] == '\0') {
            return;
        }
        std::error_code error;
        mDirectory = std::filesystem::u8path(directory);
        std::filesystem::create_directories(mDirectory, error);
        if (error) {
            mDirectory.clear();
            return;
        }
        mTrace.open(mDirectory / "observations.jsonl", std::ios::out | std::ios::trunc);
    }

    static int64_t Integer(const nlohmann::json& value, const char* key, int64_t min, int64_t max) {
        const auto& field = value.at(key);
        if (!field.is_number_integer() || (field.is_number_unsigned() && field.get<uint64_t>() > uint64_t(max))) {
            throw std::runtime_error(std::string("Invalid integer: ") + key);
        }
        const auto number = field.get<int64_t>();
        if (number < min || number > max) {
            throw std::runtime_error(std::string("Out of range: ") + key);
        }
        return number;
    }

    void ReadCommand() {
        const auto path = mDirectory / "input.json";
        std::error_code error;
        const auto stamp = std::filesystem::last_write_time(path, error);
        if (error || stamp == mStamp) {
            return;
        }
        try {
            std::ifstream stream(path);
            if (!stream) {
                return;
            }
            mStamp = stamp;
            const auto command = nlohmann::json::parse(stream);
            if (command.at("protocol") != "ootmm.runtime-test.v1") {
                throw std::runtime_error("Unsupported runtime-test protocol");
            }
            const auto sequence = Integer(command, "sequence", 1, INT64_MAX);
            if (sequence <= mSequence) {
                return;
            }
            const auto frames = Integer(command, "frames", 1, 3600);
            const auto buttons = Integer(command, "buttons", 0, 65535);
            const auto x = Integer(command, "stickX", -80, 80);
            const auto y = Integer(command, "stickY", -80, 80);
            std::optional<Warp> warp;
            if (const auto field = command.find("warp"); field != command.end()) {
                if (!field->is_array() || field->size() != 4 || !(*field)[0].is_number() ||
                    !(*field)[1].is_number() || !(*field)[2].is_number() || !(*field)[3].is_number_integer()) {
                    throw std::runtime_error("Invalid warp");
                }
                warp = Warp{ (*field)[0].get<float>(), (*field)[1].get<float>(), (*field)[2].get<float>(),
                             int16_t((*field)[3].get<int64_t>()) };
            }
            std::optional<int> room;
            if (command.contains("room")) {
                room = int(Integer(command, "room", 0, 63));
            }
            const bool hold = command.contains("hold") && command.at("hold").is_boolean() && command.at("hold").get<bool>();
            mSequence = sequence;
            mRemaining = frames;
            mPad = { uint16_t(buttons), int8_t(x), int8_t(y) };
            mWarp = warp;
            mRoom = room;
            mHold = hold;
            Write({ { "event", "inputAccepted" }, { "frame", mFrame }, { "command", command } });
        } catch (const std::exception& exception) {
            Write({ { "event", "inputRejected" }, { "frame", mFrame }, { "error", exception.what() } });
        }
    }

    void Write(const nlohmann::json& value) {
        mTrace << value.dump() << '\n';
        mTrace.flush();
    }

    std::filesystem::path mDirectory;
    std::filesystem::file_time_type mStamp{};
    std::ofstream mTrace;
    uint64_t mFrame = 0;
    uint64_t mLastObservedFrame = 0;
    int64_t mLastFinishedSequence = 0;
    int64_t mSequence = 0;
    int64_t mRemaining = 0;
    Pad mPad;
    std::optional<Warp> mWarp;
    std::optional<int> mRoom;
    bool mHold = false;
};
}
#endif
