#include "libultraship/bridge/OotmmAppliedLedger.h"

#include <fstream>
#include <charconv>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <limits>

namespace Ship {

void OotmmAppliedLedger::Reset() {
    mApplied.clear();
}

bool OotmmAppliedLedger::RestoreSnapshot(const nlohmann::json& snapshot) {
    if (!snapshot.is_object()) {
        return false;
    }
    std::unordered_map<std::string, uint32_t> restored;
    for (const auto& [key, value] : snapshot.items()) {
        if (key.empty() || key.find_first_of(" \t\r\n") != std::string::npos || !value.is_number_integer() ||
            (!value.is_number_unsigned() && value.get<int64_t>() < 0) ||
            value.get<uint64_t>() > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        restored.emplace(key, value.get<uint32_t>());
    }
    mApplied = std::move(restored);
    return true;
}

namespace {
// Bumped when a fix changes what "already applied" means, so stale ledgers are dropped
// instead of suppressing grants that never really landed.
constexpr const char* kLedgerVersion = "#ootmm-applied-2";
} // namespace

bool OotmmAppliedLedger::Load(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }
    std::string version;
    if (!std::getline(input, version) || version != kLedgerVersion) {
        return false;
    }
    std::unordered_map<std::string, uint32_t> restored;
    std::string line;
    while (std::getline(input, line)) {
        const size_t space = line.find(' ');
        if (space == std::string::npos || space == 0 || space + 1 == line.size()) {
            return false;
        }
        const std::string key = line.substr(0, space);
        if (key.find_first_of(" \t\r\n\v\f") != std::string::npos) {
            return false;
        }
        uint32_t value = 0;
        const char* first = line.data() + space + 1;
        const char* last = line.data() + line.size();
        const auto parsed = std::from_chars(first, last, value);
        if (parsed.ec != std::errc{} || parsed.ptr != last || !restored.emplace(key, value).second) {
            return false;
        }
    }
    if (input.bad()) {
        return false;
    }
    mApplied = std::move(restored);
    return true;
}

bool OotmmAppliedLedger::LoadLegacy(const std::string& primary, const std::string& fallback) {
    for (const auto& path : {primary, fallback}) {
        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        if (error) {
            return false;
        }
        if (exists) {
            return Load(path);
        }
    }
    Reset();
    return true;
}

bool OotmmAppliedLedger::Save(const std::string& path) const {
    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << kLedgerVersion << '\n';
    for (const auto& [key, value] : mApplied) {
        if (key.find(' ') == std::string::npos && key.find('\n') == std::string::npos) {
            output << key << ' ' << value << '\n';
        }
    }
    return output.good();
}

uint32_t OotmmAppliedLedger::Applied(const std::string& key) const {
    const auto found = mApplied.find(key);
    return found == mApplied.end() ? 0 : found->second;
}

void OotmmAppliedLedger::SetApplied(const std::string& key, uint32_t value) {
    mApplied[key] = value;
}

void OotmmAppliedLedger::Erase(const std::string& key) {
    mApplied.erase(key);
}

bool OotmmAppliedLedger::Has(const std::string& key) const {
    return mApplied.find(key) != mApplied.end();
}

} // namespace Ship
