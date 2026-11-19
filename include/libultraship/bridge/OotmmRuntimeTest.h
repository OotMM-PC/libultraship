#pragma once

#if !defined(NDEBUG) || defined(OOTMM_RUNTIME_TEST)
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
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
        int8_t RightY = 0;
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
        if (mRemaining == 0 && !mSteps.empty()) {
            mRemaining = mSteps.front().Frames;
            mPad = mSteps.front().Input;
            mSteps.pop_front();
        }
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
        const bool finished = Pending() == 0 && mSequence != mLastFinishedSequence;
        if (every > 1 && !finished && mFrame - mLastObservedFrame < static_cast<uint64_t>(every)) {
            return false;
        }
        mLastObservedFrame = mFrame;
        if (Pending() == 0) {
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

    void HearAudio(const int16_t* samples, size_t count) {
        if (!Enabled() || count == 0) {
            return;
        }
        uint32_t peak = 0;
        uint64_t squares = 0;
        for (size_t i = 0; i < count; i++) {
            const int32_t sample = samples[i];
            peak = std::max(peak, static_cast<uint32_t>(std::abs(sample)));
            squares += static_cast<uint64_t>(sample * sample);
        }
        uint32_t known = mAudioPeak.load();
        while (peak > known && !mAudioPeak.compare_exchange_weak(known, peak)) {
        }
        mAudioSquares += squares;
        mAudioSamples += count;
        if (mAudioDump.is_open()) {
            mAudioDump.write(reinterpret_cast<const char*>(samples), count * sizeof(int16_t));
        }
    }

    void Observe(nlohmann::json state) {
        const uint64_t samples = mAudioSamples.exchange(0);
        const uint64_t squares = mAudioSquares.exchange(0);
        state["audioPeak"] = mAudioPeak.exchange(0);
        state["audioRms"] = samples == 0 ? 0 : static_cast<uint32_t>(std::sqrt(double(squares) / double(samples)));
        state["protocol"] = "ootmm.runtime-test.v1";
        state["frame"] = mFrame;
        state["sequence"] = mSequence;
        state["remainingFrames"] = Pending();
        Write(state);
    }

  private:
    struct Step {
        int64_t Frames = 0;
        Pad Input;
    };

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
        if (const char* dump = std::getenv("OOTMM_RUNTIME_AUDIO_DUMP"); dump != nullptr && dump[0] == '1') {
            mAudioDump.open(mDirectory / "audio.pcm", std::ios::out | std::ios::trunc | std::ios::binary);
        }
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

    static Step ReadStep(const nlohmann::json& value) {
        return { Integer(value, "frames", 1, 3600),
                 { uint16_t(Integer(value, "buttons", 0, 65535)), int8_t(Integer(value, "stickX", -80, 80)),
                   int8_t(Integer(value, "stickY", -80, 80)),
                   int8_t(value.contains("rightStickY") ? Integer(value, "rightStickY", -80, 80) : 0) } };
    }

    int64_t Pending() const {
        int64_t frames = mRemaining;
        for (const Step& step : mSteps) {
            frames += step.Frames;
        }
        return frames;
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
            const Step first = ReadStep(command);
            std::deque<Step> steps;
            if (const auto field = command.find("then"); field != command.end()) {
                if (!field->is_array()) {
                    throw std::runtime_error("Invalid then");
                }
                for (const auto& step : *field) {
                    steps.push_back(ReadStep(step));
                }
            }
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
            mRemaining = first.Frames;
            mPad = first.Input;
            mSteps = std::move(steps);
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
    std::ofstream mAudioDump;
    std::atomic<uint32_t> mAudioPeak{ 0 };
    std::atomic<uint64_t> mAudioSquares{ 0 };
    std::atomic<uint64_t> mAudioSamples{ 0 };
    uint64_t mFrame = 0;
    uint64_t mLastObservedFrame = 0;
    int64_t mLastFinishedSequence = 0;
    int64_t mSequence = 0;
    int64_t mRemaining = 0;
    Pad mPad;
    std::deque<Step> mSteps;
    std::optional<Warp> mWarp;
    std::optional<int> mRoom;
    bool mHold = false;
};
}
#endif
