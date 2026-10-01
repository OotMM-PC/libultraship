#include "libultraship/bridge/OotmmSaveRecovery.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static void Write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary);
    output << text;
    Require(output.good(), "Fixture write failed");
}
static std::string Read(const std::filesystem::path& path) {
    std::optional<std::string> value;
    std::string error;
    Require(Ship::ReadRecoveryBytes(path, value, error) && value.has_value(), "Read failed");
    return *value;
}
int main(int argc, char** argv) {
    Require(argc == 2, "Evidence directory required");
    const auto root = std::filesystem::absolute(argv[1]) /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    Require(!std::filesystem::exists(root), "Use a new evidence directory");
    std::filesystem::create_directories(root);
    const auto primary = root / "file3.json";
    const auto backup = root / "file3backup.json";
    const auto retained = root / "file3.json.recovery-original-0";
    std::string error;
    Write(primary, "truncated"); Write(backup, "last-good");
    Require(Ship::RestoreNativeBackup(primary, backup, std::string("truncated"), "last-good", error), "Restore failed");
    Require(Read(primary) == "last-good" && Read(backup) == "last-good" && Read(retained) == "truncated", "Recovery lost bytes");
    Write(primary, "new-progress");
    Require(!Ship::RestoreNativeBackup(primary, backup, std::string("truncated"), "last-good", error), "Stale primary accepted");
    Require(Read(primary) == "new-progress", "Stale request overwrote progress");
    Require(!Ship::RestoreNativeBackup(primary, backup, std::string("new-progress"), "stale-backup", error), "Stale backup accepted");
    Require(Read(primary) == "new-progress", "Stale backup changed primary");
    const auto missing = root / "missing.json";
    Require(Ship::RestoreNativeBackup(missing, backup, std::nullopt, "last-good", error), "Missing primary recovery failed");
    Require(Read(missing) == "last-good", "Missing primary content differs");
#ifdef _WIN32
    HANDLE lock = CreateFileW(primary.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Require(lock != INVALID_HANDLE_VALUE, "Could not lock primary");
    const bool result = Ship::RestoreNativeBackup(primary, backup, std::string("new-progress"), "last-good", error);
    CloseHandle(lock);
    Require(!result && Read(primary) == "new-progress" && Read(backup) == "last-good", "Locked replacement lost data");
    Require(Read(root / "file3.json.recovery-original-1") == "new-progress", "Preimage not preserved before replacement");
#endif
    Require(Ship::RestoreNativeBackup(primary, backup, std::string("new-progress"), "last-good", error), "Retry failed");
    Require(Read(retained) == "truncated", "Recovery overwrote older archive");
    const auto blocked = root / "blocked.json";
    Write(blocked, "damaged");
    std::filesystem::create_directory(root / "blocked.json.recovery-original-0");
    Require(!Ship::RestoreNativeBackup(blocked, backup, std::string("damaged"), "last-good", error), "Archive failure ignored");
    Require(Read(blocked) == "damaged" && Read(backup) == "last-good", "Archive failure lost data");
    std::cout << "Native backup restoration filesystem contracts passed\n";
}
