#include "cli/commands/StatusCommand.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/Repository.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"
#include "core/CommitObject.hpp"
#include "core/TreeBuilder.hpp"
// Include concrete hasher to allow ObjectStore destructor instantiation
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

namespace gitter {

/**
 * @brief Normalize path for consistent comparison with index
 * 
 * Uses same normalization as Index::normalizePath to ensure paths match.
 * Normalizes path to use forward slashes and removes unnecessary components
 * like "./" prefix. Ensures same file always has same path representation.
 */
static std::string normalizePathForStatus(const std::string& path) {
    fs::path p(path);
    std::string normalized = p.lexically_normal().generic_string();
    
    // Remove leading ./ if present (matches Index::normalizePath)
    if (normalized.length() >= 2 && normalized.substr(0, 2) == "./") {
        normalized = normalized.substr(2);
    }
    
    return normalized;
}

/**
 * @brief Collect untracked files by scanning working tree
 * 
 * Recursively walks working directory and finds files not in the index.
 * Automatically skips .gitter/ directory.
 * 
 * @param root Repository root path
 * @param indexed Set of paths currently in index (staged files)
 * @param untracked Output vector of relative paths to untracked files
 */
static void collectUntracked(const fs::path& root, const std::unordered_set<std::string>& indexed, std::vector<std::string>& untracked) {
    fs::path gdir = root / ".gitter";
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            // Log error but continue - don't break entire scan
            continue;
        }
        
        const auto& p = it->path();
        
        // Skip if not a regular file
        if (!it->is_regular_file(ec)) continue;
        
        // Skip .gitter directory
        if (p.lexically_normal().string().rfind(gdir.lexically_normal().string(), 0) == 0) continue;
        
        // Get relative path from repository root
        fs::path rel = fs::relative(p, root, ec);
        
        // If relative() fails, try manual calculation
        if (ec || rel.empty()) {
            // Fallback: try to get relative path manually
            std::string absStr = p.lexically_normal().string();
            std::string rootStr = root.lexically_normal().string();
            
            // Remove trailing slash from root for comparison
            if (!rootStr.empty() && rootStr.back() != '/') {
                rootStr += '/';
            }
            
            if (absStr.rfind(rootStr, 0) == 0) {
                rel = absStr.substr(rootStr.length());
            } else {
                // Can't determine relative path - skip this file
                continue;
            }
        }
        
        // Normalize path using same logic as Index
        std::string normalizedPath = normalizePathForStatus(rel.generic_string());
        
        // Check if file is tracked (in index)
        if (indexed.find(normalizedPath) == indexed.end()) {
            untracked.push_back(normalizedPath);
        }
    }
}

/**
 * @brief Execute 'gitter status' command
 * 
 * Shows the working tree status by comparing three states:
 * 
 * 1. HEAD commit (last committed state)
 * 2. Index (staging area)
 * 3. Working tree (current files)
 * 
 * Categories:
 * - Changes to be committed: Index differs from HEAD
 * - Changes not staged: Working tree differs from index
 * - Untracked files: In working tree but not in index
 */
