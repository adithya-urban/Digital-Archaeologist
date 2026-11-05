/**
 * @file VirtualDisk.cpp
 * @brief Implementation of the VirtualDisk block-level storage simulation.
 */

#include "VirtualDisk.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace da {

VirtualDisk::VirtualDisk(const std::string& disk_file_path)
    : disk_file_path_(disk_file_path) {
    for (auto& block : blocks_) {
        block.status = BlockStatus::FREE;
        block.owner_file_id = -1;
        block.data.fill(0);
    }
}

bool VirtualDisk::initialize() {
    for (auto& block : blocks_) {
        block.status = BlockStatus::FREE;
        block.owner_file_id = -1;
        block.data.fill(0);
    }
    return save();
}

bool VirtualDisk::imageExists() const {
    std::ifstream ifs(disk_file_path_, std::ios::binary);
    return ifs.good();
}

bool VirtualDisk::save() const {
    std::ofstream ofs(disk_file_path_, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        return false;
    }

    // Write header
    ofs.write(reinterpret_cast<const char*>(&MAGIC), sizeof(MAGIC));
    ofs.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));
    uint32_t num_blks = NUM_BLOCKS;
    uint32_t blk_sz   = BLOCK_SIZE;
    ofs.write(reinterpret_cast<const char*>(&num_blks), sizeof(num_blks));
    ofs.write(reinterpret_cast<const char*>(&blk_sz), sizeof(blk_sz));

    // Write blocks
    for (const auto& b : blocks_) {
        uint8_t st = static_cast<uint8_t>(b.status);
        ofs.write(reinterpret_cast<const char*>(&st), sizeof(st));
        ofs.write(reinterpret_cast<const char*>(&b.owner_file_id), sizeof(b.owner_file_id));
        ofs.write(reinterpret_cast<const char*>(b.data.data()), BLOCK_SIZE);
    }

    return ofs.good();
}

bool VirtualDisk::load() {
    std::ifstream ifs(disk_file_path_, std::ios::binary);
    if (!ifs) {
        return false;
    }

    uint32_t magic = 0, version = 0, num_blks = 0, blk_sz = 0;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    ifs.read(reinterpret_cast<char*>(&version), sizeof(version));
    ifs.read(reinterpret_cast<char*>(&num_blks), sizeof(num_blks));
    ifs.read(reinterpret_cast<char*>(&blk_sz), sizeof(blk_sz));

    if (magic != MAGIC || version != VERSION || num_blks != NUM_BLOCKS || blk_sz != BLOCK_SIZE) {
        return false;
    }

    for (auto& b : blocks_) {
        uint8_t st = 0;
        ifs.read(reinterpret_cast<char*>(&st), sizeof(st));
        b.status = static_cast<BlockStatus>(st);
        ifs.read(reinterpret_cast<char*>(&b.owner_file_id), sizeof(b.owner_file_id));
        ifs.read(reinterpret_cast<char*>(b.data.data()), BLOCK_SIZE);
    }

    return ifs.good();
}

Block& VirtualDisk::getBlock(int index) {
    if (index < 0 || index >= NUM_BLOCKS) {
        throw std::out_of_range("Block index out of bounds: " + std::to_string(index));
    }
    return blocks_[index];
}

const Block& VirtualDisk::getBlock(int index) const {
    if (index < 0 || index >= NUM_BLOCKS) {
        throw std::out_of_range("Block index out of bounds: " + std::to_string(index));
    }
    return blocks_[index];
}

BlockStatus VirtualDisk::getBlockStatus(int index) const {
    return getBlock(index).status;
}

void VirtualDisk::setBlockStatus(int index, BlockStatus status) {
    getBlock(index).status = status;
}

void VirtualDisk::setBlockOwner(int index, int32_t file_id) {
    getBlock(index).owner_file_id = file_id;
}

void VirtualDisk::writeBlock(int index, const uint8_t* data, int size) {
    Block& b = getBlock(index);
    int to_copy = std::min(size, BLOCK_SIZE);
    if (to_copy > 0 && data != nullptr) {
        std::memcpy(b.data.data(), data, to_copy);
    }
    if (to_copy < BLOCK_SIZE) {
        std::memset(b.data.data() + to_copy, 0, BLOCK_SIZE - to_copy);
    }
}

void VirtualDisk::readBlock(int index, uint8_t* buffer, int size) const {
    const Block& b = getBlock(index);
    int to_copy = std::min(size, BLOCK_SIZE);
    if (to_copy > 0 && buffer != nullptr) {
        std::memcpy(buffer, b.data.data(), to_copy);
    }
}

void VirtualDisk::clearBlockData(int index) {
    getBlock(index).data.fill(0);
}

