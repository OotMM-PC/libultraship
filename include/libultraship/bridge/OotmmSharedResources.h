#pragma once

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Ship {

struct OotmmSharedResources {
    static constexpr uint64_t MaxRevision = 9007199254740991ULL;
    std::string SaveId;
    uint32_t Slot = 0;
    uint64_t Revision = 0;
    std::string Source;
    std::map<std::string, uint32_t> Values;
    std::map<std::string, uint32_t> Applied;

    nlohmann::json Json() const {
        return {{"version", 1}, {"saveId", SaveId}, {"slot", Slot}, {"revision", Revision},
                {"source", Source}, {"values", Values}, {"applied", Applied}};
    }

    static std::optional<OotmmSharedResources> Next(const std::string& saveId, uint32_t slot,
        uint64_t previousRevision, const std::string& source, const std::map<std::string, uint32_t>& values,
        const std::map<std::string, uint32_t>& applied) {
        if (previousRevision >= MaxRevision) {
            return std::nullopt;
        }
        OotmmSharedResources next{saveId, slot, previousRevision + 1, source, values, applied};
        return Parse(next.Json().dump(), saveId, slot);
    }

    static std::optional<OotmmSharedResources> Parse(const std::string& text,
                                                    const std::string& saveId, uint32_t slot) {
        if (text.size() > 262144 || saveId.empty() || slot > 2) {
            return std::nullopt;
        }
        try {
            bool valid = true;
            std::vector<std::set<std::string>> keys;
            const auto root = nlohmann::json::parse(text, [&](int depth, nlohmann::json::parse_event_t event,
                                                            nlohmann::json& value) {
                if (depth > 8) {
                    valid = false;
                }
                if (event == nlohmann::json::parse_event_t::object_start) {
                    keys.emplace_back();
                } else if (event == nlohmann::json::parse_event_t::key) {
                    if (keys.empty() || !keys.back().insert(value.get<std::string>()).second) {
                        valid = false;
                    }
                } else if (event == nlohmann::json::parse_event_t::object_end && !keys.empty()) {
                    keys.pop_back();
                }
                return true;
            });
            if (!valid || !root.is_object() || root.size() != 7 || root.at("version") != 1 ||
                !Unsigned(root.at("version"), 1) || root.at("saveId") != saveId ||
                !Unsigned(root.at("slot"), 2) || root.at("slot") != slot ||
                !Unsigned(root.at("revision"), MaxRevision) || root.at("revision") == 0 ||
                !root.at("source").is_string()) {
                return std::nullopt;
            }
            OotmmSharedResources result;
            result.SaveId = saveId;
            result.Slot = slot;
            result.Revision = root.at("revision").get<uint64_t>();
            result.Source = root.at("source").get<std::string>();
            if ((result.Source != "oot" && result.Source != "mm") ||
                !ReadMap(root.at("values"), 32, 65535, false, result.Values) ||
                !ReadMap(root.at("applied"), 1024, 1000000, true, result.Applied)) {
                return std::nullopt;
            }
            return result;
        } catch (const nlohmann::json::exception&) {
            return std::nullopt;
        }
    }

  private:
    static bool Unsigned(const nlohmann::json& value, uint64_t maximum) {
        return value.is_number_integer() &&
               (value.is_number_unsigned() || value.get<int64_t>() >= 0) && value.get<uint64_t>() <= maximum;
    }

    static bool ReadMap(const nlohmann::json& value, size_t limit, uint32_t maximum, bool rewards,
                        std::map<std::string, uint32_t>& output) {
        if (!value.is_object() || value.size() > limit) {
            return false;
        }
        for (auto entry = value.begin(); entry != value.end(); ++entry) {
            const auto& name = entry.key();
            if (name.empty() || name.size() > 96 || (rewards && name.rfind("SHARED_", 0) != 0) ||
                !Unsigned(entry.value(), maximum)) {
                return false;
            }
            for (const char c : name) {
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '_')) {
                    return false;
                }
            }
            output.emplace(name, entry.value().get<uint32_t>());
        }
        return true;
    }
};

class OotmmSharedResourceState {
  public:
    bool Load(const std::optional<OotmmSharedResources>& boot, const nlohmann::json& saved,
              const std::string& saveId, uint32_t slot) {
        mValid = false;
        mRestore = false;
        mSaveId = saveId;
        mSlot = slot;
        mCurrent = boot;
        if (boot && (boot->SaveId != saveId || boot->Slot != slot)) {
            return false;
        }
        if (!saved.is_null()) {
            auto native = OotmmSharedResources::Parse(saved.dump(), saveId, slot);
            if (!native || (boot && native->Revision == boot->Revision && native->Json() != boot->Json())) {
                return false;
            }
            if (!mCurrent || native->Revision > mCurrent->Revision) {
                mCurrent = std::move(native);
            }
        }
        if (mCurrent) {
            mRevision = std::max(mRevision, mCurrent->Revision);
            mRestore = true;
        }
        mValid = !saveId.empty() && slot <= 2;
        return mValid;
    }

    bool Capture(const std::string& saveId, uint32_t slot, const std::string& source,
                 const std::map<std::string, uint32_t>& values,
                 const std::map<std::string, uint32_t>& applied, nlohmann::json& output) {
        if (!mValid || saveId != mSaveId || slot != mSlot) {
            return false;
        }
        if (mRestore && mCurrent) {
            output = mCurrent->Json();
            return true;
        }
        auto next = OotmmSharedResources::Next(saveId, slot, mRevision, source, values, applied);
        if (!next) {
            return false;
        }
        mRevision = next->Revision;
        mCurrent = std::move(next);
        output = mCurrent->Json();
        return true;
    }

    const std::optional<OotmmSharedResources>& Current() const { return mCurrent; }
    bool NeedsRestore() const { return mRestore; }
    void Restored() { mRestore = false; }

  private:
    std::optional<OotmmSharedResources> mCurrent;
    uint64_t mRevision = 0;
    bool mValid = false;
    bool mRestore = false;
    std::string mSaveId;
    uint32_t mSlot = 0;
};

}
