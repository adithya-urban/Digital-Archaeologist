# Digital Archaeologist: Deleted File Recovery Simulator

**Digital Archaeologist** is an interactive, educational C++17 systems simulation application designed to demonstrate the fundamental inner workings of operating system file systems, disk block allocation, fragmentation dynamics, and forensic deleted-file recovery (file carving).

---

## Table of Contents
1. [Overview](#overview)
2. [Operating System Concepts Demonstrated](#operating-system-concepts-demonstrated)
   - [Disk Block & Sector Management](#1-disk-block--sector-management)
   - [File Allocation Methods](#2-file-allocation-methods)
   - [File Metadata & Directory Structures](#3-file-metadata--directory-structures)
   - [Soft Deletion & Recovery Mechanism](#4-soft-deletion--recovery-mechanism)
   - [Internal and External Fragmentation](#5-internal-and-external-fragmentation)
3. [Architecture and Modular Design](#architecture-and-modular-design)
4. [Building and Running](#building-and-running)
   - [Prerequisites](#prerequisites)
   - [Build with CMake (MinGW / GCC / Clang / MSVC)](#build-with-cmake)
   - [Direct Compilation with g++](#direct-compilation-with-g)
5. [Menu & Feature Guide](#menu--feature-guide)
6. [Sample Demonstration & Expected Output](#sample-demonstration--expected-output)
7. [Persistence & Binary Disk Image](#persistence--binary-disk-image)

---

## Overview

Modern operating systems (such as Linux with ext4, Windows with NTFS, and macOS with APFS) do not immediately zero out or shred physical disk sectors when a user deletes a file. Doing so would impose an enormous I/O performance penalty on delete operations. Instead, operating systems perform **soft deletion**:
- The directory entry or inode/MFT record is marked as deleted/inactive.
- The associated disk sectors are marked as "available" in the free-space bitmap or free-list.
- **The actual data bytes remain physically intact on disk** until another file's allocation overwrites those sectors.

**Digital Archaeologist** simulates this exact low-level operating system lifecycle inside a virtualized 16 KB disk (256 sectors × 64 bytes/sector), allowing computer science students, educators, and systems enthusiasts to observe block allocation, inspect raw sectors in hexadecimal/ASCII, track fragmentation, soft-delete files, and forensically carve/restore them back into the active directory.

---

## Operating System Concepts Demonstrated

### 1. Disk Block & Sector Management
- Disks are divided into fixed-size units known as **blocks** or **sectors** (simulated here as 64-byte blocks across 256 sectors).
- The `VirtualDisk` class manages block reading, writing, clearing, and allocation status.
- Each block maintains an allocation state:
  - `FREE (0)`: Virgin or zeroed block, never touched or completely reclaimed.
  - `ALLOCATED (1)`: Currently owned by an active file in the directory.
  - `DELETED (2)`: Soft-deleted block holding residual data that has not yet been overwritten.

### 2. File Allocation Methods
The simulator implements three foundational file allocation algorithms using the **Strategy Pattern**:

| Metric / Aspect | Contiguous Allocation | Linked Allocation | Indexed Allocation |
|---|---|---|---|
| **Storage Layout** | Consecutive linear blocks | Scattered blocks chained by next-block pointers | Dedicated index block containing pointers to scattered data blocks |
| **Directory Record** | Start block + block count | Start block + pointer to first block | Index block sector number |
| **Random Access** | $O(1)$ fast direct calculation | $O(N)$ slow; must traverse chain | $O(1)$ via index block lookup |
| **Internal Fragmentation** | In last block only | In last block only | In last block only |
| **External Fragmentation** | **Severe**; requires contiguous spans | **None**; any free block can be chained | **None**; any free block can be indexed |
| **Overhead** | None | 4 bytes per block (next-block pointer) | 1 full block (64 bytes) per file for index table |
| **Max File Size** | Limited by largest contiguous free run | Limited only by total free blocks | Limited by index block capacity (16 entries × 64 bytes = 1024 bytes) |

### 3. File Metadata & Directory Structures
- The virtual file system maintains a directory table analogous to FAT directory entries, Unix inode tables, or NTFS Master File Table (MFT) records.
- Each `FileMetadata` record retains:
  - Unique `file_id`
  - Human-readable `filename`
  - Exact payload size in bytes (`file_size`)
  - Allocation strategy used (`alloc_type`)
  - Chronological timestamps (`created_at`, `deleted_at`)
  - List of physical disk sectors (`data_blocks`, `start_block`, `index_block`)
  - Soft-deletion status flag (`deleted`)

### 4. Soft Deletion & Recovery Mechanism
- When `deleteFile()` is invoked:
  1. The file metadata record in the directory is flagged as `deleted = true`.
  2. The disk sectors assigned to the file are marked as `DELETED` in the disk block map.
  3. **The raw bytes on the disk blocks are preserved.**
- The `RecoveryEngine` acts as a digital archaeologist:
  1. **Forensic Scan**: Scans all soft-deleted directory entries and audits their referenced sectors.
  2. **Integrity Assessment**:
     - **100% Intact**: All original sectors remain in `DELETED` state with matching file ID. Full recovery is guaranteed.
     - **Partially Damaged**: Some sectors were re-allocated to a newly created file. The engine carves out intact sectors and marks overwritten sectors with `[<<< CORRUPTED/OVERWRITTEN SECTOR >>>]`.
     - **Overwritten / Lost**: All sectors have been claimed and clobbered by subsequent allocations.
  3. **Restoration**: Reclaims intact blocks into the disk allocation bitmap and re-activates the directory record.

### 5. Internal and External Fragmentation
- **Internal Fragmentation**: Wasted space that occurs inside an allocated block because the file payload does not evenly divide by the block size (or usable data portion). The simulator calculates exact slack space for every active file.
- **External Fragmentation**: Occurs when total free memory exists on disk to satisfy an allocation request, but the free space is broken up into small non-contiguous holes. The simulator highlights when a contiguous allocation fails even when overall disk capacity has ample free sectors.

---

## Architecture and Modular Design

```
Digital-Archaeologist/
├── CMakeLists.txt              # Standard cross-platform CMake build configuration
├── README.md                   # Comprehensive documentation and lab manual
├── include/
│   ├── Common.h                # Disk geometry, limits, enums, ANSI colour constants
│   ├── FileMetadata.h          # Inode / directory record definition & serialization
│   ├── VirtualDisk.h           # Low-level sector storage, hex dumping, disk map
│   ├── AllocationStrategy.h    # Contiguous, Linked, and Indexed strategy interfaces
│   ├── FileSystem.h            # High-level virtual file system API & directory manager
│   └── RecoveryEngine.h        # Forensic scanner, carver, and file recoverer
└── src/
    ├── FileMetadata.cpp        # Binary serialization of file metadata
    ├── VirtualDisk.cpp         # Block array management, persistence, disk map GUI
    ├── AllocationStrategy.cpp  # Implementation of the 3 allocation strategies
    ├── FileSystem.cpp          # File CRUD operations, directory listings, fragmentation
    ├── RecoveryEngine.cpp      # File carving, integrity check, recovery implementation
    └── main.cpp                # Interactive terminal control panel & demo scenario
```

### Class Responsibilities
- **`VirtualDisk`**: Low-level sector manager. Handles sector reads, writes, raw byte dumps, free-list searches, and ANSI 16×16 terminal visualization.
- **`AllocationStrategy`**: Abstract interface implementing Contiguous, Linked, and Indexed block placement algorithms.
- **`FileSystem`**: High-level manager providing `createFile()`, `readFile()`, `deleteFile()`, `listFiles()`, and fragmentation analytics.
- **`RecoveryEngine`**: Forensic analyzer that inspects residual disk blocks of deleted files, measures corruption, and performs file restoration.
- **`FileMetadata`**: Data holder representing an inode/MFT entry with binary serialization.

---

## Building and Running

### Prerequisites
- C++17 compatible compiler:
  - GCC 8.0+ / MinGW-w64
  - Clang 7.0+
  - MSVC 2019+ (Visual Studio 16.0+)
- CMake 3.16 or newer (optional for direct g++ compilation)

### Build with CMake

```bash
# 1. Create build directory and configure
cmake -B build -G "MinGW Makefiles"   # On Windows with MinGW
# or: cmake -B build                  # On Linux / macOS / default generator

# 2. Compile
cmake --build build

# 3. Run the executable
# On Windows:
.\build\DigitalArchaeologist.exe
# On Linux/macOS:
./build/DigitalArchaeologist
```

### Direct Compilation with g++

You can also compile directly in a single command without CMake:

```bash
g++ -std=c++17 -Wall -Wextra -O2 -Iinclude src/*.cpp -o DigitalArchaeologist.exe
./DigitalArchaeologist.exe
```

---

## Menu & Feature Guide

When you launch the program, the main control panel provides the following options:

```
--- MAIN CONTROL PANEL ---
 [1]  Create New File
 [2]  Read Active File
 [3]  Delete File (Soft Deletion)
 [4]  List Active Files
 [5]  Show File Details & Sector Health
 [6]  Display Disk Sector Map (Visual 16x16 Grid)
 [7]  Inspect Specific Block / Sector (Hex & ASCII Dump)
 [8]  Disk Geometry & Allocation Statistics
 [9]  Analyze Internal & External Fragmentation
 [10] Forensic Scan (Deleted File Carving Table)
 [11] Recover Deleted File
 [12] Run Educational Demonstration Scenario
 [13] Format / Reset Virtual Disk
 [0]  Exit Application
```

### Feature Highlights:
- **Visual Disk Map (Option 6)**: Displays all 256 sectors in a 16×16 matrix.
  - `.` (Gray): Free sector
  - `##` (Green): Allocated sector (displays owning file ID)
  - `##?` (Yellow): Soft-deleted sector holding recoverable residual data
- **Hex/ASCII Inspector (Option 7)**: Shows a 16-byte-per-line hexadecimal dump and ASCII interpretation of any sector on disk, just like `xxd` or `hexdump`.
- **Fragmentation Report (Option 9)**: Details slack space (internal fragmentation) and checks for non-contiguous gaps (external fragmentation).
- **Forensic Scan (Option 10)**: Audits the disk for soft-deleted files and predicts recovery success percentage before attempting recovery.

---

## Sample Demonstration & Expected Output

Selecting option `[12]` runs an automated, step-by-step educational lab demonstration that exercises the core scenario requested:

### Step-by-Step Scenario Walkthrough:

#### Step 1: Create Multiple Files
Three files are created with different allocation methods:
- `DocA.txt` (Contiguous)
- `SecretB.dat` (Linked)
- `ArchiveC.bin` (Indexed)

```
ID   Filename          Size(B)   Allocation  Blocks    Created                Block List
------------------------------------------------------------------------------------------
1    DocA.txt          65        Contiguous  2         2026-11-06 01:10:00   [0,1]
2    SecretB.dat       55        Linked      1         2026-11-06 01:10:00   [2]
3    ArchiveC.bin      61        Indexed     2         2026-11-06 01:10:00   [Idx:3 | 4]
```

#### Step 2: Soft-Delete `SecretB.dat`
The file is deleted. The disk map shows Sector #2 turning yellow `02?` (Deleted):
```
Legend: [ . ] Free (251)   [ ##] Allocated (4)   [##?] Deleted/Recoverable (1)
```

#### Step 3: Forensic Inspection of Raw Sector
Examining Sector #2 reveals that the text content `CONFIDENTIAL_TOP_SECRET...` is still physically present:
```
=== Block Inspector: Sector #2 ===
Status: DELETED (Contains recoverable residual data) | Owning File ID: 2
Raw Bytes (Hex Dump & ASCII):
Offset  Hex Data (16 bytes per line)             ASCII
------  ---------------------------------------  ----------------
0x0000  43 4f 4e 46 49 44 45 4e 54 49 41 4c 5f 54 4f 50  CONFIDENTIAL_TOP
0x0010  5f 53 45 43 52 45 54 5f 52 45 43 4f 56 45 52 59  _SECRET_RECOVERY
0x0020  5f 4b 45 59 5f 39 39 32 31 34 37 5f 4f 53 5f 50  _KEY_992147_OS_P
0x0030  41 53 53 43 4f 44 45 00 00 00 00 00 ff ff ff ff  ASSCODE.........
```
*(Notice the `ff ff ff ff` at offset 60, representing the `-1` linked terminator pointer!)*

#### Step 4 & 5: Forensic Recovery
The `RecoveryEngine` audits the blocks and restores `SecretB.dat`:
```
=== FORENSIC RECOVERY REPORT ===
Status: [SUCCESS - FULL RECOVERY]
Details: 100% Recoverable: All original disk sectors are completely untouched.
Sectors Recovered: 1 / 1
Carved / Recovered Payload:
┌────────────────────────────────────────────────────────────┐
CONFIDENTIAL_TOP_SECRET_RECOVERY_KEY_992147_OS_PASSCODE
└────────────────────────────────────────────────────────────┘
```
The file reappears in the active directory and its contents are read back with 100% fidelity.

#### Step 6 & 7: Overwrite Demonstration (Data Corruption)
`SecretB.dat` is deleted again. Then, a new file `Overwriter.bin` is created with a large payload that re-allocates and overwrites Sector #2.
When recovery is attempted now:
```
=== FORENSIC RECOVERY REPORT ===
Status: [PARTIAL RECOVERY - DATA CARVED WITH CORRUPTION] or [FAILED / UNRECOVERABLE]
Details: Unrecoverable: All original disk sectors have been overwritten by newer files.
Sectors Overwritten: 1 / 1
```
This clearly demonstrates to students why recovery tools fail once new writes occur on a storage drive.

---

## Persistence & Binary Disk Image

All disk sectors and filesystem metadata are saved persistently to disk:
1. `virtual_disk.bin`: Binary image containing the magic header `DAFR` (0x44414652), version number, block geometry, sector allocation statuses, owner file IDs, and raw block payloads.
2. `fs_metadata.bin`: Binary serialization of the directory table, inode structures, allocation types, and timestamps.

You can close the application, reopen it, and your virtual filesystem state will persist exactly as left. To start fresh, select Option `[13]` (Format / Reset Virtual Disk).