int VirtualDisk::getFreeBlockCount() const {
    int c = 0;
    for (const auto& b : blocks_) {
        if (b.status == BlockStatus::FREE) c++;
    }
    return c;
}

int VirtualDisk::getDeletedBlockCount() const {
    int c = 0;
    for (const auto& b : blocks_) {
        if (b.status == BlockStatus::DELETED) c++;
    }
    return c;
}

int VirtualDisk::getAllocatedBlockCount() const {
    int c = 0;
    for (const auto& b : blocks_) {
        if (b.status == BlockStatus::ALLOCATED) c++;
    }
    return c;
}

std::vector<int> VirtualDisk::findAvailableBlocks(int count) const {
    std::vector<int> result;
    if (count <= 0) return result;

    // Realistic OS block allocator policy:
    // 1. Try to take completely FREE (virgin) blocks first to avoid clobbering deleted data.
    // 2. If not enough FREE blocks exist, reclaim DELETED blocks.
    for (int i = 0; i < NUM_BLOCKS && static_cast<int>(result.size()) < count; ++i) {
        if (blocks_[i].status == BlockStatus::FREE) {
            result.push_back(i);
        }
    }

    if (static_cast<int>(result.size()) < count) {
        for (int i = 0; i < NUM_BLOCKS && static_cast<int>(result.size()) < count; ++i) {
            if (blocks_[i].status == BlockStatus::DELETED) {
                // Check not already picked
                if (std::find(result.begin(), result.end(), i) == result.end()) {
                    result.push_back(i);
                }
            }
        }
    }

    if (static_cast<int>(result.size()) < count) {
        return {}; // Not enough total available space
    }

    return result;
}

std::vector<int> VirtualDisk::findContiguousAvailableBlocks(int count) const {
    if (count <= 0) return {};

    // First attempt: search for a run consisting purely of FREE blocks
    int current_run = 0;
    int start_index = -1;

    for (int i = 0; i < NUM_BLOCKS; ++i) {
        if (blocks_[i].status == BlockStatus::FREE) {
            if (current_run == 0) start_index = i;
            current_run++;
            if (current_run == count) {
                std::vector<int> res;
                for (int k = 0; k < count; ++k) res.push_back(start_index + k);
                return res;
            }
        } else {
            current_run = 0;
            start_index = -1;
        }
    }

    // Second attempt: search for a run of FREE or DELETED blocks
    current_run = 0;
    start_index = -1;
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        if (blocks_[i].status == BlockStatus::FREE || blocks_[i].status == BlockStatus::DELETED) {
            if (current_run == 0) start_index = i;
            current_run++;
            if (current_run == count) {
                std::vector<int> res;
                for (int k = 0; k < count; ++k) res.push_back(start_index + k);
                return res;
            }
        } else {
            current_run = 0;
            start_index = -1;
        }
    }

    return {}; // No contiguous span large enough (External Fragmentation!)
}

void VirtualDisk::displayDiskMap() const {
    std::cout << "\n" << ansi::BOLD << ansi::CYAN
              << "╔════════════════════════════════════════════════════════════════════════════════╗\n"
              << "║                         VIRTUAL DISK SECTOR MAP                                ║\n"
              << "╚════════════════════════════════════════════════════════════════════════════════╝\n"
              << ansi::RESET;

    std::cout << "       ";
    for (int col = 0; col < 16; ++col) {
        std::cout << std::setw(3) << std::hex << std::uppercase << col << " ";
    }
    std::cout << std::dec << "\n";
    std::cout << "      +" << std::string(64, '-') << "+\n";

    for (int row = 0; row < 16; ++row) {
        std::cout << " 0x" << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << (row * 16) << std::dec << std::setfill(' ') << " |";
        for (int col = 0; col < 16; ++col) {
            int idx = row * 16 + col;
            BlockStatus st = blocks_[idx].status;

            switch (st) {
                case BlockStatus::FREE:
                    std::cout << ansi::GRAY << "  . " << ansi::RESET;
                    break;
                case BlockStatus::ALLOCATED: {
                    int fid = blocks_[idx].owner_file_id;
                    if (fid >= 0) {
                        std::cout << ansi::BOLD << ansi::GREEN << " " << std::setw(2) << (fid % 100) << " " << ansi::RESET;
                    } else {
                        std::cout << ansi::BOLD << ansi::GREEN << "  # " << ansi::RESET;
                    }
                    break;
                }
                case BlockStatus::DELETED: {
                    int fid = blocks_[idx].owner_file_id;
                    if (fid >= 0) {
                        std::cout << ansi::BOLD << ansi::YELLOW << " " << std::setw(2) << (fid % 100) << "?" << ansi::RESET;
                    } else {
                        std::cout << ansi::BOLD << ansi::YELLOW << "  ? " << ansi::RESET;
                    }
                    break;
                }
            }
        }
        std::cout << "|\n";
    }
    std::cout << "      +" << std::string(64, '-') << "+\n\n";

    std::cout << ansi::BOLD << "Legend: " << ansi::RESET
              << ansi::GRAY << "[ . ] Free (" << getFreeBlockCount() << ")" << ansi::RESET << "   "
              << ansi::BOLD << ansi::GREEN << "[ ##] Allocated (" << getAllocatedBlockCount() << ")" << ansi::RESET << "   "
              << ansi::BOLD << ansi::YELLOW << "[##?] Deleted/Recoverable (" << getDeletedBlockCount() << ")" << ansi::RESET << "\n"
              << "Note: numbers represent the owning File ID.\n\n";
}

