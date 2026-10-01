#include "ship/utils/AtomicFile.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Ship {
bool WriteFileAtomically(const std::filesystem::path& destination, std::string_view contents,
                         std::error_code& error) {
    static std::atomic<uint64_t> serial{ 0 };
    error.clear();
    std::filesystem::path temporary;
#ifdef _WIN32
    const auto process = GetCurrentProcessId();
#else
    const auto process = getpid();
#endif
#ifdef _WIN32
    HANDLE file = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 32; ++attempt) {
        temporary = destination;
        temporary += ".tmp-" + std::to_string(process) + "-" + std::to_string(++serial);
        file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS) {
            break;
        }
    }
    if (file == INVALID_HANDLE_VALUE) {
        error = std::error_code(GetLastError(), std::system_category());
        return false;
    }
    size_t offset = 0;
    while (offset < contents.size()) {
        DWORD written = 0;
        const auto length = static_cast<DWORD>(std::min<size_t>(contents.size() - offset, 1024 * 1024));
        if (!WriteFile(file, contents.data() + offset, length, &written, nullptr)) {
            error = std::error_code(GetLastError(), std::system_category());
            break;
        }
        if (written == 0) {
            error = std::make_error_code(std::errc::io_error);
            break;
        }
        offset += written;
    }
    if (!error && !FlushFileBuffers(file)) {
        error = std::error_code(GetLastError(), std::system_category());
    }
    if (!CloseHandle(file) && !error) {
        error = std::error_code(GetLastError(), std::system_category());
    }
    if (!error && !MoveFileExW(temporary.c_str(), destination.c_str(),
                                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = std::error_code(GetLastError(), std::system_category());
    }
#else
    int file = -1;
    for (int attempt = 0; attempt < 32; ++attempt) {
        temporary = destination;
        temporary += ".tmp-" + std::to_string(process) + "-" + std::to_string(++serial);
        file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (file >= 0 || errno != EEXIST) {
            break;
        }
    }
    if (file < 0) {
        error = std::error_code(errno, std::generic_category());
        return false;
    }
    size_t offset = 0;
    while (offset < contents.size()) {
        const auto written = write(file, contents.data() + offset,
                                   std::min<size_t>(contents.size() - offset, 1024 * 1024));
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            error = std::error_code(written < 0 ? errno : EIO, std::generic_category());
            break;
        }
        offset += static_cast<size_t>(written);
    }
    if (!error && fsync(file) != 0) {
        error = std::error_code(errno, std::generic_category());
    }
    if (close(file) != 0 && !error) {
        error = std::error_code(errno, std::generic_category());
    }
    if (!error) {
        std::filesystem::rename(temporary, destination, error);
    }
#endif
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}
}
