#include "ship/utils/AtomicFile.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Expected evidence root");
        const auto directory = std::filesystem::absolute(argv[1]) /
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(directory);
        const auto path = directory / std::filesystem::path(u8"save-\u00e9.json");
        auto require = [](bool pass, const char* message) { if (!pass) throw std::runtime_error(message); };
        auto read = [&]() {
            std::ifstream file(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), {});
        };
        std::error_code error;
        const std::string original = "{\"collected\":[\"first\"]}\n";
#ifdef _WIN32
        auto stale = path;
        stale += ".tmp-" + std::to_string(GetCurrentProcessId()) + "-1";
        std::ofstream(stale) << "Interrupted write from a reused process ID";
#endif
        require(Ship::WriteFileAtomically(path, original, error), "Initial write failed");
        require(read() == original, "Initial content mismatch");
#ifdef _WIN32
        require(std::filesystem::exists(stale), "Writer removed a temporary file it did not create");
        std::filesystem::remove(stale);
        const HANDLE lock = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        require(lock != INVALID_HANDLE_VALUE, "Cannot lock destination");
        const bool replaced = Ship::WriteFileAtomically(path, "incomplete", error);
        CloseHandle(lock);
        require(!replaced && bool(error), "Locked replacement unexpectedly succeeded");
        require(read() == original, "Failed replacement destroyed previous save");
#endif
        const std::string large(3 * 1024 * 1024, 'x');
        require(Ship::WriteFileAtomically(path, large, error), "Multi-chunk replacement failed");
        require(!error && read() == large, "Replacement mismatch or stale error");
        require(Ship::WriteFileAtomically(path, {}, error), "Empty replacement failed");
        require(read().empty(), "Empty replacement retained old bytes");
        require(!Ship::WriteFileAtomically(directory / "missing" / "file.json", original, error),
                "Missing parent unexpectedly accepted");
        size_t files = 0;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) ++files;
        require(files == 1, "Normal or failed write left temporary files behind");
        std::cout << "PASS: initial/replace/large/empty/unicode writes and failure preservation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
