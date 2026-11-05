/**
 * @file FileSystem.cpp
 * @brief Implementation of FileSystem directory management and file operations.
 */

#include "FileSystem.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <algorithm>

namespace da {

FileSystem::FileSystem(VirtualDisk& disk)
    : disk_(disk) {
    strategies_[AllocationType::CONTIGUOUS] = createStrategy(AllocationType::CONTIGUOUS);
    strategies_[AllocationType::LINKED]     = createStrategy(AllocationType::LINKED);
    strategies_[AllocationType::INDEXED]    = createStrategy(AllocationType::INDEXED);
}

void FileSystem::reset() {
    directory_.clear();
    next_file_id_ = 1;
    current_alloc_type_ = AllocationType::CONTIGUOUS;
}

void FileSystem::setAllocationMethod(AllocationType type) {
    current_alloc_type_ = type;
}

AllocationType FileSystem::getAllocationMethod() const {
    return current_alloc_type_;
}

AllocationStrategy* FileSystem::getStrategy(AllocationType type) {
    auto it = strategies_.find(type);
    if (it != strategies_.end()) {
        return it->second.get();
    }
    return nullptr;
}

FileMetadata* FileSystem::findFile(const std::string& filename, bool include_deleted) {
    for (auto& file : directory_) {
        if (file.filename == filename) {
            if (!file.deleted || include_deleted) {
                return &file;
            }
        }
    }
    return nullptr;
}

const FileMetadata* FileSystem::findFile(const std::string& filename, bool include_deleted) const {
    for (const auto& file : directory_) {
        if (file.filename == filename) {
            if (!file.deleted || include_deleted) {
                return &file;
            }
        }
    }
    return nullptr;
}

bool FileSystem::createFile(const std::string& filename, const std::string& content) {
    return createFile(filename, content, current_alloc_type_);
}

bool FileSystem::createFile(const std::string& filename,
                            const std::string& content,
                            AllocationType type) {
    if (filename.empty()) {
        std::cout << ansi::RED << "[!] Error: Filename cannot be empty." << ansi::RESET << "\n";
        return false;
    }
    if (filename.length() > MAX_FILENAME_LEN) {
        std::cout << ansi::RED << "[!] Error: Filename exceeds maximum length of " << MAX_FILENAME_LEN << " chars." << ansi::RESET << "\n";
        return false;
    }
    if (findFile(filename, false) != nullptr) {
        std::cout << ansi::RED << "[!] Error: An active file with name '" << filename << "' already exists." << ansi::RESET << "\n";
        return false;
    }

    AllocationStrategy* strat = getStrategy(type);
    if (!strat) {
        std::cout << ansi::RED << "[!] Error: Unsupported allocation strategy." << ansi::RESET << "\n";
        return false;
    }

    FileMetadata meta;
    meta.file_id = next_file_id_++;
    meta.filename = filename;
    meta.alloc_type = type;
    meta.created_at = std::time(nullptr);
    meta.deleted = false;

    std::vector<uint8_t> data(content.begin(), content.end());
    bool ok = strat->allocate(disk_, data, meta);
    if (!ok) {
        std::cout << ansi::RED << "[!] Allocation failed: insufficient contiguous or total disk blocks." << ansi::RESET << "\n";
        return false;
    }

    directory_.push_back(meta);
    disk_.save();
    return true;
}

bool FileSystem::deleteFile(const std::string& filename) {
    FileMetadata* meta = findFile(filename, false);
    if (!meta) {
        std::cout << ansi::RED << "[!] Error: File '" << filename << "' not found or already deleted." << ansi::RESET << "\n";
        return false;
    }

    AllocationStrategy* strat = getStrategy(meta->alloc_type);
    if (strat) {
        strat->deallocate(disk_, *meta);
    }
    meta->deleted = true;
    meta->deleted_at = std::time(nullptr);

    disk_.save();
    return true;
}

std::optional<std::string> FileSystem::readFile(const std::string& filename) {
    const FileMetadata* meta = findFile(filename, false);
    if (!meta) {
        return std::nullopt;
    }

    auto strat = strategies_.find(meta->alloc_type);
    if (strat == strategies_.end()) {
        return std::nullopt;
    }

    std::vector<uint8_t> bytes = strat->second->readFile(disk_, *meta);
    return std::string(bytes.begin(), bytes.end());
}

static std::string formatTime(std::time_t t) {
    if (t == 0) return "N/A";
    char buf[32];
    std::tm* tm_info = std::localtime(&t);
    if (tm_info) {
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
        return std::string(buf);
    }
    return "Unknown";
}

void FileSystem::listFiles() const {
    std::cout << "\n" << ansi::BOLD << ansi::CYAN
              << "╔══════════════════════════════════════════════════════════════════════════════════════════╗\n"
              << "║                                   ACTIVE DIRECTORY LISTING                               ║\n"
              << "╚══════════════════════════════════════════════════════════════════════════════════════════╝\n"
              << ansi::RESET;

    int active_count = 0;
    for (const auto& f : directory_) {
        if (!f.deleted) active_count++;
    }

    if (active_count == 0) {
        std::cout << " (Directory is empty. No active files found.)\n\n";
        return;
    }

    std::cout << std::left
              << std::setw(5)  << "ID"
              << std::setw(18) << "Filename"
              << std::setw(10) << "Size(B)"
              << std::setw(12) << "Allocation"
              << std::setw(10) << "Blocks"
              << std::setw(22) << "Created"
              << "Block List\n";
    std::cout << std::string(90, '-') << "\n";

    for (const auto& f : directory_) {
        if (f.deleted) continue;

        std::string blk_list = "[";
        if (f.alloc_type == AllocationType::INDEXED && f.index_block != -1) {
            blk_list += "Idx:" + std::to_string(f.index_block) + " | ";
        }
        for (size_t i = 0; i < f.data_blocks.size(); ++i) {
            blk_list += std::to_string(f.data_blocks[i]);
            if (i + 1 < f.data_blocks.size()) blk_list += ",";
            if (i >= 5 && f.data_blocks.size() > 6) {
                blk_list += "...";
                break;
            }
        }
        blk_list += "]";

        std::cout << std::left
                  << std::setw(5)  << f.file_id
                  << std::setw(18) << f.filename
                  << std::setw(10) << f.file_size
                  << std::setw(12) << allocationTypeName(f.alloc_type)
                  << std::setw(10) << f.getTotalBlocks()
                  << std::setw(22) << formatTime(f.created_at)
                  << blk_list << "\n";
    }
    std::cout << std::right << std::dec << "\n";
}

void FileSystem::listDeletedFiles() const {
    std::cout << "\n" << ansi::BOLD << ansi::YELLOW
              << "╔══════════════════════════════════════════════════════════════════════════════════════════╗\n"
              << "║                         DELETED FILES (RETAINED DIRECTORY RECORDS)                       ║\n"
              << "╚══════════════════════════════════════════════════════════════════════════════════════════╝\n"
              << ansi::RESET;

    int deleted_count = 0;
    for (const auto& f : directory_) {
        if (f.deleted) deleted_count++;
    }

    if (deleted_count == 0) {
        std::cout << " (No deleted files found in metadata journal.)\n\n";
        return;
    }

    std::cout << std::left
              << std::setw(5)  << "ID"
              << std::setw(18) << "Filename"
              << std::setw(10) << "Size(B)"
              << std::setw(12) << "Allocation"
              << std::setw(22) << "Deleted At"
              << "Original Blocks\n";
    std::cout << std::string(90, '-') << "\n";

    for (const auto& f : directory_) {
        if (!f.deleted) continue;

        std::string blk_list = "[";
        if (f.alloc_type == AllocationType::INDEXED && f.index_block != -1) {
            blk_list += "Idx:" + std::to_string(f.index_block) + " | ";
        }
        for (size_t i = 0; i < f.data_blocks.size(); ++i) {
            blk_list += std::to_string(f.data_blocks[i]);
            if (i + 1 < f.data_blocks.size()) blk_list += ",";
        }
        blk_list += "]";

        std::cout << std::left
                  << std::setw(5)  << f.file_id
                  << std::setw(18) << f.filename
                  << std::setw(10) << f.file_size
                  << std::setw(12) << allocationTypeName(f.alloc_type)
                  << std::setw(22) << formatTime(f.deleted_at)
                  << blk_list << "\n";
    }
    std::cout << std::right << std::dec << "\n";
}

void FileSystem::showFileDetails(const std::string& filename) const {
    const FileMetadata* meta = findFile(filename, true);
    if (!meta) {
        std::cout << ansi::RED << "[!] File '" << filename << "' not found in directory." << ansi::RESET << "\n";
        return;
    }

    std::cout << "\n" << ansi::BOLD << "=== File Details: " << meta->filename << " ===" << ansi::RESET << "\n";
    std::cout << "  File ID          : " << meta->file_id << "\n";
    std::cout << "  Status           : " << (meta->deleted ? (ansi::YELLOW + std::string("DELETED (Soft-deleted)") + ansi::RESET) : (ansi::GREEN + std::string("ACTIVE") + ansi::RESET)) << "\n";
    std::cout << "  Size             : " << meta->file_size << " bytes\n";
    std::cout << "  Allocation Type  : " << allocationTypeName(meta->alloc_type) << "\n";
    std::cout << "  Created Timestamp: " << formatTime(meta->created_at) << "\n";
    if (meta->deleted) {
        std::cout << "  Deleted Timestamp: " << formatTime(meta->deleted_at) << "\n";
    }

    if (meta->alloc_type == AllocationType::INDEXED) {
        std::cout << "  Index Block      : Sector #" << meta->index_block << "\n";
    } else if (meta->alloc_type == AllocationType::CONTIGUOUS || meta->alloc_type == AllocationType::LINKED) {
        std::cout << "  Start Block      : Sector #" << meta->start_block << "\n";
    }

    std::cout << "  Assigned Sectors : [";
    for (size_t i = 0; i < meta->data_blocks.size(); ++i) {
        std::cout << meta->data_blocks[i];
        if (i + 1 < meta->data_blocks.size()) std::cout << ", ";
    }
    std::cout << "] (Total: " << meta->getTotalBlocks() << " blocks)\n";

    // Block status inspection
    std::cout << "  Block Health Status:\n";
    int overwritten_count = 0;
    for (int b_idx : meta->data_blocks) {
        BlockStatus st = disk_.getBlockStatus(b_idx);
        std::cout << "    - Sector #" << std::setw(3) << b_idx << ": " << blockStatusName(st);
        int current_owner = disk_.getBlock(b_idx).owner_file_id;
        if (st == BlockStatus::ALLOCATED && current_owner != meta->file_id) {
            std::cout << ansi::RED << " (OVERWRITTEN by File #" << current_owner << "!)" << ansi::RESET;
            overwritten_count++;
        } else if (st == BlockStatus::DELETED) {
            std::cout << ansi::YELLOW << " (Untouched residual data preserved)" << ansi::RESET;
        } else if (st == BlockStatus::ALLOCATED) {
            std::cout << ansi::GREEN << " (Intact active block)" << ansi::RESET;
        }
        std::cout << "\n";
    }

    if (meta->alloc_type == AllocationType::INDEXED && meta->index_block >= 0) {
        BlockStatus st = disk_.getBlockStatus(meta->index_block);
        std::cout << "    - Index Sector #" << meta->index_block << ": " << blockStatusName(st);
        int current_owner = disk_.getBlock(meta->index_block).owner_file_id;
        if (st == BlockStatus::ALLOCATED && current_owner != meta->file_id) {
            std::cout << ansi::RED << " (Index Overwritten by File #" << current_owner << "!)" << ansi::RESET;
        }
        std::cout << "\n";
    }

    std::cout << "\n";
}

void FileSystem::showFragmentationAnalysis() const {
    std::cout << "\n" << ansi::BOLD << ansi::CYAN
              << "╔══════════════════════════════════════════════════════════════════════════════════════════╗\n"
              << "║                       COMPREHENSIVE FRAGMENTATION & EFFICIENCY REPORT                    ║\n"
              << "╚══════════════════════════════════════════════════════════════════════════════════════════╝\n"
              << ansi::RESET;

    for (const auto& pair : strategies_) {
        std::cout << pair.second->getFragmentationInfo(disk_, directory_) << "\n";
    }

    // Overall internal fragmentation summary
    int total_internal_slack = 0;
    int total_payload_bytes = 0;
    int total_allocated_disk_bytes = 0;

    for (const auto& f : directory_) {
        if (!f.deleted) {
            total_payload_bytes += f.file_size;
            total_allocated_disk_bytes += f.getTotalBlocks() * BLOCK_SIZE;
        }
    }
    total_internal_slack = total_allocated_disk_bytes - total_payload_bytes;

    std::cout << ansi::BOLD << "=== System-Wide Storage Efficiency ===" << ansi::RESET << "\n";
    std::cout << "Actual User Data Stored : " << total_payload_bytes << " bytes\n";
    std::cout << "Raw Disk Blocks Occupied: " << total_allocated_disk_bytes << " bytes ("
              << (total_allocated_disk_bytes / BLOCK_SIZE) << " sectors)\n";
    std::cout << "Total Overhead / Slack  : " << total_internal_slack << " bytes (Internal Slack & Pointers/Indices)\n";

    if (total_allocated_disk_bytes > 0) {
        double efficiency = (static_cast<double>(total_payload_bytes) / total_allocated_disk_bytes) * 100.0;
        std::cout << "Payload Storage Density : " << std::fixed << std::setprecision(2) << efficiency << "%\n";
    }
    std::cout << "\n";
}

void FileSystem::save(std::ostream& os) const {
    os.write(reinterpret_cast<const char*>(&next_file_id_), sizeof(next_file_id_));
    uint8_t alloc = static_cast<uint8_t>(current_alloc_type_);
    os.write(reinterpret_cast<const char*>(&alloc), sizeof(alloc));

    uint32_t dir_size = static_cast<uint32_t>(directory_.size());
    os.write(reinterpret_cast<const char*>(&dir_size), sizeof(dir_size));

    for (const auto& file : directory_) {
        file.serialize(os);
    }
}

void FileSystem::load(std::istream& is) {
    directory_.clear();
    is.read(reinterpret_cast<char*>(&next_file_id_), sizeof(next_file_id_));

    uint8_t alloc = 0;
    is.read(reinterpret_cast<char*>(&alloc), sizeof(alloc));
    current_alloc_type_ = static_cast<AllocationType>(alloc);

    uint32_t dir_size = 0;
    is.read(reinterpret_cast<char*>(&dir_size), sizeof(dir_size));

    if (dir_size < 10000) {
        for (uint32_t i = 0; i < dir_size; ++i) {
            directory_.push_back(FileMetadata::deserialize(is));
        }
    }
}

} // namespace da
