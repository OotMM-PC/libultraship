#pragma once

#include "ship/utils/AtomicFile.h"
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace Ship {
inline bool ReadRecoveryBytes(const std::filesystem::path& path, std::optional<std::string>& bytes,
                              std::string& error) {
    bytes.reset();
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    if (!exists) {
        return true;
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > 8 * 1024 * 1024) {
        error = "Recovery file is unreadable or too large.";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        error = "Recovery file cannot be opened.";
        return false;
    }
    std::string value(static_cast<size_t>(size), '\0');
    input.read(value.data(), static_cast<std::streamsize>(size));
    if (input.gcount() != static_cast<std::streamsize>(size) || input.bad() || input.peek() != EOF) {
        error = "Recovery file changed while being read.";
        return false;
    }
    bytes = std::move(value);
    return true;
}

inline bool RestoreNativeBackup(const std::filesystem::path& primary, const std::filesystem::path& backup,
                                const std::optional<std::string>& expectedPrimary, const std::string& expectedBackup,
                                std::string& error) {
    error.clear();
    if (primary == backup || expectedBackup.empty() || expectedBackup.size() > 8 * 1024 * 1024 ||
        (expectedPrimary && expectedPrimary->size() > 8 * 1024 * 1024)) {
        error = "Invalid recovery source or destination.";
        return false;
    }
    std::optional<std::string> actualPrimary;
    std::optional<std::string> actualBackup;
    if (!ReadRecoveryBytes(primary, actualPrimary, error) || !ReadRecoveryBytes(backup, actualBackup, error)) {
        return false;
    }
    if (actualPrimary != expectedPrimary || !actualBackup || *actualBackup != expectedBackup) {
        error = "Save files changed after recovery was prepared.";
        return false;
    }
    if (expectedPrimary) {
        bool preserved = false;
        for (unsigned index = 0; index < 128; ++index) {
            auto retained = primary;
            retained += ".recovery-original-" + std::to_string(index);
            std::optional<std::string> existing;
            if (!ReadRecoveryBytes(retained, existing, error)) {
                return false;
            }
            if (existing) {
                if (*existing == *expectedPrimary) {
                    preserved = true;
                    break;
                }
                continue;
            }
            std::error_code ec;
            if (!WriteFileAtomically(retained, *expectedPrimary, ec)) {
                error = "Could not preserve damaged save: " + ec.message();
                return false;
            }
            preserved = true;
            break;
        }
        if (!preserved) {
            error = "No recovery archive slot is available.";
            return false;
        }
    }
    if (!ReadRecoveryBytes(primary, actualPrimary, error) || !ReadRecoveryBytes(backup, actualBackup, error) ||
        actualPrimary != expectedPrimary || !actualBackup || *actualBackup != expectedBackup) {
        if (error.empty()) {
            error = "Save files changed before recovery could commit.";
        }
        return false;
    }
    std::error_code ec;
    if (!WriteFileAtomically(primary, expectedBackup, ec)) {
        error = "Could not restore native backup: " + ec.message();
        return false;
    }
    return true;
}
}
