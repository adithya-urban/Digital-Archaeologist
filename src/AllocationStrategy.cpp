/**
 * @file AllocationStrategy.cpp
 * @brief Implementations of Contiguous, Linked, and Indexed allocation strategies.
 */

#include "AllocationStrategy.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

namespace da {

// ============================================================================
// Contiguous Allocation
// ============================================================================

bool ContiguousAllocation::allocate(VirtualDisk& disk,
                                    const std::vector<uint8_t>& data,
                                    FileMetadata& metadata) {
    int data_size = static_cast<int>(data.size());
    int blocks_needed = (data_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocks_needed == 0) blocks_needed = 1; // At least one block allocated

    std::vector<int> blocks = disk.findContiguousAvailableBlocks(blocks_needed);
    if (blocks.empty()) {
        return false; // Failed: External fragmentation or lack of space
    }

    metadata.alloc_type = AllocationType::CONTIGUOUS;
    metadata.file_size = data_size;
    metadata.start_block = blocks[0];
    metadata.index_block = -1;
    metadata.data_blocks = blocks;

    for (int i = 0; i < blocks_needed; ++i) {
        int b_idx = blocks[i];
        disk.setBlockStatus(b_idx, BlockStatus::ALLOCATED);
        disk.setBlockOwner(b_idx, metadata.file_id);

        int offset = i * BLOCK_SIZE;
        int chunk = std::min(BLOCK_SIZE, data_size - offset);
        if (chunk > 0) {
            disk.writeBlock(b_idx, data.data() + offset, chunk);
        } else {
            disk.clearBlockData(b_idx);
        }
    }

    return true;
}

std::vector<uint8_t> ContiguousAllocation::readFile(const VirtualDisk& disk,
                                                    const FileMetadata& metadata) const {
    std::vector<uint8_t> result;
    result.reserve(metadata.file_size);

    int remaining = metadata.file_size;
    for (int b_idx : metadata.data_blocks) {
        if (remaining <= 0) break;
        int to_read = std::min(BLOCK_SIZE, remaining);
        uint8_t buf[BLOCK_SIZE];
        disk.readBlock(b_idx, buf, to_read);
        result.insert(result.end(), buf, buf + to_read);
        remaining -= to_read;
    }

    return result;
}

void ContiguousAllocation::deallocate(VirtualDisk& disk, FileMetadata& metadata) {
    for (int b_idx : metadata.data_blocks) {
        disk.setBlockStatus(b_idx, BlockStatus::DELETED);
        // Note: Owner ID and block data are intentionally preserved for recovery!
    }
    metadata.deleted = true;
}

std::string ContiguousAllocation::getFragmentationInfo(
    const VirtualDisk& disk,
    const std::vector<FileMetadata>& dir) const {
    std::ostringstream ss;

    // Internal fragmentation
    int total_internal_waste = 0;
    int file_count = 0;
    for (const auto& meta : dir) {
        if (!meta.deleted && meta.alloc_type == AllocationType::CONTIGUOUS) {
            int allocated_bytes = static_cast<int>(meta.data_blocks.size()) * BLOCK_SIZE;
            int waste = allocated_bytes - meta.file_size;
            total_internal_waste += waste;
            file_count++;
        }
    }

    // External fragmentation analysis:
    // Look at runs of available (FREE or DELETED) blocks.
    int current_free_run = 0;
    int max_free_run = 0;
    int total_available = 0;
    int free_segments = 0;

    for (int i = 0; i < NUM_BLOCKS; ++i) {
        BlockStatus st = disk.getBlockStatus(i);
        if (st == BlockStatus::FREE || st == BlockStatus::DELETED) {
            total_available++;
            if (current_free_run == 0) free_segments++;
            current_free_run++;
            if (current_free_run > max_free_run) {
                max_free_run = current_free_run;
            }
        } else {
            current_free_run = 0;
        }
    }

    ss << ansi::BOLD << "--- Contiguous Allocation Fragmentation Analysis ---" << ansi::RESET << "\n";
    ss << "Active Contiguous Files : " << file_count << "\n";
    ss << "Internal Fragmentation  : " << total_internal_waste << " bytes wasted (slack space in last blocks)\n";
    ss << "Total Available Blocks  : " << total_available << " (" << (total_available * BLOCK_SIZE) << " bytes)\n";
    ss << "Largest Contiguous Span : " << max_free_run << " blocks (" << (max_free_run * BLOCK_SIZE) << " bytes)\n";
    ss << "Available Hole Segments : " << free_segments << "\n";

    if (total_available > max_free_run) {
        int fragmented_blocks = total_available - max_free_run;
        ss << ansi::YELLOW << "External Fragmentation Detected: "
           << fragmented_blocks << " blocks (" << (fragmented_blocks * BLOCK_SIZE)
           << " bytes) cannot be utilized by a file requiring " << total_available << " blocks contiguous space."
           << ansi::RESET << "\n";
    } else {
        ss << ansi::GREEN << "No severe external fragmentation (free blocks form a single span)." << ansi::RESET << "\n";
    }

    return ss.str();
}

// ============================================================================
// Linked Allocation
// ============================================================================

bool LinkedAllocation::allocate(VirtualDisk& disk,
                                const std::vector<uint8_t>& data,
                                FileMetadata& metadata) {
    int data_size = static_cast<int>(data.size());
    int blocks_needed = (data_size + LINKED_DATA_SIZE - 1) / LINKED_DATA_SIZE;
    if (blocks_needed == 0) blocks_needed = 1;

    std::vector<int> blocks = disk.findAvailableBlocks(blocks_needed);
    if (static_cast<int>(blocks.size()) < blocks_needed) {
        return false;
    }

    metadata.alloc_type = AllocationType::LINKED;
    metadata.file_size = data_size;
    metadata.start_block = blocks[0];
    metadata.index_block = -1;
    metadata.data_blocks = blocks;

    for (int i = 0; i < blocks_needed; ++i) {
        int b_idx = blocks[i];
        disk.setBlockStatus(b_idx, BlockStatus::ALLOCATED);
        disk.setBlockOwner(b_idx, metadata.file_id);

        int offset = i * LINKED_DATA_SIZE;
        int chunk = std::min(LINKED_DATA_SIZE, data_size - offset);

        uint8_t block_buf[BLOCK_SIZE];
        std::memset(block_buf, 0, BLOCK_SIZE);
        if (chunk > 0) {
            std::memcpy(block_buf, data.data() + offset, chunk);
        }

        // Write pointer to next block at the end of block (last 4 bytes)
        int32_t next_ptr = (i + 1 < blocks_needed) ? blocks[i + 1] : -1;
        std::memcpy(block_buf + LINKED_DATA_SIZE, &next_ptr, sizeof(next_ptr));

        disk.writeBlock(b_idx, block_buf, BLOCK_SIZE);
    }

    return true;
}

std::vector<uint8_t> LinkedAllocation::readFile(const VirtualDisk& disk,
                                                const FileMetadata& metadata) const {
    std::vector<uint8_t> result;
    result.reserve(metadata.file_size);

    int current_blk = metadata.start_block;
    int remaining = metadata.file_size;

    while (current_blk != -1 && remaining > 0) {
        if (current_blk < 0 || current_blk >= NUM_BLOCKS) break;

        uint8_t block_buf[BLOCK_SIZE];
        disk.readBlock(current_blk, block_buf, BLOCK_SIZE);

        int to_read = std::min(LINKED_DATA_SIZE, remaining);
        result.insert(result.end(), block_buf, block_buf + to_read);
        remaining -= to_read;

        // Read next pointer
        int32_t next_blk = -1;
        std::memcpy(&next_blk, block_buf + LINKED_DATA_SIZE, sizeof(next_blk));
        current_blk = next_blk;
    }

    return result;
}

void LinkedAllocation::deallocate(VirtualDisk& disk, FileMetadata& metadata) {
    for (int b_idx : metadata.data_blocks) {
        disk.setBlockStatus(b_idx, BlockStatus::DELETED);
    }
    metadata.deleted = true;
}

std::string LinkedAllocation::getFragmentationInfo(
    const VirtualDisk& disk,
    const std::vector<FileMetadata>& dir) const {
    (void)disk;
    std::ostringstream ss;

    int total_internal_waste = 0;
    int total_pointer_overhead = 0;
    int file_count = 0;

    for (const auto& meta : dir) {
        if (!meta.deleted && meta.alloc_type == AllocationType::LINKED) {
            int num_blks = static_cast<int>(meta.data_blocks.size());
            total_pointer_overhead += num_blks * LINKED_PTR_SIZE;

            int capacity = num_blks * LINKED_DATA_SIZE;
            total_internal_waste += (capacity - meta.file_size);
            file_count++;
        }
    }

    ss << ansi::BOLD << "--- Linked Allocation Fragmentation Analysis ---" << ansi::RESET << "\n";
    ss << "Active Linked Files     : " << file_count << "\n";
    ss << "Internal Fragmentation  : " << total_internal_waste << " bytes wasted in final block payloads\n";
    ss << "Pointer Overhead        : " << total_pointer_overhead << " bytes reserved for block links (4B/block)\n";
    ss << ansi::GREEN << "External Fragmentation  : None! (Blocks are non-contiguous linked lists)\n" << ansi::RESET;
    ss << "Trade-off               : Sequential access only; direct random seeks require traversing chains.\n";

    return ss.str();
}

// ============================================================================
// Indexed Allocation
// ============================================================================

bool IndexedAllocation::allocate(VirtualDisk& disk,
                                 const std::vector<uint8_t>& data,
                                 FileMetadata& metadata) {
    int data_size = static_cast<int>(data.size());
    int data_blocks_needed = (data_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (data_blocks_needed == 0) data_blocks_needed = 1;

    // Check maximum file size supported by single index block
    if (data_blocks_needed > INDEX_ENTRIES_PER_BLOCK) {
        return false; // File too large for single index block
    }

    // 1 index block + N data blocks
    int total_needed = 1 + data_blocks_needed;
    std::vector<int> blocks = disk.findAvailableBlocks(total_needed);
    if (static_cast<int>(blocks.size()) < total_needed) {
        return false;
    }

    int index_blk = blocks[0];
    std::vector<int> data_blks(blocks.begin() + 1, blocks.end());

    metadata.alloc_type = AllocationType::INDEXED;
    metadata.file_size = data_size;
    metadata.index_block = index_blk;
    metadata.start_block = data_blks.empty() ? -1 : data_blks[0];
    metadata.data_blocks = data_blks;

    // Mark and set up index block
    disk.setBlockStatus(index_blk, BlockStatus::ALLOCATED);
    disk.setBlockOwner(index_blk, metadata.file_id);

    // Build index table
    int32_t index_table[INDEX_ENTRIES_PER_BLOCK];
    for (int i = 0; i < INDEX_ENTRIES_PER_BLOCK; ++i) {
        if (i < data_blocks_needed) {
            index_table[i] = data_blks[i];
        } else {
            index_table[i] = -1; // Unused entry
        }
    }
    disk.writeBlock(index_blk, reinterpret_cast<const uint8_t*>(index_table), BLOCK_SIZE);

    // Mark and write data blocks
    for (int i = 0; i < data_blocks_needed; ++i) {
        int b_idx = data_blks[i];
        disk.setBlockStatus(b_idx, BlockStatus::ALLOCATED);
        disk.setBlockOwner(b_idx, metadata.file_id);

        int offset = i * BLOCK_SIZE;
        int chunk = std::min(BLOCK_SIZE, data_size - offset);
        if (chunk > 0) {
            disk.writeBlock(b_idx, data.data() + offset, chunk);
        } else {
            disk.clearBlockData(b_idx);
        }
    }

    return true;
}

std::vector<uint8_t> IndexedAllocation::readFile(const VirtualDisk& disk,
                                                 const FileMetadata& metadata) const {
    std::vector<uint8_t> result;
    result.reserve(metadata.file_size);

    if (metadata.index_block < 0 || metadata.index_block >= NUM_BLOCKS) {
        return result;
    }

    // Read index block
    int32_t index_table[INDEX_ENTRIES_PER_BLOCK];
    disk.readBlock(metadata.index_block, reinterpret_cast<uint8_t*>(index_table), BLOCK_SIZE);

    int remaining = metadata.file_size;
    for (int i = 0; i < INDEX_ENTRIES_PER_BLOCK; ++i) {
        if (remaining <= 0) break;
        int b_idx = index_table[i];
        if (b_idx < 0 || b_idx >= NUM_BLOCKS) break;

        int to_read = std::min(BLOCK_SIZE, remaining);
        uint8_t buf[BLOCK_SIZE];
        disk.readBlock(b_idx, buf, to_read);
        result.insert(result.end(), buf, buf + to_read);
        remaining -= to_read;
    }

    return result;
}

void IndexedAllocation::deallocate(VirtualDisk& disk, FileMetadata& metadata) {
    if (metadata.index_block >= 0 && metadata.index_block < NUM_BLOCKS) {
        disk.setBlockStatus(metadata.index_block, BlockStatus::DELETED);
    }
    for (int b_idx : metadata.data_blocks) {
        disk.setBlockStatus(b_idx, BlockStatus::DELETED);
    }
    metadata.deleted = true;
}

std::string IndexedAllocation::getFragmentationInfo(
    const VirtualDisk& disk,
    const std::vector<FileMetadata>& dir) const {
    (void)disk;
    std::ostringstream ss;

    int total_internal_waste = 0;
    int total_index_block_overhead = 0;
    int file_count = 0;

    for (const auto& meta : dir) {
        if (!meta.deleted && meta.alloc_type == AllocationType::INDEXED) {
            int allocated_data = static_cast<int>(meta.data_blocks.size()) * BLOCK_SIZE;
            total_internal_waste += (allocated_data - meta.file_size);
            total_index_block_overhead += BLOCK_SIZE; // 1 full block for index
            file_count++;
        }
    }

    ss << ansi::BOLD << "--- Indexed Allocation Fragmentation Analysis ---" << ansi::RESET << "\n";
    ss << "Active Indexed Files    : " << file_count << "\n";
    ss << "Internal Fragmentation  : " << total_internal_waste << " bytes wasted in final data blocks\n";
    ss << "Index Block Overhead    : " << total_index_block_overhead << " bytes (1 block = 64B per file)\n";
    ss << ansi::GREEN << "External Fragmentation  : None! (Blocks can be placed in any non-contiguous slot)\n" << ansi::RESET;
    ss << "Trade-off               : Direct random access enabled, but small files suffer significant overhead.\n";

    return ss.str();
}

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<AllocationStrategy> createStrategy(AllocationType type) {
    switch (type) {
        case AllocationType::CONTIGUOUS:
            return std::make_unique<ContiguousAllocation>();
        case AllocationType::LINKED:
            return std::make_unique<LinkedAllocation>();
        case AllocationType::INDEXED:
            return std::make_unique<IndexedAllocation>();
    }
    return std::make_unique<ContiguousAllocation>();
}

} // namespace da
