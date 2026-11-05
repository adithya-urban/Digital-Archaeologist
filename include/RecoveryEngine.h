/**
 * @file RecoveryEngine.h
 * @brief Deleted-file recovery engine — the "digital archaeologist".
 *
 * OS Concepts Demonstrated:
 * - After deletion the OS marks blocks as free but does NOT zero them.
 *   Data persists until those blocks are reused by a new file.
 * - Recovery is possible only while the original blocks remain intact.
 *   Partial recovery occurs when some blocks have been overwritten.
 * - Forensic scanning: enumerating all soft-deleted directory entries and
 *   checking block-level integrity for each.
 */
#pragma once

#include "VirtualDisk.h"
#include "FileSystem.h"

#include <string>
#include <vector>

namespace da {

// ─── Recovery Outcome ────────────────────────────────────────────────────────
struct RecoveryResult {
    bool        success            = false;
    bool        partial            = false;
    std::string recovered_content;
    int         blocks_recovered   = 0;
    int         blocks_overwritten = 0;
    int         total_blocks       = 0;
    std::string message;
};

// ─── Recovery Engine ─────────────────────────────────────────────────────────
class RecoveryEngine {
public:
    RecoveryEngine(VirtualDisk& disk, FileSystem& fs);

    /// Return pointers to every soft-deleted directory entry.
    std::vector<const FileMetadata*> scanDeletedFiles() const;

    /// Non-destructive analysis: can the file be recovered, and how completely?
    RecoveryResult analyzeRecoverability(const std::string& filename) const;

    /// Actually restore the file (re-mark blocks as ALLOCATED, clear deleted flag).
    RecoveryResult attemptRecovery(const std::string& filename);

    // Display helpers ───────────────────────────────────────────────────────
    void displayRecoveryReport(const RecoveryResult& result) const;
    void displayForensicScan()  const;

private:
    VirtualDisk& disk_;
    FileSystem&  fs_;
};

} // namespace da

