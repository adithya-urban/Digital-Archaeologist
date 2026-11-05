/**
 * @file VirtualDisk.h
 * @brief Virtual disk backed by an in-memory block array and a binary image file.
 *
 * OS Concepts Demonstrated:
 * - Disk block management: reading, writing, and status-tracking of fixed-size sectors.
 * - Free-block search: scanning for available blocks (first-fit for contiguous,
 *   any-fit for linked/indexed).
 * - Persistence: the block array is saved to / loaded from a binary "disk image",
 *   similar to a raw disk dump or dd image.
 */
#pragma once

#include "Common.h"
#include <array>
#include <vector>
#include <string>

namespace da {

class VirtualDisk {
public:
    explicit VirtualDisk(const std::string& disk_file_path);

    // Lifecycle ─────────────────────────────────────────────────────────────
    bool initialize();                       // format (zero-fill) the disk
    bool load();                             // read disk image from file
    bool save() const;                       // write disk image to file
    bool imageExists() const;

    // Block access ──────────────────────────────────────────────────────────
    Block&       getBlock(int index);
    const Block& getBlock(int index) const;

    BlockStatus  getBlockStatus(int index) const;
    void         setBlockStatus(int index, BlockStatus status);
    void         setBlockOwner(int index, int32_t file_id);

    void         writeBlock(int index, const uint8_t* data, int size);
    void         readBlock(int index, uint8_t* buffer, int size) const;
    void         clearBlockData(int index);  // zero-fill data bytes

    // Statistics ────────────────────────────────────────────────────────────
    int  getFreeBlockCount()      const;
    int  getDeletedBlockCount()   const;
    int  getAllocatedBlockCount()  const;

    // Block search ──────────────────────────────────────────────────────────
    /// Return up to @p count blocks that are FREE *or* DELETED (available for reuse).
    std::vector<int> findAvailableBlocks(int count) const;
    /// Return @p count *contiguous* available blocks, or empty if impossible.
    std::vector<int> findContiguousAvailableBlocks(int count) const;

    // Display ───────────────────────────────────────────────────────────────
    void displayDiskMap()   const;
    void displayBlockContent(int index) const;
    void displayDiskStats() const;

    // Constants ─────────────────────────────────────────────────────────────
    static constexpr int blockSize()  { return BLOCK_SIZE; }
    static constexpr int numBlocks()  { return NUM_BLOCKS; }

private:
    std::array<Block, NUM_BLOCKS> blocks_;
    std::string                   disk_file_path_;
};

} // namespace da

