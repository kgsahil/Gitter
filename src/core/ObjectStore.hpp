#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace gitter {

class IHasher;

/**
 * @brief Git object storage manager
 * 
 * Manages the .gitter/objects/ directory where all Git objects (blobs, trees, commits)
 * are stored in content-addressable format. Each object is identified by its hash (SHA-1 or SHA-256).
 * 
 * Git Object Format:
 *   Objects are stored as: "<type> <size>\0<content>"
 *   For blobs: "blob 12\0file content"
 *   
 * Storage Layout (Git standard):
 *   .gitter/objects/<first-2-chars>/<remaining-38-chars>
 *   Example: hash "abc123..." → .gitter/objects/ab/c123...
 *   
 * Compression:
 *   Objects are zlib-compressed before writing to disk (Git standard).
 * 
 * Strategy Pattern:
 *   Uses IHasher interface to support both SHA-1 (Git default) and SHA-256.
 */
class ObjectStore {
public:
    /**
     * @param repoRoot Path to repository root
     * @param hasher Hash algorithm to use (defaults to SHA-1 if nullptr)
     */
    explicit ObjectStore(const std::filesystem::path& repoRoot, std::unique_ptr<IHasher> hasher = nullptr);
    
    /// Destructor (must be defined in .cpp to allow incomplete IHasher type)
    ~ObjectStore();
    
    // Disable copy (unique_ptr is not copyable)
    ObjectStore(const ObjectStore&) = delete;
    ObjectStore& operator=(const ObjectStore&) = delete;
    
    // Allow move
    ObjectStore(ObjectStore&&) noexcept;
    ObjectStore& operator=(ObjectStore&&) noexcept;
    
    /// Returns path to .gitter/objects directory
    std::filesystem::path objectsDir() const;

    /**
     * @brief Write blob object from raw content
     * @param bytes Raw file content
     * @return Hash (hex) of the Git blob object
     * 
     * Creates Git blob object: "blob <size>\0<content>"
     * Compresses with zlib and stores in .gitter/objects/<aa>/<bbbb...>
     */
    std::string writeBlob(const std::string& bytes);
    
    /**
     * @brief Write blob object from file
     * @param filePath Path to file to hash and store
     * @return Hash (hex) of the Git blob object
     */
    std::string writeBlobFromFile(const std::filesystem::path& filePath);
    
    /**
     * @brief Compute Git blob hash for file without storing
     * @param filePath Path to file to hash
     * @return Hash (hex) as Git would compute it
     * 
     * Used by status command to detect modifications by comparing
     * working tree file hash against index-recorded hash
     */
    std::string hashFileContent(const std::filesystem::path& filePath);
    
    /**
     * @brief Read and decompress object from storage
     * @param hash Object hash (hex)
     * @return Decompressed object content (including header)
     */
    std::string readObject(const std::string& hash);

private:
    std::filesystem::path root;       // Repository root path
    std::unique_ptr<IHasher> hasher;  // Hash algorithm (SHA-1/SHA-256)
    
    /// Get path for object: .gitter/objects/<aa>/<bbbb...>
    std::filesystem::path getObjectPath(const std::string& hash) const;
};

}


