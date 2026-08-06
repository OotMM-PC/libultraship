#include "libultraship/bridge/ModLoadOrder.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace Ship {

namespace {

std::string StemOf(const std::string& value) {
    const size_t slash = value.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? value : value.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

bool EqualsNoCase(const std::string& a, const std::string& b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
               return std::tolower(x) == std::tolower(y);
           });
}

std::filesystem::path ManifestPath(const std::string& modsDirectory) {
    return std::filesystem::path(modsDirectory) / "loadorder.txt";
}

// Launcher-generated archives (ported local models, co-op fills) never appear in the manifest
// and must win anyway: they mount after everything, ranked or not.
bool IsPinnedLast(const std::string& fileOrPath) {
    const size_t slash = fileOrPath.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? fileOrPath : fileOrPath.substr(slash + 1);
    return name.rfind("zz_localmodel_", 0) == 0 || name.rfind("zz_coop_", 0) == 0;
}

} // namespace

std::vector<std::string> ModLoadOrder::Read(const std::string& modsDirectory) {
    std::vector<std::string> order;
    if (modsDirectory.empty()) {
        return order;
    }

    std::ifstream in(ManifestPath(modsDirectory));
    if (!in) {
        return order;
    }

    bool first = true;
    std::string line;
    while (std::getline(in, line)) {
        if (first) {
            first = false;
            // A hand-edited manifest may lead with a UTF-8 BOM, which would corrupt the first entry.
            if (line.rfind("\xEF\xBB\xBF", 0) == 0) {
                line.erase(0, 3);
            }
        }
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        const size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line[start] == '#') {
            continue;
        }
        order.push_back(line.substr(start));
    }
    return order;
}

int ModLoadOrder::RankOf(const std::vector<std::string>& order, const std::string& fileOrPath) {
    const std::string stem = StemOf(fileOrPath);
    for (size_t i = 0; i < order.size(); ++i) {
        if (EqualsNoCase(StemOf(order[i]), stem)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void ModLoadOrder::SortForMounting(std::vector<std::string>& filesOrPaths, const std::string& modsDirectory) {
    if (filesOrPaths.empty()) {
        return;
    }

    const std::vector<std::string> order = Read(modsDirectory);
    std::stable_sort(filesOrPaths.begin(), filesOrPaths.end(),
                     [&order](const std::string& a, const std::string& b) {
                         const bool pinnedA = IsPinnedLast(a);
                         const bool pinnedB = IsPinnedLast(b);
                         if (pinnedA != pinnedB) {
                             return !pinnedA;
                         }
                         const int rankA = RankOf(order, a);
                         const int rankB = RankOf(order, b);
                         if ((rankA < 0) != (rankB < 0)) {
                             return rankA < 0; // unlisted mounts first, so ranked archives override it
                         }
                         if (rankA < 0) {
                             return false; // both unlisted: keep the caller's order
                         }
                         return rankA > rankB; // the top line mounts last and therefore wins
                     });
}

} // namespace Ship
