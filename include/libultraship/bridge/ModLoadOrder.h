#pragma once

#include <string>
#include <vector>

namespace Ship {

class ModLoadOrder {
  public:
    static std::vector<std::string> Read(const std::string& modsDirectory);

    static int RankOf(const std::vector<std::string>& order, const std::string& fileOrPath);

    static void SortForMounting(std::vector<std::string>& filesOrPaths, const std::string& modsDirectory);
};

} // namespace Ship
