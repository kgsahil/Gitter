#pragma once

/**
 * @brief Git-like constants used throughout the codebase
 * 
 * Centralizes magic numbers to improve maintainability and readability.
 */
namespace gitter {

namespace Constants {
    // Hash algorithm constants
    constexpr size_t SHA1_HEX_LENGTH = 40;        // SHA-1 produces 40-char hex strings
    constexpr size_t SHA256_HEX_LENGTH = 64;      // SHA-256 produces 64-char hex strings
    
    // Object storage structure
    constexpr size_t OBJECT_DIR_LENGTH = 2;       // First 2 chars of hash form directory name
    
    // Commit log limits
    constexpr size_t MAX_COMMIT_LOG = 10;          // Default max commits shown in log
    
    // File permissions (octal)
    constexpr uint32_t MODE_FILE = 0100644;       // Regular file (-rw-r--r--)
    constexpr uint32_t MODE_EXECUTABLE = 0100755; // Executable file (-rwxr-xr-x)
    constexpr uint32_t MODE_DIR = 0040000;        // Directory
}
}

