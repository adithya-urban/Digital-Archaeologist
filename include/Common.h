/**
 * @file Common.h
 * @brief Shared constants, enums, and utility types for the Digital Archaeologist simulator.
 *
 * OS Concepts Demonstrated:
 * - Disk geometry: block (sector) size, total block count, disk capacity.
 * - Block status tracking: free-list management at the block level.
 * - Allocation method taxonomy: contiguous, linked, and indexed allocation.
 */
#pragma once

#include <cstdint>
#include <array>
#include <string>

namespace da {

// ─── Disk Geometry ───────────────────────────────────────────────────────────
constexpr int BLOCK_SIZE  = 64;                          // bytes per block
constexpr int NUM_BLOCKS  = 256;                         // total blocks on disk
constexpr int DISK_SIZE   = BLOCK_SIZE * NUM_BLOCKS;     // 16,384 bytes

// ─── File System Limits ──────────────────────────────────────────────────────
constexpr int MAX_FILENAME_LEN = 31;                     // +1 for null terminator

// ─── Linked Allocation Layout ────────────────────────────────────────────────
// In linked allocation each block reserves its last 4 bytes as a "next-block"
// pointer, exactly mirroring how real linked-list allocation works on disk.
constexpr int LINKED_PTR_SIZE  = static_cast<int>(sizeof(int32_t));   // 4 bytes
constexpr int LINKED_DATA_SIZE = BLOCK_SIZE - LINKED_PTR_SIZE;        // 60 bytes usable

// ─── Indexed Allocation Layout ───────────────────────────────────────────────
// An index block stores int32_t block numbers; capacity = BLOCK_SIZE / 4 = 16.
constexpr int INDEX_ENTRIES_PER_BLOCK = BLOCK_SIZE / static_cast<int>(sizeof(int32_t));

// ─── Persistence ─────────────────────────────────────────────────────────────
constexpr uint32_t MAGIC   = 0x44414652;   // "DAFR" in little-endian
constexpr uint32_t VERSION = 1;

// ─── Block Status (free-list concept) ────────────────────────────────────────
enum class BlockStatus : uint8_t {
    FREE      = 0,   // Block has never been used, or has been fully reclaimed
    ALLOCATED = 1,   // Block is in active use by a file
    DELETED   = 2    // Block's owning file was deleted; data lingers (recoverable)
};

// ─── Allocation Method ───────────────────────────────────────────────────────
enum class AllocationType : uint8_t {
    CONTIGUOUS = 0,  // File occupies consecutive blocks
    LINKED     = 1,  // Each block stores a next-pointer (linked list on disk)
    INDEXED    = 2   // A dedicated index block maps logical→physical blocks
};

// ─── Single Disk Block ──────────────────────────────────────────────────────
struct Block {
    BlockStatus              status        = BlockStatus::FREE;
    std::array<uint8_t, BLOCK_SIZE> data   = {};
    int32_t                  owner_file_id = -1;          // file that owns this block
};

// ─── Helpers ─────────────────────────────────────────────────────────────────
inline std::string allocationTypeName(AllocationType t) {
    switch (t) {
        case AllocationType::CONTIGUOUS: return "Contiguous";
        case AllocationType::LINKED:     return "Linked";
        case AllocationType::INDEXED:    return "Indexed";
    }
    return "Unknown";
}

inline std::string blockStatusName(BlockStatus s) {
    switch (s) {
        case BlockStatus::FREE:      return "Free";
        case BlockStatus::ALLOCATED: return "Allocated";
        case BlockStatus::DELETED:   return "Deleted";
    }
    return "Unknown";
}

// ─── ANSI Colour Helpers ────────────────────────────────────────────────────
namespace ansi {
    inline const char* RESET   = "\033[0m";
    inline const char* BOLD    = "\033[1m";
    inline const char* RED     = "\033[31m";
    inline const char* GREEN   = "\033[32m";
    inline const char* YELLOW  = "\033[33m";
    inline const char* BLUE    = "\033[34m";
    inline const char* MAGENTA = "\033[35m";
    inline const char* CYAN    = "\033[36m";
    inline const char* GRAY    = "\033[90m";
    inline const char* WHITE   = "\033[97m";
    inline const char* BG_RED  = "\033[41m";
    inline const char* BG_GRN  = "\033[42m";
    inline const char* BG_YEL  = "\033[43m";
    inline const char* BG_CYN  = "\033[46m";
}

} // namespace da

