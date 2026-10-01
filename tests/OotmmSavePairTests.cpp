#include "libultraship/bridge/OotmmSavePair.h"
#include <iostream>
#include <stdexcept>

namespace {
const std::string saveId(32, 'a');
void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
void Put(const std::filesystem::path& path, const std::string& bytes) {
    std::error_code error;
    Require(Ship::WriteFileAtomically(path, bytes, error), "Could not initialize test file");
}
std::optional<std::string> Read(const std::filesystem::path& path) {
    std::optional<std::string> bytes;
    std::string error;
    Require(Ship::ReadRecoveryBytes(path, bytes, error), "Could not read test file");
    return bytes;
}
struct Pair {
    std::filesystem::path primary, backup, journal;
    explicit Pair(const std::filesystem::path& root) {
        std::filesystem::create_directories(root);
        primary = root / "file1.json";
        backup = root / "file1backup.json";
        journal = Ship::SavePairDetail::JournalPath(primary);
    }
    void Initialize() const {
        Put(primary, "old-primary");
        Put(backup, "old-backup");
    }
    void Check(bool committed) const {
        Require(Read(primary) == (committed ? "new-primary" : "old-primary"), "Primary generation was wrong");
        Require(Read(backup) == (committed ? "new-backup" : "old-backup"), "Backup generation was wrong");
    }
};
void Interrupt(const Pair& pair, Ship::SavePairStage stage) {
    std::string error;
    bool interrupted = false;
    try {
        Ship::WriteSavePairAtomically(pair.primary, pair.backup, "new-primary", "new-backup", saveId, 0, error,
            [&](Ship::SavePairStage current) { if (current == stage) { throw 1; } });
    } catch (int) {
        interrupted = true;
    }
    Require(interrupted && std::filesystem::exists(pair.journal), "Interruption did not retain a transaction");
}
#ifdef _WIN32
HANDLE Block(const std::filesystem::path& path) {
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(handle != INVALID_HANDLE_VALUE, "Could not hold real Windows replacement lock");
    return handle;
}
#endif
}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "Expected isolated evidence directory");
        const auto root = std::filesystem::path(argv[1]);
        std::string error;
        int index = 0;
        for (const auto stage : {Ship::SavePairStage::Prepared, Ship::SavePairStage::BackupWritten,
                                 Ship::SavePairStage::PrimaryWritten, Ship::SavePairStage::Committed}) {
            const Pair pair(root / ("interrupted-" + std::to_string(index++)));
            pair.Initialize();
            Interrupt(pair, stage);
            Require(Ship::RecoverSavePair(pair.primary, pair.backup, saveId, 0, error), "Stage recovery failed");
            pair.Check(stage == Ship::SavePairStage::Committed);
            Require(!std::filesystem::exists(pair.journal), "Recovered transaction was not retired");
            Require(Ship::RecoverSavePair(pair.primary, pair.backup, saveId, 0, error), "Repeated recovery failed");
            pair.Check(stage == Ship::SavePairStage::Committed);
        }
        const Pair missing(root / "missing");
        Interrupt(missing, Ship::SavePairStage::BackupWritten);
        Require(Ship::RecoverSavePair(missing.primary, missing.backup, saveId, 0, error), "Missing preimage recovery failed");
        Require(!Read(missing.primary) && !Read(missing.backup), "Missing preimages became invented saves");
        const Pair guarded(root / "guarded");
        guarded.Initialize();
        Interrupt(guarded, Ship::SavePairStage::BackupWritten);
        const auto beforeJournal = Read(guarded.journal);
        Require(!Ship::RecoverSavePair(guarded.primary, guarded.backup, std::string(32, 'b'), 0, error), "Foreign identity was accepted");
        Require(Read(guarded.journal) == beforeJournal, "Rejected recovery changed its journal");
        Put(guarded.primary, "external-progress");
        Require(!Ship::RecoverSavePair(guarded.primary, guarded.backup, saveId, 0, error), "External progress was overwritten");
        Require(Read(guarded.primary) == "external-progress" && Read(guarded.backup) == "new-backup", "Rejected recovery changed files");
        Put(guarded.primary, "old-primary");
        Require(!Ship::RecoverSavePair(guarded.primary, guarded.backup, saveId, 0, error,
            [](const std::string&, std::string& reason) { reason = "Native payload rejected"; return false; }),
            "Invalid native payload was accepted");
        Require(Read(guarded.journal) == beforeJournal, "Native validation failure changed its journal");
        Require(Ship::RecoverSavePair(guarded.primary, guarded.backup, saveId, 0, error), "Validated retry failed");
        guarded.Check(false);
        const Pair malformed(root / "malformed");
        malformed.Initialize();
        Put(malformed.journal, "{\"version\":1,\"version\":1}");
        Require(!Ship::RecoverSavePair(malformed.primary, malformed.backup, saveId, 0, error), "Ambiguous transaction was accepted");
        malformed.Check(false);
        const Pair invalid(root / "invalid");
        invalid.Initialize();
        Require(!Ship::WriteSavePairAtomically(invalid.primary, invalid.primary, "new-primary", "new-backup", saveId, 0, error), "Aliased paths were accepted");
        Require(!Ship::WriteSavePairAtomically(invalid.primary, invalid.backup, "new-primary", "new-backup", saveId, 1, error), "Wrong slot was accepted");
        invalid.Check(false);
        const Pair bounded(root / "bounded-journal");
        const std::string oldBytes(6 * 1024 * 1024, '\0'), newBytes(6 * 1024 * 1024, '\1');
        Put(bounded.primary, oldBytes);
        Put(bounded.backup, oldBytes);
        bool notified = false;
        Require(!Ship::WriteSavePairAtomically(bounded.primary, bounded.backup, newBytes, newBytes, saveId, 0, error,
            [&](Ship::SavePairStage) { notified = true; }), "Oversized journal was accepted");
        Require(!notified && !std::filesystem::exists(bounded.journal), "Oversized journal was published");
        Require(Read(bounded.primary) == oldBytes && Read(bounded.backup) == oldBytes,
                "Oversized transaction changed the previous save pair");
        const Pair payloadBound(root / "bounded-payload");
        payloadBound.Initialize();
        Require(!Ship::WriteSavePairAtomically(payloadBound.primary, payloadBound.backup,
            std::string(8 * 1024 * 1024 + 1, 'x'), "new-backup", saveId, 0, error), "Oversized payload was accepted");
        payloadBound.Check(false);
        Require(!std::filesystem::exists(payloadBound.journal), "Oversized payload published a transaction");
        for (const uint32_t slot : {0u, 1u, 2u}) {
            const auto directory = root / ("slot-" + std::to_string(slot));
            std::filesystem::create_directories(directory);
            const auto primary = directory / ("file" + std::to_string(slot + 1) + ".json");
            const auto backup = directory / ("file" + std::to_string(slot + 1) + "backup.json");
            const std::string maximum(8 * 1024 * 1024, 'x');
            Require(Ship::WriteSavePairAtomically(primary, backup, maximum, maximum, saveId, slot, error),
                    "Supported payload limit or logical slot was rejected");
            Require(Read(primary) == maximum && Read(backup) == maximum, "Supported payload or slot was changed");
            Require(Ship::RecoverSavePair(primary, backup, saveId, slot, error), "Supported slot recovery failed");
        }
