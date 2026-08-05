#pragma once

#include <string>
#include <vector>

namespace Ship {

/// loadorder.txt ranks a mods directory's archives top-first; later mounts win, so ranked ones
/// mount in reverse and unlisted ones mount ahead of them all. The launcher owns it; games read.
class ModLoadOrder {
  public:
    static std::vector<std::string> Read(const std::string& modsDirectory);

    /// Rank within the order, or -1 when unlisted. Names match with or without their extension,
    /// so a caller holding bare stems compares the same as one holding full paths.
    static int RankOf(const std::vector<std::string>& order, const std::string& fileOrPath);

    /// Reorders into mount order. Stable: unlisted entries keep the order they came in with, so a
    /// caller's own preference survives for anything the file does not rank.
    static void SortForMounting(std::vector<std::string>& filesOrPaths, const std::string& modsDirectory);
};

} // namespace Ship
