#pragma once

#include <cstdint>
#include <filesystem>

namespace gitter {

/**
 * @brief File metadata structure for Git-like file tracking
 * 
 * Stores size, modification time, and permissions needed for the index.
 */
struct FileMetadata {
    uint64_t sizeBytes{0};   // File size in bytes
    uint64_t mtimeNs{0};     // Last modification time in nanoseconds
    uint32_t mode{0};        // File permissions (0100644 regular, 0100755 executable)
    uint64_t ctimeNs{0};     // Creation time in nanoseconds
};

/**
 * @brief Read file metadata from filesystem
 * 
 * Extracts size, mtime, and permissions from a file path.
 * Converts filesystem permissions to Git octal mode format.
 * 
 * @param filePath Path to file
 * @return FileMetadata with size, mtime, and mode, or all zeros on error
 */
FileMetadata getFileMetadata(const std::filesystem::path& filePath);

}

