#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace gitter {

/**
 * @brief Staging area entry for a single file
 * 
 * Tracks a file that has been added to the staging area (index).
 * Stores metadata used for fast dirty detection via mtime/size,
 * falling back to content hash comparison when needed.
 */
struct IndexEntry {
    std::string path;        // Path relative to repo root (e.g., "src/main.cpp")
    std::string hashHex;     // SHA-256 hash of Git blob object (64-char hex)
    uint64_t sizeBytes{0};   // File size in bytes (for fast dirty check)
    uint64_t mtimeNs{0};     // Last modification time in nanoseconds (for fast dirty check)
};

/**
 * @brief Git staging area (index) manager
 * 
 * The index stores files that have been staged via 'add' and will be included
 * in the next commit. Each entry tracks a file's path, blob hash, and metadata.
 * 
 * On-disk format (.gitter/index):
 *   TSV with one entry per line: path<TAB>hash<TAB>size<TAB>mtime
 *   Example: "src/main.cpp<TAB>a3b2c1...<TAB>1024<TAB>1234567890000000000"
 * 
 * This mimics Git's index (staging area) where files are prepared for commit.
 */
class Index {
public:
    /// Load index from .gitter/index (returns true even if file doesn't exist)
    bool load(const std::filesystem::path& repoRoot);
    
    /// Save index to .gitter/index (overwrites existing)
    bool save(const std::filesystem::path& repoRoot) const;

    /// Add or update an entry in the index (replaces if path exists)
    void addOrUpdate(const IndexEntry& entry);
    
    /// Remove an entry from the index by path
    void remove(const std::string& path);
    
    /// Get read-only access to all index entries
    const std::unordered_map<std::string, IndexEntry>& entries() const { return pathToEntry; }
    
    /// Get mutable access to entries (use with caution)
    std::unordered_map<std::string, IndexEntry>& entriesMut() { return pathToEntry; }

private:
    std::unordered_map<std::string, IndexEntry> pathToEntry;  // Map: relative path -> entry
};

}


