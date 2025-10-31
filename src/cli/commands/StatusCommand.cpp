#include "cli/commands/StatusCommand.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>

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
        if (ec) break;
        const auto& p = it->path();
        if (!it->is_regular_file(ec)) continue;
        if (p.lexically_normal().string().rfind(gdir.lexically_normal().string(), 0) == 0) continue;
        fs::path rel = fs::relative(p, root, ec);
        if (ec) rel = p;
        auto relStr = rel.generic_string();
        if (indexed.find(relStr) == indexed.end()) untracked.push_back(relStr);
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
    fs::path headPath = root / ".gitter" / "HEAD";
    bool hasCommits = false;
    std::string currentCommitHash;
    
    if (fs::exists(headPath)) {
        std::ifstream headFile(headPath);
        std::string headContent;
        std::getline(headFile, headContent);
        headFile.close();
        
        if (headContent.rfind("ref: ", 0) == 0) {
            std::string refPath = headContent.substr(5);
            fs::path refFile = root / ".gitter" / refPath;
            if (fs::exists(refFile)) {
                std::ifstream rf(refFile);
                std::getline(rf, currentCommitHash);
                hasCommits = !currentCommitHash.empty();
            }
        } else {
            currentCommitHash = headContent;
            hasCommits = !currentCommitHash.empty();
        }
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
        // If trees differ, there are staged changes
        try {
            std::string indexTreeHash = TreeBuilder::buildFromIndex(index, store);
            CommitObject commit = store.readCommit(currentCommitHash);
            
            if (indexTreeHash != commit.treeHash) {
                // Trees differ - show all index entries as staged
                for (const auto& kv : indexEntries) {
                    staged.push_back(kv.second.path);
                }
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
        
        // Fast check: size and mtime
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
        
        bool suspect = (sizeBytes != e.sizeBytes) || (mtimeNs != e.mtimeNs);
        if (suspect) {
            // Hash to confirm
            std::string nowHash = store.hashFileContent(p);
            if (nowHash != e.hashHex) {
                modified.push_back(e.path);
            }
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
