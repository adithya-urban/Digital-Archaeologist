/**
 * @file main.cpp
 * @brief Interactive terminal application: Digital Archaeologist (File Recovery Simulator).
 */

#include "Common.h"
#include "VirtualDisk.h"
#include "FileSystem.h"
#include "RecoveryEngine.h"

#include <iostream>
#include <fstream>
#include <string>
#include <limits>
#include <memory>

#ifdef _WIN32
#include <windows.h>
void enableVirtualTerminalProcessing() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
#else
void enableVirtualTerminalProcessing() {}
#endif

namespace da {

const std::string DISK_IMAGE_FILE = "virtual_disk.bin";
const std::string FS_META_FILE    = "fs_metadata.bin";

void saveAll(const VirtualDisk& disk, const FileSystem& fs) {
    disk.save();
    std::ofstream ofs(FS_META_FILE, std::ios::binary | std::ios::trunc);
    if (ofs) {
        fs.save(ofs);
    }
}

void loadAll(VirtualDisk& disk, FileSystem& fs) {
    if (disk.imageExists()) {
        disk.load();
    } else {
        disk.initialize();
    }

    std::ifstream ifs(FS_META_FILE, std::ios::binary);
    if (ifs) {
        fs.load(ifs);
    }
}

void clearInputBuffer() {
    if (std::cin.eof()) return;
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

std::string getLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) {
        return "";
    }
    return line;
}

void printBanner() {
    std::cout << ansi::BOLD << ansi::CYAN
              << "╔══════════════════════════════════════════════════════════════════════════════════╗\n"
              << "║               DIGITAL ARCHAEOLOGIST: FILE RECOVERY SIMULATOR                     ║\n"
              << "║       Operating Systems Lab: Disk Allocation, Fragmentation & File Carving       ║\n"
              << "╚══════════════════════════════════════════════════════════════════════════════════╝\n"
              << ansi::RESET;
}

void printMenu() {
    std::cout << ansi::BOLD << "\n--- MAIN CONTROL PANEL ---" << ansi::RESET << "\n"
              << " [1]  Create New File\n"
              << " [2]  Read Active File\n"
              << " [3]  Delete File (Soft Deletion)\n"
              << " [4]  List Active Files\n"
              << " [5]  Show File Details & Sector Health\n"
              << " [6]  Display Disk Sector Map (Visual 16x16 Grid)\n"
              << " [7]  Inspect Specific Block / Sector (Hex & ASCII Dump)\n"
              << " [8]  Disk Geometry & Allocation Statistics\n"
              << " [9]  Analyze Internal & External Fragmentation\n"
              << " [10] Forensic Scan (Deleted File Carving Table)\n"
              << " [11] Recover Deleted File\n"
              << " [12] Run Educational Demonstration Scenario\n"
              << " [13] Format / Reset Virtual Disk\n"
              << " [0]  Exit Application\n"
              << "Select option [0-13]: ";
}

void handleCreateFile(FileSystem& fs, VirtualDisk& disk) {
    std::cout << "\n" << ansi::BOLD << "--- Create File ---" << ansi::RESET << "\n";
    std::string filename = getLine("Enter filename: ");
    if (filename.empty()) {
        std::cout << ansi::RED << "[!] Filename cannot be empty." << ansi::RESET << "\n";
        return;
    }

    std::cout << "Choose Allocation Method:\n"
              << "  [1] Contiguous Allocation (Sequential blocks, prone to external fragmentation)\n"
              << "  [2] Linked Allocation     (Pointer chain, no external fragmentation)\n"
              << "  [3] Indexed Allocation    (Index block table, random access support)\n"
              << "Selection [1-3, default 1]: ";
    std::string alloc_choice_str;
    std::getline(std::cin, alloc_choice_str);

    AllocationType alloc_type = AllocationType::CONTIGUOUS;
    if (alloc_choice_str == "2") {
        alloc_type = AllocationType::LINKED;
    } else if (alloc_choice_str == "3") {
        alloc_type = AllocationType::INDEXED;
    }

    std::string content = getLine("Enter file content / text payload: ");
    bool ok = fs.createFile(filename, content, alloc_type);
    if (ok) {
        saveAll(disk, fs);
        std::cout << ansi::GREEN << "[+] File '" << filename << "' created successfully ("
                  << content.size() << " bytes) using " << allocationTypeName(alloc_type) << " allocation." << ansi::RESET << "\n";
    }
}

void handleReadFile(FileSystem& fs) {
    std::cout << "\n" << ansi::BOLD << "--- Read File ---" << ansi::RESET << "\n";
    std::string filename = getLine("Enter filename to read: ");
    auto content = fs.readFile(filename);
    if (content.has_value()) {
        std::cout << ansi::BOLD << ansi::GREEN << "Content of '" << filename << "':" << ansi::RESET << "\n";
        std::cout << "┌" << std::string(60, '-') << "┐\n";
        std::cout << *content << "\n";
        std::cout << "└" << std::string(60, '-') << "┘\n";
    } else {
        std::cout << ansi::RED << "[!] Error: File '" << filename << "' not found or is currently deleted." << ansi::RESET << "\n";
    }
}