Expected<void> StatusCommand::execute(const AppContext&, const std::vector<std::string>&) {
    // Find repository root
    auto rootRes = Repository::instance().discoverRoot(fs::current_path());
    if (!rootRes) return Error{rootRes.error().code, rootRes.error().message};
    fs::path root = rootRes.value();

    // Load current index (staging area)
    Index index;
    index.load(root);
    const auto& indexEntries = index.entries();

    ObjectStore store(root);
    
    // Check if HEAD commit exists
    bool hasCommits = false;
    std::string currentCommitHash;
    
    auto headRes = Repository::resolveHEAD(root);
    if (headRes) {
        currentCommitHash = headRes.value().first;
        hasCommits = !currentCommitHash.empty();
    }
    
    // Build set of all tracked paths (index)
    std::unordered_set<std::string> trackedPaths;
    for (const auto& kv : indexEntries) {
        trackedPaths.insert(kv.first);
    }
    
    // Collect untracked files
    std::vector<std::string> untracked;
    collectUntracked(root, trackedPaths, untracked);
    
    // Find changes to be committed (index vs HEAD)
    std::vector<std::string> staged;
    if (hasCommits) {
        try {
            // Build map of HEAD tree contents for comparison
            CommitObject commit = store.readCommit(currentCommitHash);
            std::unordered_map<std::string, std::string> headTree;  // path -> hash
            
            // Helper to recursively read tree and build map
            std::function<void(const std::string&, const std::string&)> readTreeRecursive = 
                [&](const std::string& treeHash, const std::string& basePath) {
                    if (treeHash.empty()) return;
                    
                    std::vector<TreeEntry> entries = store.readTree(treeHash);
                    for (const auto& entry : entries) {
                        std::string fullPath = basePath.empty() ? entry.name : basePath + "/" + entry.name;
                        if (entry.isTree) {
                            readTreeRecursive(entry.hashHex, fullPath);
                        } else {
                            headTree[fullPath] = entry.hashHex;
                        }
                    }
                };
            
            // Read HEAD tree recursively
            readTreeRecursive(commit.treeHash, "");
            
            // Compare index with HEAD to find staged changes
            for (const auto& kv : indexEntries) {
                const auto& indexEntry = kv.second;
                auto it = headTree.find(indexEntry.path);
                
                if (it == headTree.end()) {
                    // File not in HEAD - newly added
                    staged.push_back(indexEntry.path);
                } else if (it->second != indexEntry.hashHex) {
                    // File exists in HEAD but hash differs - modified
                    staged.push_back(indexEntry.path);
                }
                // If hash matches, file is unchanged - not staged
            }
        } catch (const std::exception&) {
            // Error comparing, assume all staged
            for (const auto& kv : indexEntries) {
                staged.push_back(kv.second.path);
            }
        }
    } else {
        // No commits yet - everything in index is staged
        for (const auto& kv : indexEntries) {
            staged.push_back(kv.second.path);
        }
    }
    
    // Find changes not staged (working tree vs index)
    std::vector<std::string> modified;
    std::vector<std::string> deleted;
    std::error_code ec;
    
    for (const auto& kv : indexEntries) {
        const auto& e = kv.second;
        fs::path p = root / e.path;
        
        if (!fs::exists(p, ec)) {
            deleted.push_back(e.path);
            continue;
        }
        
        // Fast check: size and mtime (Git's optimization - skip expensive hash if both match)
        uint64_t sizeBytes = static_cast<uint64_t>(fs::file_size(p, ec));
        if (ec) sizeBytes = 0;
        
        uint64_t mtimeNs = 0;
        auto ft = fs::last_write_time(p, ec);
        if (!ec) {
            auto now_sys = std::chrono::system_clock::now();
            auto now_file = fs::file_time_type::clock::now();
            auto adj = ft - now_file + now_sys;
            mtimeNs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(adj.time_since_epoch()).count());
        }
        
        // Git's optimization: If size AND mtime match, assume unchanged (skip expensive hash)
        // This works because:
        // 1. Modifying a file updates its mtime (filesystem guarantees this)
        // 2. If size differs, content definitely changed
        // 3. If both match, file is almost certainly unchanged (very high probability)
        // 4. This optimization is crucial for performance with large repositories
        
        bool sizeMatches = sizeBytes == e.sizeBytes;
        bool mtimeMatches = mtimeNs == e.mtimeNs;
        
        if (sizeMatches && mtimeMatches) {
            // Fast path: skip hash computation for performance
            // Note: In rare edge cases (same-size edits within same second, or filesystem
            // mtime granularity issues), a content change might be missed. However, this
            // matches Git's behavior and is acceptable for performance reasons.
            continue;
        }
        
        // Slow path: size or mtime differs, hash to confirm actual change
        try {
            std::string nowHash = store.hashFileContent(p);
            if (nowHash != e.hashHex) {
                modified.push_back(e.path);
            }
        } catch (const std::exception&) {
            // If we can't hash but fast check suggests change, assume modified
            modified.push_back(e.path);
        }
    }
    
    // Display results
    
    // 1. Untracked files first
    if (!untracked.empty()) {
        std::cout << "Untracked files:\n";
        for (const auto& p : untracked) {
            std::cout << "  " << p << "\n";
        }
        std::cout << "\n";
    }
    
    // 2. Changes to be committed (staged)
    if (!staged.empty()) {
        std::cout << "Changes to be committed:\n";
        for (const auto& p : staged) {
            std::cout << "  " << p << "\n";
        }
        std::cout << "\n";
    }
    
    // 3. Changes not staged for commit
    if (!modified.empty() || !deleted.empty()) {
        std::cout << "Changes not staged for commit:\n";
        for (const auto& p : modified) {
            std::cout << "  modified: " << p << "\n";
        }
        for (const auto& p : deleted) {
            std::cout << "  deleted:  " << p << "\n";
        }
        std::cout << "\n";
    }
    
    // Clean state message
    if (staged.empty() && modified.empty() && deleted.empty() && untracked.empty()) {
        std::cout << "nothing to commit, working tree clean\n";
    }
    
    return {};
}

}
