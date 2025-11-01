#include "cli/commands/CheckoutCommand.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <filesystem>

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
        
        // Read commit to get tree hash
        ObjectStore store(root);
        CommitObject targetCommit;
        try {
            targetCommit = store.readCommit(targetCommitHash);
        } catch (const std::exception& e) {
            return Error{ErrorCode::InvalidArgs, 
                std::string("checkout: failed to read commit: ") + e.what()};
        }
        
        // Clear and rebuild index
        Index index;
        index.load(root);
        index.clear();
        
        // Restore working tree from commit's tree
        restoreTree(root, std::filesystem::path(""), store, index, targetCommit.treeHash);
        
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