void handleDeleteFile(FileSystem& fs, VirtualDisk& disk) {
    std::cout << "\n" << ansi::BOLD << "--- Delete File ---" << ansi::RESET << "\n";
    std::string filename = getLine("Enter filename to delete: ");
    bool ok = fs.deleteFile(filename);
    if (ok) {
        saveAll(disk, fs);
        std::cout << ansi::YELLOW << "[*] File '" << filename << "' deleted." << ansi::RESET << "\n"
                  << "    (OS Concept: Directory entry flagged as deleted. Blocks marked available in free-list,\n"
                  << "     but raw sector data has NOT been erased and remains recoverable.)\n";
    }
}

void handleInspectBlock(const VirtualDisk& disk) {
    std::cout << "\n" << ansi::BOLD << "--- Inspect Disk Sector ---" << ansi::RESET << "\n";
    std::cout << "Enter sector number [0 to " << (NUM_BLOCKS - 1) << "]: ";
    int blk = -1;
    if (!(std::cin >> blk)) {
        if (std::cin.eof()) return;
        clearInputBuffer();
        std::cout << ansi::RED << "[!] Invalid input." << ansi::RESET << "\n";
        return;
    }
    clearInputBuffer();
    disk.displayBlockContent(blk);
}

void handleRecoverFile(RecoveryEngine& engine, VirtualDisk& disk, FileSystem& fs) {
    std::cout << "\n" << ansi::BOLD << "--- Recover Deleted File ---" << ansi::RESET << "\n";
    std::string filename = getLine("Enter deleted filename to recover: ");
    RecoveryResult res = engine.attemptRecovery(filename);
    engine.displayRecoveryReport(res);
    if (res.success || res.partial) {
        saveAll(disk, fs);
    }
}

void runDemoScenario(FileSystem& fs, VirtualDisk& disk, RecoveryEngine& engine) {
    std::cout << "\n" << ansi::BOLD << ansi::CYAN
              << "╔══════════════════════════════════════════════════════════════════════════════════╗\n"
              << "║                AUTOMATED STEP-BY-STEP LAB DEMONSTRATION                          ║\n"
              << "╚══════════════════════════════════════════════════════════════════════════════════╝\n"
              << ansi::RESET;

    auto pauseStep = [](const std::string& step_title) {
        std::cout << "\n" << ansi::BOLD << ansi::WHITE << ">>> [Step " << step_title << "] Press Enter to proceed..." << ansi::RESET;
        std::string dummy;
        if (!std::getline(std::cin, dummy)) {
            // EOF reached
        }
    };

    // Step 0: Clean format
    pauseStep("0: Format disk to clean state");
    disk.initialize();
    fs.reset();
    saveAll(disk, fs);
    std::cout << "[*] Virtual disk initialized clean.\n";

    // Step 1: Create multiple files using different allocation schemes
    pauseStep("1: Create three files (DocA, SecretB, ArchiveC) with distinct allocation schemes");
    fs.createFile("DocA.txt", "Operating Systems Lab Demonstration - Principles of File Allocation.", AllocationType::CONTIGUOUS);
    fs.createFile("SecretB.dat", "CONFIDENTIAL_TOP_SECRET_RECOVERY_KEY_992147_OS_PASSCODE", AllocationType::LINKED);
    fs.createFile("ArchiveC.bin", "Structured indexed data archive records 101, 102, 103, 104.", AllocationType::INDEXED);
    saveAll(disk, fs);
    fs.listFiles();
    disk.displayDiskMap();

    // Step 2: Delete SecretB.dat
    pauseStep("2: Soft-delete 'SecretB.dat'");
    fs.deleteFile("SecretB.dat");
    saveAll(disk, fs);
    std::cout << ansi::YELLOW << "[*] 'SecretB.dat' deleted. Notice in the directory listing it disappears from active files.\n" << ansi::RESET;
    fs.listFiles();
    std::cout << "Notice on the Disk Map below that its blocks turn YELLOW [##?], marked available for recycling:\n";
    disk.displayDiskMap();

    // Step 3: Forensic block inspection
    pauseStep("3: Forensic inspection of raw sectors (verifying residual data)");
    const FileMetadata* deleted_meta = fs.findFile("SecretB.dat", true);
    if (deleted_meta && !deleted_meta->data_blocks.empty()) {
        int target_sector = deleted_meta->data_blocks[0];
        std::cout << "Inspecting Sector #" << target_sector << " of the deleted file:\n";
        disk.displayBlockContent(target_sector);
        std::cout << ansi::GREEN << "[✓] Note that the actual text 'CONFIDENTIAL...' is still completely present in the sector!\n" << ansi::RESET;
    }

    // Step 4: Forensic scan & Recovery
    pauseStep("4: Perform Forensic Scan and Recover 'SecretB.dat'");
    engine.displayForensicScan();
    RecoveryResult res = engine.attemptRecovery("SecretB.dat");
    engine.displayRecoveryReport(res);
    saveAll(disk, fs);

    pauseStep("5: Verify 'SecretB.dat' is restored back into active directory");
    fs.listFiles();
    auto read_content = fs.readFile("SecretB.dat");
    if (read_content) {
        std::cout << ansi::BOLD << ansi::GREEN << "Read recovered file 'SecretB.dat': " << *read_content << ansi::RESET << "\n";
    }

    // Step 6: Educational overwrite experiment (Block recycling & Data loss)
    pauseStep("6: Advanced Overwrite Scenario: Delete file again, then overwrite its sectors");
    fs.deleteFile("SecretB.dat");
    std::cout << "[*] 'SecretB.dat' deleted again.\n";
    std::cout << "[*] Simulating sector recycling: A new file/process writes over Sector #2 (reclaimed by File #99)...\n";
    
    // Simulate re-allocation and overwriting of sector 2
    disk.setBlockStatus(2, BlockStatus::ALLOCATED);
    disk.setBlockOwner(2, 99);
    const char overwrite_payload[] = "NEW_PROCESS_DATA_OVERWRITING_SECTOR_02_CORRUPTING_ORIGINAL_TEXT!";
    disk.writeBlock(2, reinterpret_cast<const uint8_t*>(overwrite_payload), sizeof(overwrite_payload));
    saveAll(disk, fs);
    disk.displayDiskMap();
    std::cout << "Inspecting Sector #2 post-overwrite:\n";
    disk.displayBlockContent(2);

    pauseStep("7: Forensic scan after overwrite (observe corruption / unrecoverability)");
    engine.displayForensicScan();
    RecoveryResult res2 = engine.attemptRecovery("SecretB.dat");
    engine.displayRecoveryReport(res2);

    pauseStep("8: Fragmentation Analysis");
    fs.showFragmentationAnalysis();

    std::cout << ansi::BOLD << ansi::GREEN << "\n[✓] Demonstration scenario completed successfully!\n" << ansi::RESET;
}

} // namespace da

