cmake_minimum_required(VERSION 3.16)
project(DigitalArchaeologist VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/FileMetadata.cpp
    src/VirtualDisk.cpp
    src/AllocationStrategy.cpp
    src/FileSystem.cpp
    src/RecoveryEngine.cpp
    src/main.cpp
)

# Headers (for IDE project navigation)
set(HEADERS
    include/Common.h
    include/FileMetadata.h
    include/VirtualDisk.h
    include/AllocationStrategy.h
    include/FileSystem.h
    include/RecoveryEngine.h
)

# Executable target
add_executable(DigitalArchaeologist ${SOURCES} ${HEADERS})

# Compiler warnings and options
if(MSVC)
    target_compile_options(DigitalArchaeologist PRIVATE /W4 /permissive-)
else()
    target_compile_options(DigitalArchaeologist PRIVATE -Wall -Wextra -Wpedantic)
endif()

