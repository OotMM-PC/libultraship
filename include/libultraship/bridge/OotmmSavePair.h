#pragma once

#include "libultraship/bridge/OotmmSaveRecovery.h"
#include <functional>
#include <nlohmann/json.hpp>
#include <set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace Ship {
enum class SavePairStage { Prepared, BackupWritten, PrimaryWritten, Committed };
using SavePairObserver = std::function<void(SavePairStage)>;
using SavePairValidator = std::function<bool(const std::string&, std::string&)>;

namespace SavePairDetail {
inline constexpr size_t MaxJournalBytes = 128 * 1024 * 1024;

inline std::filesystem::path JournalPath(const std::filesystem::path& primary) {
    auto path = primary;
    path += ".pair-transaction";
    return path;
}

class Lock {
  public:
    explicit Lock(const std::filesystem::path& primary, std::string& error) {
        auto path = primary;
        path += ".pair-lock";
#ifdef _WIN32
        mHandle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (mHandle == INVALID_HANDLE_VALUE) {
            error = "Cannot lock the save pair: " + std::error_code(GetLastError(), std::system_category()).message();
        }
#else
        mHandle = open(path.c_str(), O_RDWR | O_CREAT, 0600);
        if (mHandle >= 0 && flock(mHandle, LOCK_EX | LOCK_NB) != 0) {
            close(mHandle);
            mHandle = -1;
        }
        if (mHandle < 0) {
            error = "Cannot lock the save pair.";
        }
#endif
    }
    ~Lock() {
#ifdef _WIN32
        if (mHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(mHandle);
        }
#else
        if (mHandle >= 0) {
            close(mHandle);
        }
#endif
    }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    bool Acquired() const {
#ifdef _WIN32
        return mHandle != INVALID_HANDLE_VALUE;
#else
        return mHandle >= 0;
#endif
    }
  private:
#ifdef _WIN32
    HANDLE mHandle = INVALID_HANDLE_VALUE;
#else
    int mHandle = -1;
#endif
};

inline bool ValidPaths(const std::filesystem::path& primary, const std::filesystem::path& backup,
                       const std::string& saveId, uint32_t slot, std::string& error) {
    if (slot > 2 || saveId.size() != 32 || saveId.find_first_not_of("0123456789abcdef") != std::string::npos ||
        primary.parent_path().lexically_normal() != backup.parent_path().lexically_normal() ||
        primary.filename() != "file" + std::to_string(slot + 1) + ".json" ||
        backup.filename() != "file" + std::to_string(slot + 1) + "backup.json") {
        error = "Invalid save pair paths or identity.";
        return false;
    }
    return true;
}

inline bool ReadJournal(const std::filesystem::path& path, nlohmann::json& journal, bool& exists,
                        std::string& error) {
    std::error_code ec;
    exists = std::filesystem::exists(path, ec);
    if (ec || !exists) {
        error = ec ? ec.message() : "";
        return !ec;
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > MaxJournalBytes) {
        error = "Save pair transaction is unreadable or too large.";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    std::string text(static_cast<size_t>(size), '\0');
    input.read(text.data(), static_cast<std::streamsize>(size));
    if (!input || input.peek() != EOF) {
        error = "Save pair transaction changed while being read.";
        return false;
    }
    try {
        std::set<std::string> keys;
        bool unique = true;
        journal = nlohmann::json::parse(text, [&](int depth, nlohmann::json::parse_event_t event,
                                                 nlohmann::json& value) {
            if (depth > 1 || (event == nlohmann::json::parse_event_t::key &&
                              !keys.insert(value.get<std::string>()).second)) {
                unique = false;
            }
            return true;
        });
        if (!unique || !journal.is_object() || journal.size() != 8) {
            throw std::runtime_error("Invalid save pair transaction structure.");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

inline bool Write(const std::filesystem::path& path, const std::string& bytes, std::string& error) {
    std::error_code ec;
    if (!WriteFileAtomically(path, bytes, ec)) {
        error = path.filename().string() + ": " + ec.message();
        return false;
    }
    return true;
}

inline bool RecoverLocked(const std::filesystem::path& primary, const std::filesystem::path& backup,
                          const std::string& saveId, uint32_t slot, std::string& error,
                          const SavePairValidator& validate) {
    const auto path = JournalPath(primary);
    nlohmann::json journal;
    bool exists;
    if (!ReadJournal(path, journal, exists, error) || !exists) {
        return !exists && error.empty();
    }
    try {
        if (!journal.at("version").is_number_unsigned() || journal.at("version") != 1 ||
            !journal.at("slot").is_number_unsigned() || journal.at("slot") != slot ||
            journal.at("saveId") != saveId || !journal.at("committed").is_boolean()) {
            throw std::runtime_error("Save pair transaction does not match this save and slot.");
        }
        for (const char* name : {"beforePrimary", "beforeBackup", "afterPrimary", "afterBackup"}) {
            const auto& value = journal.at(name);
            const bool nullable = std::string_view(name).starts_with("before");
            if ((nullable && value.is_null())) {
                continue;
            }
            if (!value.is_string() || value.get_ref<const std::string&>().size() > 8 * 1024 * 1024) {
                throw std::runtime_error("Invalid save pair transaction contents.");
            }
        }
        if (validate && (!validate(journal.at("afterPrimary").get<std::string>(), error) ||
                         !validate(journal.at("afterBackup").get<std::string>(), error))) {
            return false;
        }
        std::optional<std::string> current[2], before[2];
        const std::filesystem::path paths[] = {primary, backup};
        const char* beforeKeys[] = {"beforePrimary", "beforeBackup"};
        const char* afterKeys[] = {"afterPrimary", "afterBackup"};
        for (int index = 0; index < 2; ++index) {
            if (!journal.at(beforeKeys[index]).is_null()) {
                before[index] = journal.at(beforeKeys[index]).get<std::string>();
            }
            if (!ReadRecoveryBytes(paths[index], current[index], error)) {
                return false;
            }
            if (current[index] != before[index] && current[index] != journal.at(afterKeys[index]).get<std::string>()) {
                error = "Save pair changed outside its pending transaction. Files were retained.";
                return false;
            }
        }
        for (const int index : {1, 0}) {
            const std::optional<std::string> target = journal.at("committed").get<bool>()
                ? std::optional<std::string>(journal.at(afterKeys[index]).get<std::string>()) : before[index];
            if (current[index] == target) {
                continue;
            }
            if (target) {
                if (!Write(paths[index], *target, error)) {
                    return false;
                }
            } else {
                std::error_code ec;
                std::filesystem::remove(paths[index], ec);
                if (ec) {
                    error = ec.message();
                    return false;
                }
            }
        }
        std::error_code ec;
        std::filesystem::remove(path, ec);
        if (ec) {
            error = "Save pair transaction cleanup is pending: " + ec.message();
            return false;
        }
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
}

inline bool RecoverSavePair(const std::filesystem::path& primary, const std::filesystem::path& backup,
                            const std::string& saveId, uint32_t slot, std::string& error,
                            const SavePairValidator& validate = {}) {
    error.clear();
    if (!SavePairDetail::ValidPaths(primary, backup, saveId, slot, error)) {
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::exists(primary.parent_path(), ec)) {
        if (ec) {
            error = ec.message();
        }
        return !ec;
    }
    SavePairDetail::Lock lock(primary, error);
    return lock.Acquired() && SavePairDetail::RecoverLocked(primary, backup, saveId, slot, error, validate);
}

inline bool WriteSavePairAtomically(const std::filesystem::path& primary, const std::filesystem::path& backup,
                                  const std::string& primaryBytes, const std::string& backupBytes,
                                  const std::string& saveId, uint32_t slot, std::string& error,
                                  const SavePairObserver& observe = {}, const SavePairValidator& validate = {}) {
    error.clear();
    if (!SavePairDetail::ValidPaths(primary, backup, saveId, slot, error) || primaryBytes.size() > 8 * 1024 * 1024 ||
        backupBytes.size() > 8 * 1024 * 1024 || (validate && (!validate(primaryBytes, error) || !validate(backupBytes, error)))) {
        if (error.empty()) {
            error = "Invalid save pair contents.";
        }
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(primary.parent_path(), ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    SavePairDetail::Lock lock(primary, error);
    if (!lock.Acquired() || !SavePairDetail::RecoverLocked(primary, backup, saveId, slot, error, validate)) {
        return false;
    }
    std::optional<std::string> beforePrimary, beforeBackup;
    if (!ReadRecoveryBytes(primary, beforePrimary, error) || !ReadRecoveryBytes(backup, beforeBackup, error)) {
        return false;
    }
    nlohmann::json journal = {{"version", 1u}, {"saveId", saveId}, {"slot", slot}, {"committed", false},
        {"beforePrimary", beforePrimary ? nlohmann::json(*beforePrimary) : nlohmann::json(nullptr)},
        {"beforeBackup", beforeBackup ? nlohmann::json(*beforeBackup) : nlohmann::json(nullptr)},
        {"afterPrimary", primaryBytes}, {"afterBackup", backupBytes}};
    const auto path = SavePairDetail::JournalPath(primary);
    const auto prepared = journal.dump();
    if (prepared.size() > SavePairDetail::MaxJournalBytes) {
        error = "Save pair transaction exceeds the supported size. Previous save files were retained.";
        return false;
    }
    if (!SavePairDetail::Write(path, prepared, error)) {
        return false;
    }
    const auto notify = [&](SavePairStage stage) { if (observe) { observe(stage); } };
    const auto rollback = [&] {
        std::string recoveryError;
        if (!SavePairDetail::RecoverLocked(primary, backup, saveId, slot, recoveryError, validate)) {
            error += "; save pair recovery is pending: " + recoveryError;
        }
        return false;
    };
    notify(SavePairStage::Prepared);
    if (!SavePairDetail::Write(backup, backupBytes, error)) {
        return rollback();
    }
    notify(SavePairStage::BackupWritten);
    if (!SavePairDetail::Write(primary, primaryBytes, error)) {
        return rollback();
    }
    notify(SavePairStage::PrimaryWritten);
    journal["committed"] = true;
    if (!SavePairDetail::Write(path, journal.dump(), error)) {
        return rollback();
    }
    notify(SavePairStage::Committed);
    return SavePairDetail::RecoverLocked(primary, backup, saveId, slot, error, validate);
}
}