int main() {
    enableVirtualTerminalProcessing();
    da::printBanner();

    da::VirtualDisk disk(da::DISK_IMAGE_FILE);
    da::FileSystem fs(disk);
    da::loadAll(disk, fs);
    da::RecoveryEngine engine(disk, fs);

    bool running = true;
    while (running) {
        da::printMenu();
        int choice = -1;
        if (!(std::cin >> choice)) {
            if (std::cin.eof()) {
                std::cout << "\n[EOF reached. Exiting]\n";
                break;
            }
            da::clearInputBuffer();
            std::cout << da::ansi::RED << "[!] Invalid input. Please enter a number from 0 to 13." << da::ansi::RESET << "\n";
            continue;
        }
        da::clearInputBuffer();

        switch (choice) {
            case 1:
                da::handleCreateFile(fs, disk);
                break;
            case 2:
                da::handleReadFile(fs);
                break;
            case 3:
                da::handleDeleteFile(fs, disk);
                break;
            case 4:
                fs.listFiles();
                break;
            case 5: {
                std::string fn = da::getLine("Enter filename: ");
                fs.showFileDetails(fn);
                break;
            }
            case 6:
                disk.displayDiskMap();
                break;
            case 7:
                da::handleInspectBlock(disk);
                break;
            case 8:
                disk.displayDiskStats();
                break;
            case 9:
                fs.showFragmentationAnalysis();
                break;
            case 10:
                engine.displayForensicScan();
                break;
            case 11:
                da::handleRecoverFile(engine, disk, fs);
                break;
            case 12:
                da::runDemoScenario(fs, disk, engine);
                break;
            case 13: {
                std::string confirm = da::getLine("Are you sure you want to format the disk? All data will be erased! (y/N): ");
                if (confirm == "y" || confirm == "Y") {
                    disk.initialize();
                    fs.reset();
                    da::saveAll(disk, fs);
                    std::cout << da::ansi::GREEN << "[+] Virtual disk formatted successfully." << da::ansi::RESET << "\n";
                } else {
                    std::cout << "[-] Format canceled.\n";
                }
                break;
            }
            case 0:
                std::cout << "\nSaving disk state and exiting Digital Archaeologist. Farewell!\n";
                da::saveAll(disk, fs);
                running = false;
                break;
            default:
                std::cout << da::ansi::RED << "[!] Unknown option: " << choice << da::ansi::RESET << "\n";
                break;
        }
    }

    return 0;
}
