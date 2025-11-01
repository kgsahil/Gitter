#include "cli/commands/CheckoutCommand.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <set>
#include <string>

#include "core/Repository.hpp"
#include "core/CommitObject.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"
#include "core/TreeBuilder.hpp"
#include "util/Sha1Hasher.hpp"

namespace gitter {

namespace {

/**
 * @brief Recursively restore files from tree object to working directory
 */
void restoreTree(
    const std::filesystem::path& root,
    const std::filesystem::path& basePath,
    ObjectStore& store,
    Index& index,
    const std::string& treeHash
) {
    if (treeHash.empty()) {
        return;
    }
    
    // Read tree entries
    std::vector<TreeEntry> entries;
    try {
        entries = store.readTree(treeHash);
    } catch (const std::exception& e) {
        // If tree doesn't exist or is invalid, skip silently
        return;
    }
    
    // Restore each entry
    for (const auto& entry : entries) {
        std::filesystem::path entryPath = basePath / entry.name;
        
        if (entry.isTree) {
            // Directory: recursively restore subtree
            std::filesystem::path dirPath = root / entryPath;
            std::error_code ec;
            std::filesystem::create_directories(dirPath, ec);
            if (!ec) {
                restoreTree(root, entryPath, store, index, entry.hashHex);
            }
        } else {
            // File: restore blob content
            try {
                std::string blobContent = store.readBlob(entry.hashHex);
                std::filesystem::path filePath = root / entryPath;
                
                // Create parent directories if needed
                std::error_code ec;
                std::filesystem::create_directories(filePath.parent_path(), ec);
                
                // Write file
                std::ofstream out(filePath, std::ios::binary);
                if (out) {
                    out.write(blobContent.data(), static_cast<std::streamsize>(blobContent.size()));
                    out.close();
                    
                    // Add to index
                    IndexEntry indexEntry;
                    indexEntry.path = entryPath.string();
                    indexEntry.hashHex = entry.hashHex;
                    indexEntry.sizeBytes = blobContent.size();
                    indexEntry.mode = entry.mode;
                    
                    // Set mtime and ctime to current time
                    auto now = std::chrono::system_clock::now();
                    auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
                    indexEntry.mtimeNs = static_cast<uint64_t>(nowNs);
                    indexEntry.ctimeNs = static_cast<uint64_t>(nowNs);
                    
                    index.addOrUpdate(indexEntry);
                }
            } catch (const std::exception&) {
                // Skip files that can't be restored
            }
        }
    }
}

/**
 * @brief Recursively remove empty directories from a given path
 */
void removeEmptyDirs(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return;
    }
    
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec) && !ec) {
        for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
            if (!ec) {
                removeEmptyDirs(entry.path());
            }
        }
        
        // Try to remove if now empty
        std::filesystem::remove(path, ec);
    }
}

/**
 * @brief Recursively collect all file paths from a tree object
 */
void collectTreeFiles(ObjectStore& store, const std::string& treeHash, 
                      std::filesystem::path basePath, std::set<std::string>& files) {
    if (treeHash.empty()) {
        return;
    }
    
    try {
        std::vector<TreeEntry> entries = store.readTree(treeHash);
        for (const auto& entry : entries) {
            std::filesystem::path entryPath = basePath / entry.name;
            
            if (entry.isTree) {
                // Directory: recursively collect files
                collectTreeFiles(store, entry.hashHex, entryPath, files);
            } else {
                // File: add to set
                files.insert(entryPath.generic_string());
            }
        }
    } catch (const std::exception&) {
        // If tree can't be read, skip silently
    }
}

} // anonymous namespace