void VirtualDisk::displayBlockContent(int index) const {
    if (index < 0 || index >= NUM_BLOCKS) {
        std::cout << ansi::RED << "[!] Invalid block index: " << index << ansi::RESET << "\n";
        return;
    }

    const Block& b = blocks_[index];
    std::cout << "\n" << ansi::BOLD << "=== Block Inspector: Sector #" << index << " ===" << ansi::RESET << "\n";
    std::cout << "Status: ";
    switch (b.status) {
        case BlockStatus::FREE:
            std::cout << ansi::GRAY << "FREE" << ansi::RESET;
            break;
        case BlockStatus::ALLOCATED:
            std::cout << ansi::GREEN << "ALLOCATED" << ansi::RESET;
            break;
        case BlockStatus::DELETED:
            std::cout << ansi::YELLOW << "DELETED (Contains recoverable residual data)" << ansi::RESET;
            break;
    }
    std::cout << " | Owning File ID: " << b.owner_file_id << "\n";
    std::cout << "Raw Bytes (Hex Dump & ASCII):\n";
    std::cout << "Offset  Hex Data (16 bytes per line)             ASCII\n";
    std::cout << "------  ---------------------------------------  ----------------\n";

    for (int row = 0; row < BLOCK_SIZE; row += 16) {
        std::cout << "0x" << std::right << std::setw(4) << std::setfill('0') << std::hex << row << "  ";

        for (int col = 0; col < 16; ++col) {
            int off = row + col;
            if (off < BLOCK_SIZE) {
                std::cout << std::right << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(b.data[off]) << " ";
            } else {
                std::cout << "   ";
            }
        }

        std::cout << " ";
        for (int col = 0; col < 16; ++col) {
            int off = row + col;
            if (off < BLOCK_SIZE) {
                char ch = static_cast<char>(b.data[off]);
                if (ch >= 32 && ch <= 126) {
                    std::cout << ch;
                } else {
                    std::cout << '.';
                }
            }
        }
        std::cout << std::dec << std::setfill(' ') << "\n";
    }
    std::cout << "\n";
}

void VirtualDisk::displayDiskStats() const {
    int free_b = getFreeBlockCount();
    int alloc_b = getAllocatedBlockCount();
    int del_b = getDeletedBlockCount();

    double util_alloc = (static_cast<double>(alloc_b) / NUM_BLOCKS) * 100.0;
    double util_del = (static_cast<double>(del_b) / NUM_BLOCKS) * 100.0;

    std::cout << "\n" << ansi::BOLD << "╔══════════════════════════════════════════════════╗\n"
              << "║              DISK GEOMETRY & STATUS              ║\n"
              << "╚══════════════════════════════════════════════════╝\n" << ansi::RESET;
    std::cout << "  Block / Sector Size : " << BLOCK_SIZE << " bytes\n";
    std::cout << "  Total Sectors       : " << NUM_BLOCKS << " blocks\n";
    std::cout << "  Total Disk Capacity : " << DISK_SIZE << " bytes (" << (DISK_SIZE / 1024) << " KB)\n";
    std::cout << "  Allocated Blocks    : " << ansi::GREEN << alloc_b << " (" << std::fixed << std::setprecision(1) << util_alloc << "%)\n" << ansi::RESET;
    std::cout << "  Deleted Blocks      : " << ansi::YELLOW << del_b << " (" << util_del << "%)\n" << ansi::RESET;
    std::cout << "  Free Blocks         : " << ansi::GRAY << free_b << " (" << (100.0 - util_alloc - util_del) << "%)\n" << ansi::RESET;
    std::cout << "  Usable Space Left   : " << ((free_b + del_b) * BLOCK_SIZE) << " bytes\n\n";
}

} // namespace da
