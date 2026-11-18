#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "ship/Context.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Ship {
/// Offset of a code address inside the game executable, so a dump can be matched against its symbols.
inline uintptr_t OotmmStateDump_CodeOffset(const void* address) {
#ifdef _WIN32
    return reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
#else
    return reinterpret_cast<uintptr_t>(address);
#endif
}

/// Writes a state dump into the "dumps" folder beside the crash reports. Returns its path, empty on failure.
inline std::string OotmmStateDump_Write(const std::string& game, nlohmann::json dump) {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
    dump["written"] = stamp;

    std::error_code error;
    const std::filesystem::path directory = Context::GetPathRelativeToAppDirectory("dumps");
    std::filesystem::create_directories(directory, error);
    const std::filesystem::path path = directory / (game + "-" + stamp + ".json");
    std::ofstream file(path);
    if (!file) {
        return {};
    }
    file << dump.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
    file.close();
    return file ? path.string() : std::string();
}
} // namespace Ship
