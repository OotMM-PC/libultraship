#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <nlohmann/json_fwd.hpp>

namespace Ship {

/// Records how much of each OoTMM item has already been written into the native
/// save, so counters apply deltas instead of re-applying on every reload.
class OotmmAppliedLedger {
  public:
    void Reset();
    bool Load(const std::string& path);
    bool LoadLegacy(const std::string& primary, const std::string& fallback);
    bool Save(const std::string& path) const;

    uint32_t Applied(const std::string& key) const;
    void SetApplied(const std::string& key, uint32_t value);
    void Erase(const std::string& key);
    bool Has(const std::string& key) const;
    const std::unordered_map<std::string, uint32_t>& GetValues() const { return mApplied; }
    bool RestoreSnapshot(const nlohmann::json& snapshot);

  private:
    std::unordered_map<std::string, uint32_t> mApplied;
};

} // namespace Ship
