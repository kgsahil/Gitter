#pragma once

#include <filesystem>
#include <string>

namespace gitter {

/**
 * @brief Git object storage manager
 * 
 * Manages the .gitter/objects/ directory where all Git objects (blobs, trees, commits)
 * are stored in content-addressable format. Each object is identified by its SHA-256 hash.
 * 
 * Git Object Format:
 *   Objects are stored as: "<type> <size>\0<content>"
 *   For blobs: "blob 12\0file content"
 *   
 * Storage:
 *   .gitter/objects/<sha256-hash>
 *   
 * This mimics Git's loose object storage (without compression for simplicity).
 */
class ObjectStore {
public:
    explicit ObjectStore(const std::filesystem::path& repoRoot);
    
    /// Returns path to .gitter/objects directory
    std::filesystem::path objectsDir() const;

    /**
     * @brief Write blob object from raw content
     * @param bytes Raw file content
     * @return SHA-256 hash (hex) of the Git blob object
     * 
     * Creates Git blob object: "blob <size>\0<content>"
     * Stores in .gitter/objects/<hash>
     */
    std::string writeBlob(const std::string& bytes);
    
    /**
     * @brief Write blob object from file
     * @param filePath Path to file to hash and store
     * @return SHA-256 hash (hex) of the Git blob object
     */
    std::string writeBlobFromFile(const std::filesystem::path& filePath);
    
    /**
     * @brief Compute Git blob hash for file without storing
     * @param filePath Path to file to hash
     * @return SHA-256 hash (hex) as Git would compute it
     * 
     * Used by status command to detect modifications by comparing
     * working tree file hash against index-recorded hash
     */
    std::string hashFileContent(const std::filesystem::path& filePath);

private:
    std::filesystem::path root;  // Repository root path
};

}


