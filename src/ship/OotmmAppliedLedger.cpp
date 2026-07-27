#include "libultraship/bridge/OotmmAppliedLedger.h"

#include <fstream>
#include <sstream>

namespace Ship {

void OotmmAppliedLedger::Reset() {
    mApplied.clear();
}

namespace {
// Bumped when a fix changes what "already applied" means, so stale ledgers are dropped
// instead of suppressing grants that never really landed.
constexpr const char* kLedgerVersion = "#ootmm-applied-2";
} // namespace

bool OotmmAppliedLedger::Load(const std::string& path) {
    mApplied.clear();
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }
    std::string version;
    if (!std::getline(input, version) || version != kLedgerVersion) {
        return false;
    }
    std::string line;
    while (std::getline(input, line)) {
        const size_t space = line.rfind(' ');
        if (space == std::string::npos || space == 0) {
            continue;
        }
        const std::string key = line.substr(0, space);
        uint32_t value = 0;
        std::istringstream(line.substr(space + 1)) >> value;
        if (!key.empty()) {
            mApplied[key] = value;
        }
    }
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

bool OotmmAppliedLedger::Has(const std::string& key) const {
    return mApplied.find(key) != mApplied.end();
}

} // namespace Ship
