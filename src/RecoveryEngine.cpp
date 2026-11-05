/**
 * @file RecoveryEngine.cpp
 * @brief Implementation of the forensic recovery engine for deleted files.
 */

#include "RecoveryEngine.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace da {

RecoveryEngine::RecoveryEngine(VirtualDisk& disk, FileSystem& fs)
    : disk_(disk), fs_(fs) {}

std::vector<const FileMetadata*> RecoveryEngine::scanDeletedFiles() const {
    std::vector<const FileMetadata*> deleted_files;
    for (const auto& f : fs_.getDirectory()) {
        if (f.deleted) {
            deleted_files.push_back(&f);
        }
    }
    return deleted_files;
}

RecoveryResult RecoveryEngine::analyzeRecoverability(const std::string& filename) const {
    RecoveryResult res;
    const FileMetadata* meta = fs_.findFile(filename, true);

    if (!meta) {
        res.success = false;
        res.message = "File '" + filename + "' not found in directory metadata records.";
        return res;
    }

    if (!meta->deleted) {
        res.success = false;
        res.message = "File '" + filename + "' is active and not deleted.";
        return res;
    }

    res.total_blocks = meta->getTotalBlocks();
    int data_blk_count = static_cast<int>(meta->data_blocks.size());
    int intact_data_blocks = 0;
    int overwritten_data_blocks = 0;

    // Check index block if indexed
    bool index_intact = true;
    if (meta->alloc_type == AllocationType::INDEXED && meta->index_block >= 0) {
        BlockStatus ist = disk_.getBlockStatus(meta->index_block);
        int i_owner = disk_.getBlock(meta->index_block).owner_file_id;
        if (ist == BlockStatus::ALLOCATED && i_owner != meta->file_id) {
            index_intact = false;
            res.blocks_overwritten++;
        } else {
            res.blocks_recovered++;
        }
    }

    std::ostringstream reconstructed_data;
    int bytes_remaining = meta->file_size;

    for (int i = 0; i < data_blk_count; ++i) {
        int b_idx = meta->data_blocks[i];
        BlockStatus st = disk_.getBlockStatus(b_idx);
        int owner = disk_.getBlock(b_idx).owner_file_id;

        // A block is considered intact if it has not been reassigned to another file
        if (st == BlockStatus::ALLOCATED && owner != meta->file_id) {
            // Overwritten by another file!
            overwritten_data_blocks++;
            int chunk = std::min(meta->alloc_type == AllocationType::LINKED ? LINKED_DATA_SIZE : BLOCK_SIZE, bytes_remaining);
            reconstructed_data << "[<<< CORRUPTED/OVERWRITTEN SECTOR #" << b_idx << " >>>]";
            bytes_remaining -= chunk;
        } else {
            // Sector content is preserved in slack/deleted space
            intact_data_blocks++;
            int chunk = std::min(meta->alloc_type == AllocationType::LINKED ? LINKED_DATA_SIZE : BLOCK_SIZE, bytes_remaining);

            uint8_t buffer[BLOCK_SIZE];
            disk_.readBlock(b_idx, buffer, BLOCK_SIZE);

            if (chunk > 0) {
                for (int b = 0; b < chunk; ++b) {
                    reconstructed_data << static_cast<char>(buffer[b]);
                }
                bytes_remaining -= chunk;
            }
        }
    }

    res.blocks_recovered += intact_data_blocks;
    res.blocks_overwritten += overwritten_data_blocks;
    res.recovered_content = reconstructed_data.str();

    if (res.blocks_overwritten == 0) {
        res.success = true;
        res.partial = false;
        res.message = "100% Recoverable: All original disk sectors are completely untouched.";
    } else if (intact_data_blocks > 0) {
        res.success = true;
        res.partial = true;
        res.message = "Partially Recoverable: " + std::to_string(intact_data_blocks) + "/" +
                      std::to_string(data_blk_count) + " data sectors intact. " +
                      std::to_string(overwritten_data_blocks) + " sectors were overwritten.";
        if (!index_intact) {
            res.message += " (Note: Primary index block was also overwritten).";
        }
    } else {
        res.success = false;
        res.partial = false;
        res.message = "Unrecoverable: All original disk sectors have been overwritten by newer files.";
    }

    return res;
}