Expected<void> CheckoutCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    // Parse arguments
    bool createBranch = false;
    std::string branchName;
    
    if (args.empty()) {
        return Error{ErrorCode::InvalidArgs, "checkout: branch name required"};
    }
    
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-b" && i + 1 < args.size()) {
            createBranch = true;
            branchName = args[i + 1];
            ++i;
        } else if (args[i][0] != '-') {
            branchName = args[i];
        }
    }
    
    // Find repository root
    auto rootRes = Repository::instance().discoverRoot(std::filesystem::current_path());
    if (!rootRes) return Error{rootRes.error().code, rootRes.error().message};
    std::filesystem::path root = rootRes.value();
    
    // Get current HEAD to use as starting point
    auto headRes = Repository::resolveHEAD(root);
    if (!headRes) {
        return Error{ErrorCode::InvalidArgs, "checkout: no commits yet"};
    }
    auto [currentHash, currentBranchRef] = headRes.value();
    
    // Create branch
    if (createBranch) {
        // Check if branch already exists by ref file
        auto existsRes = Repository::branchExists(root, branchName);
        if (!existsRes) {
            return Error{existsRes.error().code, existsRes.error().message};
        }
        
        if (existsRes.value()) {
            return Error{ErrorCode::InvalidArgs, 
                std::string("checkout: a branch named '") + branchName + "' already exists"};
        }
        
        // Create new branch at current commit (even if empty - matches Git behavior)
        auto createRes = Repository::createBranch(root, branchName, currentHash);
        if (!createRes) {
            return Error{createRes.error().code, createRes.error().message};
        }
        
        // Switch to new branch
        auto switchRes = Repository::switchToBranch(root, branchName);
        if (!switchRes) {
            return Error{switchRes.error().code, switchRes.error().message};
        }
        
        // Output success message
        std::cout << "Switched to a new branch '" << branchName << "'\n";
        
    } else {
        // Switching to existing branch - requires commits
        if (currentHash.empty()) {
            return Error{ErrorCode::InvalidArgs, "checkout: no commits yet"};
        }
        // Switch to existing branch
        auto existsRes = Repository::branchExists(root, branchName);
        if (!existsRes) {
            return Error{existsRes.error().code, existsRes.error().message};
        }
        
        if (!existsRes.value()) {
            return Error{ErrorCode::InvalidArgs, 
                std::string("checkout: '") + branchName + "' does not exist"};
        }
        
        // Read target branch commit
        auto commitRes = Repository::getBranchCommit(root, branchName);
        if (!commitRes) {
            return Error{commitRes.error().code, commitRes.error().message};
        }
        std::string targetCommitHash = commitRes.value();
        if (targetCommitHash.empty()) {
            return Error{ErrorCode::InvalidArgs, 
                std::string("checkout: branch '") + branchName + "' has no commits"};
        }
        
        // Read target commit to get tree hash
        ObjectStore store(root);
        CommitObject targetCommit;
        try {
            targetCommit = store.readCommit(targetCommitHash);
        } catch (const std::exception& e) {
            return Error{ErrorCode::InvalidArgs, 
                std::string("checkout: failed to read target commit: ") + e.what()};
        }
        
        // Read current commit's tree to identify files to remove
        CommitObject currentCommit;
        std::string currentTreeHash;
        if (!currentHash.empty()) {
            try {
                currentCommit = store.readCommit(currentHash);
                currentTreeHash = currentCommit.treeHash;
            } catch (const std::exception& e) {
                // If current commit can't be read, use empty tree
                currentTreeHash = "";
            }
        }
        
        // Read current commit's tree entries (recursively)
        std::set<std::string> currentTreeFiles;
        if (!currentTreeHash.empty()) {
            collectTreeFiles(store, currentTreeHash, std::filesystem::path(""), currentTreeFiles);
        }
        
        // Read target tree entries
        std::set<std::string> targetTreeFiles;
        collectTreeFiles(store, targetCommit.treeHash, std::filesystem::path(""), targetTreeFiles);
        
        // Restore working tree from commit's tree
        Index tempIndex;
        tempIndex.load(root);
        tempIndex.clear();
        restoreTree(root, std::filesystem::path(""), store, tempIndex, targetCommit.treeHash);
        const auto& targetEntries = tempIndex.entries();
        
        // Load current index and update it to match target branch
        Index index;
        index.load(root);
        
        // Get set of current index entries
        std::set<std::string> currentIndexFiles;
        for (const auto& kv : index.entries()) {
            currentIndexFiles.insert(kv.first);
        }
        
        // Remove entries that exist in index but not in target branch
        // BUT only if they're tracked in the current commit (to preserve staged new files)
        std::vector<std::string> keysToRemove;
        for (const auto& kv : index.entries()) {
            if (targetEntries.find(kv.first) == targetEntries.end()) {
                // Only remove if file is in current commit tree (not a new staged file)
                if (currentTreeFiles.find(kv.first) != currentTreeFiles.end()) {
                    keysToRemove.push_back(kv.first);
                }
            }
        }
        for (const auto& key : keysToRemove) {
            index.remove(key);
        }
        
        // Add or update entries from target branch (but don't overwrite existing entries)
        for (const auto& kv : targetEntries) {
            // Only add if not already in index (preserve staged changes)
            if (currentIndexFiles.find(kv.first) == currentIndexFiles.end()) {
                index.addOrUpdate(kv.second);
            }
        }
        
        // Remove files that exist in current commit's tree but not in target commit's tree
        for (const auto& oldPath : currentTreeFiles) {
            if (targetTreeFiles.find(oldPath) == targetTreeFiles.end()) {
                // File exists in current tree but not in target tree - remove from working tree
                std::filesystem::path filePath = root / oldPath;
                std::error_code ec;
                std::filesystem::remove(filePath, ec);
                // Ignore errors (file might not exist, or might be a directory)
                
                // Try to remove parent directories if they become empty
                if (!ec) {
                    removeEmptyDirs(filePath.parent_path());
                }
            }
        }
        
        // Save index
        if (!index.save(root)) {
            return Error{ErrorCode::IoError, "checkout: failed to save index"};
        }
        
        // Switch to branch (update HEAD)
        auto switchRes = Repository::switchToBranch(root, branchName);
        if (!switchRes) {
            return Error{switchRes.error().code, switchRes.error().message};
        }
        
        // Output success message
        std::cout << "Switched to branch '" << branchName << "'\n";
    }
    
    return {};
}

}


