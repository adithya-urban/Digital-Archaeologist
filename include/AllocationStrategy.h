/**
 * @file AllocationStrategy.h
 * @brief Strategy-pattern hierarchy for the three classic file-allocation methods.
 *
 * OS Concepts Demonstrated:
 * - Contiguous allocation: fast sequential reads, but suffers from external
 *   fragmentation — free space exists but is not contiguous.
 * - Linked allocation: eliminates external fragmentation by chaining blocks
 *   with next-pointers, at the cost of 4 bytes/block overhead and no random access.
 * - Indexed allocation: a dedicated index block maps logical offsets to physical
 *   blocks, enabling random access without external fragmentation, but wastes one
 *   full block for the index and caps max file size at INDEX_ENTRIES_PER_BLOCK × BLOCK_SIZE.
 * - Internal fragmentation: the last block of every file is typically only partially
 *   filled, wasting the remainder.
 */
#pragma once

#include "Common.h"
#include "VirtualDisk.h"
#include "FileMetadata.h"

#include <memory>
#include <vector>
#include <string>

namespace da {

// ─── Abstract Strategy ───────────────────────────────────────────────────────
class AllocationStrategy {
public:
    virtual ~AllocationStrategy() = default;

    virtual AllocationType getType()     const = 0;
    virtual std::string    getTypeName() const = 0;

    /// Allocate blocks on @p disk for @p data and fill @p metadata.
    virtual bool allocate(VirtualDisk& disk,
                          const std::vector<uint8_t>& data,
                          FileMetadata& metadata) = 0;

    /// Read the file described by @p metadata back from @p disk.
    virtual std::vector<uint8_t> readFile(const VirtualDisk& disk,
                                          const FileMetadata& metadata) const = 0;

    /// Mark the file's blocks as DELETED (data is *not* erased).
    virtual void deallocate(VirtualDisk& disk, FileMetadata& metadata) = 0;

    /// Human-readable fragmentation report for all active files using this method.
    virtual std::string getFragmentationInfo(
        const VirtualDisk& disk,
        const std::vector<FileMetadata>& directory) const = 0;
};

// ─── Contiguous ──────────────────────────────────────────────────────────────
class ContiguousAllocation : public AllocationStrategy {
public:
    AllocationType getType()     const override { return AllocationType::CONTIGUOUS; }
    std::string    getTypeName() const override { return "Contiguous"; }

    bool allocate(VirtualDisk& disk, const std::vector<uint8_t>& data,
                  FileMetadata& metadata) override;
    std::vector<uint8_t> readFile(const VirtualDisk& disk,
                                  const FileMetadata& metadata) const override;
    void deallocate(VirtualDisk& disk, FileMetadata& metadata) override;
    std::string getFragmentationInfo(const VirtualDisk& disk,
                                     const std::vector<FileMetadata>& dir) const override;
};

// ─── Linked ──────────────────────────────────────────────────────────────────
class LinkedAllocation : public AllocationStrategy {
public:
    AllocationType getType()     const override { return AllocationType::LINKED; }
    std::string    getTypeName() const override { return "Linked"; }

    bool allocate(VirtualDisk& disk, const std::vector<uint8_t>& data,
                  FileMetadata& metadata) override;
    std::vector<uint8_t> readFile(const VirtualDisk& disk,
                                  const FileMetadata& metadata) const override;
    void deallocate(VirtualDisk& disk, FileMetadata& metadata) override;
    std::string getFragmentationInfo(const VirtualDisk& disk,
                                     const std::vector<FileMetadata>& dir) const override;
};

// ─── Indexed ─────────────────────────────────────────────────────────────────
class IndexedAllocation : public AllocationStrategy {
public:
    AllocationType getType()     const override { return AllocationType::INDEXED; }
    std::string    getTypeName() const override { return "Indexed"; }

    bool allocate(VirtualDisk& disk, const std::vector<uint8_t>& data,
                  FileMetadata& metadata) override;
    std::vector<uint8_t> readFile(const VirtualDisk& disk,
                                  const FileMetadata& metadata) const override;
    void deallocate(VirtualDisk& disk, FileMetadata& metadata) override;
    std::string getFragmentationInfo(const VirtualDisk& disk,
                                     const std::vector<FileMetadata>& dir) const override;
};

// ─── Factory ─────────────────────────────────────────────────────────────────
std::unique_ptr<AllocationStrategy> createStrategy(AllocationType type);

} // namespace da