RecoveryResult RecoveryEngine::attemptRecovery(const std::string& filename) {
    RecoveryResult analysis = analyzeRecoverability(filename);
    if (!analysis.success && !analysis.partial) {
        return analysis;
    }

    FileMetadata* meta = fs_.findFile(filename, true);
    if (!meta) {
        analysis.success = false;
        analysis.message = "Metadata record disappeared.";
        return analysis;
    }

    // Reclaim intact blocks
    for (int b_idx : meta->data_blocks) {
        BlockStatus st = disk_.getBlockStatus(b_idx);
        int owner = disk_.getBlock(b_idx).owner_file_id;
        // Only re-mark if not currently belonging to an active other file
        if (st != BlockStatus::ALLOCATED || owner == meta->file_id) {
            disk_.setBlockStatus(b_idx, BlockStatus::ALLOCATED);
            disk_.setBlockOwner(b_idx, meta->file_id);
        }
    }

    if (meta->alloc_type == AllocationType::INDEXED && meta->index_block >= 0) {
        BlockStatus ist = disk_.getBlockStatus(meta->index_block);
        int owner = disk_.getBlock(meta->index_block).owner_file_id;
        if (ist != BlockStatus::ALLOCATED || owner == meta->file_id) {
            disk_.setBlockStatus(meta->index_block, BlockStatus::ALLOCATED);
            disk_.setBlockOwner(meta->index_block, meta->file_id);
        }
    }

    // Reactivate file metadata record
    meta->deleted = false;
    meta->deleted_at = 0;

    disk_.save();

    return analysis;
}

void RecoveryEngine::displayRecoveryReport(const RecoveryResult& result) const {
    std::cout << "\n" << ansi::BOLD << "╔══════════════════════════════════════════════════════════════════╗\n"
              << "║                   FORENSIC RECOVERY REPORT                       ║\n"
              << "╚══════════════════════════════════════════════════════════════════╝\n" << ansi::RESET;

    if (result.success && !result.partial) {
        std::cout << "Status: " << ansi::BOLD << ansi::GREEN << "[SUCCESS - FULL RECOVERY]" << ansi::RESET << "\n";
    } else if (result.partial) {
        std::cout << "Status: " << ansi::BOLD << ansi::YELLOW << "[PARTIAL RECOVERY - DATA CARVED WITH CORRUPTION]" << ansi::RESET << "\n";
    } else {
        std::cout << "Status: " << ansi::BOLD << ansi::RED << "[FAILED / UNRECOVERABLE]" << ansi::RESET << "\n";
    }

    std::cout << "Details             : " << result.message << "\n";
    std::cout << "Sectors Recovered   : " << result.blocks_recovered << " / " << result.total_blocks << "\n";
    std::cout << "Sectors Overwritten : " << result.blocks_overwritten << " / " << result.total_blocks << "\n";

    if (!result.recovered_content.empty()) {
        std::cout << "\n" << ansi::BOLD << "Carved / Recovered Payload:" << ansi::RESET << "\n";
        std::cout << "┌" << std::string(60, '-') << "┐\n";
        std::cout << result.recovered_content << "\n";
        std::cout << "└" << std::string(60, '-') << "┘\n";
    }
    std::cout << "\n";
}

void RecoveryEngine::displayForensicScan() const {
    auto deleted = scanDeletedFiles();

    std::cout << "\n" << ansi::BOLD << ansi::CYAN
              << "╔══════════════════════════════════════════════════════════════════════════════════════════════╗\n"
              << "║                       FORENSIC SCAN: DELETED FILE CARVING & RECOVERY                         ║\n"
              << "╚══════════════════════════════════════════════════════════════════════════════════════════════╝\n"
              << ansi::RESET;

    if (deleted.empty()) {
        std::cout << " (No soft-deleted files detected on the disk.)\n\n";
        return;
    }

    std::cout << std::left
              << std::setw(5)  << "ID"
              << std::setw(18) << "Filename"
              << std::setw(10) << "Size(B)"
              << std::setw(12) << "Alloc Type"
              << std::setw(14) << "Intact Blks"
              << std::setw(14) << "Clashed Blks"
              << "Estimated Feasibility\n";
    std::cout << std::string(96, '-') << "\n";

    for (const auto* f : deleted) {
        RecoveryResult res = analyzeRecoverability(f->filename);

        std::string status_str;
        if (res.success && !res.partial) {
            status_str = ansi::BOLD + std::string(ansi::GREEN) + "100% Intact (Clean)" + ansi::RESET;
        } else if (res.partial) {
            status_str = ansi::BOLD + std::string(ansi::YELLOW) + "Partially Damaged" + ansi::RESET;
        } else {
            status_str = ansi::BOLD + std::string(ansi::RED) + "Lost (Overwritten)" + ansi::RESET;
        }

        std::cout << std::left
                  << std::setw(5)  << f->file_id
                  << std::setw(18) << f->filename
                  << std::setw(10) << f->file_size
                  << std::setw(12) << allocationTypeName(f->alloc_type)
                  << std::setw(14) << (std::to_string(res.blocks_recovered) + "/" + std::to_string(res.total_blocks))
                  << std::setw(14) << (std::to_string(res.blocks_overwritten) + "/" + std::to_string(res.total_blocks))
                  << status_str << "\n";
    }
    std::cout << std::right << std::dec << "\n";
}

} // namespace da
