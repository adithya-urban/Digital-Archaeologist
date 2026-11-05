/**
 * @file FileSystem.h
 * @brief Virtual file system built on top of VirtualDisk.
 *
 * OS Concepts Demonstrated:
 * - Directory structure: a flat directory table mapping filenames → metadata,
 *   similar to a simplified FAT directory or single-level Unix directory.
 * - Soft-deletion: deleting a file marks blocks as DELETED and flags the
 *   directory entry, but does NOT erase data. This mirrors real OS behaviour
 *   where `unlink()` simply removes the directory entry and frees blocks.
 * - Allocation-method selection at file-creation time.
 */
#pragma once

#include "VirtualDisk.h"
#include "FileMetadata.h"
#include "AllocationStrategy.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace da {

class FileSystem {
public:
    explicit FileSystem(VirtualDisk& disk);

    // File operations ───────────────────────────────────────────────────────
    bool                    createFile(const std::string& filename,
                                       const std::string& content);
    bool                    createFile(const std::string& filename,
                                       const std::string& content,
                                       AllocationType type);
    bool                    deleteFile(const std::string& filename);
    std::optional<std::string> readFile(const std::string& filename);

    // Directory queries ─────────────────────────────────────────────────────
    void listFiles()        const;
    void listDeletedFiles() const;
    void showFileDetails(const std::string& filename) const;

    // Allocation method ─────────────────────────────────────────────────────
    void           setAllocationMethod(AllocationType type);
    AllocationType getAllocationMethod() const;
    void           reset();

    // Accessors ─────────────────────────────────────────────────────────────
    const std::vector<FileMetadata>& getDirectory() const { return directory_; }
    std::vector<FileMetadata>&       getDirectory()       { return directory_; }
    VirtualDisk&                     getDisk()             { return disk_; }

    AllocationStrategy* getStrategy(AllocationType type);

    // Analysis ──────────────────────────────────────────────────────────────
    void showFragmentationAnalysis() const;

    // Persistence ───────────────────────────────────────────────────────────
    void save(std::ostream& os) const;
    void load(std::istream& is);

    // Lookup helpers ────────────────────────────────────────────────────────
    FileMetadata*       findFile(const std::string& filename,
                                 bool include_deleted = false);
    const FileMetadata* findFile(const std::string& filename,
                                 bool include_deleted = false) const;

private:
    VirtualDisk&       disk_;
    std::vector<FileMetadata> directory_;
    std::map<AllocationType, std::unique_ptr<AllocationStrategy>> strategies_;
    AllocationType     current_alloc_type_ = AllocationType::CONTIGUOUS;
    int32_t            next_file_id_       = 1;
};

} // namespace da

