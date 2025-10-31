#pragma once

#include <filesystem>
#include <mutex>
#include <string>

#include "util/Expected.hpp"

namespace gitter {

/**
 * @brief Repository singleton - manages .gitter directory and global repo state
 * 
 * Provides high-level repository operations like initialization and root discovery.
 * Uses Singleton pattern to ensure a single instance manages the repository state
 * across all commands.
 * 
 * Repository layout:
 *   .gitter/
 *     HEAD              - Current branch reference (e.g., "ref: refs/heads/main")
 *     index             - Staging area (TSV format)
 *     objects/          - Content-addressable object storage
 *       <hash>          - Blob/tree/commit objects
 *     refs/
 *       heads/
 *         main          - Branch tip commit hash
 *         feature       - Other branches...
 * 
 * This mimics Git's repository structure.
 */
class Repository {
public:
    /// Get the global repository instance
    static Repository& instance();

    /**
     * @brief Initialize a new Gitter repository
     * @param path Directory to initialize (creates .gitter subdirectory)
     * @return Success or error if already initialized
     * 
     * Creates:
     *   - .gitter/objects/
     *   - .gitter/refs/heads/
     *   - .gitter/HEAD -> "ref: refs/heads/main"
     *   - .gitter/refs/heads/main (empty initially)
     */
    Expected<void> init(const std::filesystem::path& path);
    
    /**
     * @brief Find repository root by searching upwards for .gitter
     * @param start Starting directory (usually current working directory)
     * @return Absolute path to repository root, or error if not found
     * 
     * Walks up the directory tree until .gitter/ is found or root is reached.
     */
    Expected<std::filesystem::path> discoverRoot(const std::filesystem::path& start) const;

    /// Get repository root path (must call discoverRoot or init first)
    const std::filesystem::path& root() const { return rootPath; }
    
    /// Get .gitter directory path
    std::filesystem::path gitterDir() const { return rootPath / ".gitter"; }
    
    /**
     * @brief Resolve HEAD to commit hash
     * @param root Repository root path
     * @return Pair of (commitHash, branchRef), or error
     * 
     * Parses `.gitter/HEAD` and resolves to the actual commit hash.
     * Returns both the hash and the branch reference path (if attached).
     * Empty hash indicates no commits yet.
     */
    static Expected<std::pair<std::string, std::string>> resolveHEAD(const std::filesystem::path& root);
    
    /**
     * @brief Update HEAD to point to a commit
     * @param root Repository root path
     * @param commitHash Commit hash to set
     * @return Success or error
     * 
     * Writes the commit hash to the current branch reference file.
     */
    static Expected<void> updateHEAD(const std::filesystem::path& root, const std::string& commitHash);

private:
    Repository() = default;
    std::filesystem::path rootPath{};  // Repository root directory
    mutable std::mutex mtx;             // Thread-safety for init operations
};

}

 