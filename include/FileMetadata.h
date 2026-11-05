/**
 * @file FileMetadata.h
 * @brief File metadata / directory-entry structure.
 *
 * OS Concepts Demonstrated:
 * - Directory entries store the mapping from human-readable filenames to
 *   on-disk block locations, much like an inode or MFT record.
 * - Soft-deletion: the directory entry is flagged as deleted but is NOT
 *   removed, preserving metadata for potential recovery (analogous to
 *   NTFS MFT entries or ext4 inode tables after unlink).
 */
#pragma once

#include "Common.h"
#include <vector>
#include <string>
#include <ctime>
#include <iostream>

namespace da {

struct FileMetadata {
    int32_t             file_id     = -1;
    std::string         filename;
    int32_t             file_size   = 0;               // actual payload bytes
    AllocationType      alloc_type  = AllocationType::CONTIGUOUS;

    // Block mapping — retained even after deletion for recovery purposes.
    std::vector<int32_t> data_blocks;                  // ordered list of data-block indices
    int32_t             index_block = -1;              // index-block index (Indexed alloc)
    int32_t             start_block = -1;              // first block (Contiguous / Linked)

    bool                deleted     = false;
    std::time_t         created_at  = 0;
    std::time_t         deleted_at  = 0;

    // Convenience ───────────────────────────────────────────────────────────
    int  getBlockCount()   const { return static_cast<int>(data_blocks.size()); }
    int  getTotalBlocks()  const {
        // Indexed allocation uses an extra index block.
        return getBlockCount() + (alloc_type == AllocationType::INDEXED ? 1 : 0);
    }

    // Binary serialisation ──────────────────────────────────────────────────
    void                serialize(std::ostream& os) const;
    static FileMetadata deserialize(std::istream& is);
};

} // namespace da

