#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

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
    
    /**
     * @brief Check if a branch exists
     * @param root Repository root path
     * @param branchName Branch name to check
     * @return True if branch exists, false otherwise, or error
     * 
     * Checks if `.gitter/refs/heads/<branch-name>` file exists.
     */
    static Expected<bool> branchExists(const std::filesystem::path& root, const std::string& branchName);
    
    /**
     * @brief List all branch names
     * @param root Repository root path
     * @return Vector of branch names, or error
     * 
     * Reads `.gitter/refs/heads/` directory and returns all branch names.
     */
    static Expected<std::vector<std::string>> listBranches(const std::filesystem::path& root);
    
    /**
     * @brief Get current branch name
     * @param root Repository root path
     * @return Current branch name, or error
     * 
     * Parses `.gitter/HEAD` file and returns the branch name if attached.
     */
    static Expected<std::string> getCurrentBranch(const std::filesystem::path& root);
    
    /**
     * @brief Create a new branch reference
     * @param root Repository root path
     * @param branchName New branch name
     * @param commitHash Commit hash for the branch
     * @return Success or error
     * 
     * Creates a new branch reference at the specified commit hash.
     */
    static Expected<void> createBranch(const std::filesystem::path& root, const std::string& branchName, const std::string& commitHash);
    
    /**
     * @brief Switch HEAD to a specific branch
     * @param root Repository root path
     * @param branchName Branch name to switch to
     * @return Success or error
     * 
     * Updates `.gitter/HEAD` to point to the specified branch.
     */
    static Expected<void> switchToBranch(const std::filesystem::path& root, const std::string& branchName);
    
    /**
     * @brief Get commit hash from a branch reference
     * @param root Repository root path
     * @param branchName Branch name to read
     * @return Commit hash, or empty string if ref doesn't exist
     * 
     * Reads `.gitter/refs/heads/<branch-name>` file and returns the commit hash.
     * Returns empty string if the ref file doesn't exist (no commits on that branch).
     */
    static Expected<std::string> getBranchCommit(const std::filesystem::path& root, const std::string& branchName);

private:
    Repository() = default;
    std::filesystem::path rootPath{};  // Repository root directory
    mutable std::mutex mtx;             // Thread-safety for init operations
};

}

 