#ifdef _WIN32
        for (const bool blockPrimary : {false, true}) {
            const Pair pair(root / (blockPrimary ? "primary-locked" : "backup-locked"));
            pair.Initialize();
            HANDLE handle = Block(blockPrimary ? pair.primary : pair.backup);
            const bool written = Ship::WriteSavePairAtomically(pair.primary, pair.backup, "new-primary", "new-backup", saveId, 0, error);
            CloseHandle(handle);
            Require(!written, "Replacement lock was ignored");
            pair.Check(false);
            Require(!std::filesystem::exists(pair.journal), "Successful rollback left a transaction");
            Require(Ship::WriteSavePairAtomically(pair.primary, pair.backup, "new-primary", "new-backup", saveId, 0, error), "Replacement retry failed");
            pair.Check(true);
        }
        const Pair rollback(root / "rollback-locked");
        rollback.Initialize();
        HANDLE primaryLock = Block(rollback.primary), backupLock = INVALID_HANDLE_VALUE;
        const bool rolled = Ship::WriteSavePairAtomically(rollback.primary, rollback.backup, "new-primary", "new-backup", saveId, 0, error,
            [&](Ship::SavePairStage stage) { if (stage == Ship::SavePairStage::BackupWritten) { backupLock = Block(rollback.backup); } });
        Require(!rolled && std::filesystem::exists(rollback.journal), "Blocked rollback did not remain recoverable");
        Require(Read(rollback.primary) == "old-primary" && Read(rollback.backup) == "new-backup", "Blocked rollback lost either generation");
        Require(!Ship::RecoverSavePair(rollback.primary, rollback.backup, saveId, 0, error), "Blocked recovery falsely succeeded");
        CloseHandle(primaryLock);CloseHandle(backupLock);
        Require(Ship::RecoverSavePair(rollback.primary, rollback.backup, saveId, 0, error), "Unblocked rollback failed");
        rollback.Check(false);
        for (const auto stage : {Ship::SavePairStage::PrimaryWritten, Ship::SavePairStage::Committed}) {
            const Pair pair(root / (stage == Ship::SavePairStage::Committed ? "cleanup-locked" : "commit-locked"));
            pair.Initialize();
            HANDLE journalLock = INVALID_HANDLE_VALUE;
            const bool result = Ship::WriteSavePairAtomically(pair.primary, pair.backup, "new-primary", "new-backup", saveId, 0, error,
                [&](Ship::SavePairStage current) { if (current == stage) { journalLock = Block(pair.journal); } });
            Require(!result && std::filesystem::exists(pair.journal), "Journal failure was ignored");
            pair.Check(stage == Ship::SavePairStage::Committed);
            CloseHandle(journalLock);
            Require(Ship::RecoverSavePair(pair.primary, pair.backup, saveId, 0, error), "Journal failure recovery failed");
            pair.Check(stage == Ship::SavePairStage::Committed);
        }
        const Pair exclusive(root / "exclusive");
        exclusive.Initialize();
        bool rejected = false;
        Require(Ship::WriteSavePairAtomically(exclusive.primary, exclusive.backup, "new-primary", "new-backup", saveId, 0, error,
            [&](Ship::SavePairStage stage) { if (stage == Ship::SavePairStage::Prepared) { std::string reason; rejected = !Ship::RecoverSavePair(exclusive.primary, exclusive.backup, saveId, 0, reason); } }),
            "Owned transaction failed");
        Require(rejected, "A second pair writer acquired the same lock");
#endif
        std::cout << "Save pair filesystem, failure, recovery and identity regressions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
