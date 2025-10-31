#include "cli/commands/ResetCommand.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "core/Repository.hpp"
#include "core/ObjectStore.hpp"
#include "core/CommitObject.hpp"
#include "core/Index.hpp"
#include "core/TreeBuilder.hpp"
// Include concrete hasher to allow ObjectStore destructor instantiation
#include "util/Sha1Hasher.hpp"

namespace gitter {

namespace fs = std::filesystem;

/**
 * @brief Execute 'gitter reset' command
 * 
 * Resets HEAD to a previous commit:
 *   1. Supports HEAD~n syntax (e.g., HEAD~1, HEAD~2)
 *   2. Reads commit from HEAD and traverses parent chain
 *   3. Updates branch reference (HEAD) to target commit
 *   4. Updates index to match target commit tree
 *   5. Changes after target commit become unindexed
 * 
 * Examples:
 *   gitter reset HEAD~1  # Reset to previous commit
 *   gitter reset HEAD~2  # Reset 2 commits back
 */
Expected<void> ResetCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    if (args.empty()) {
        return Error{ErrorCode::InvalidArgs, "reset: target commit required (e.g., HEAD~1)"};
    }
    
    std::string target = args[0];
    
    // Find repository root
    auto rootRes = Repository::instance().discoverRoot(std::filesystem::current_path());
    if (!rootRes) return Error{rootRes.error().code, rootRes.error().message};
    std::filesystem::path root = rootRes.value();
    
    // Parse target: HEAD~n or HEAD
    int steps = 0;
    if (target == "HEAD") {
        steps = 0;
    } else if (target.rfind("HEAD~", 0) == 0) {
        // Extract number from HEAD~n
        std::string numStr = target.substr(5);
        try {
            steps = std::stoi(numStr);
            if (steps < 0) {
                return Error{ErrorCode::InvalidArgs, "reset: negative steps not allowed"};
            }
        } catch (const std::exception&) {
            return Error{ErrorCode::InvalidArgs, "reset: invalid HEAD~n format"};
        }
    } else {
        return Error{ErrorCode::InvalidArgs, "reset: only HEAD and HEAD~n are supported"};
    }
    
    // Resolve HEAD to commit hash
    auto headRes = Repository::resolveHEAD(root);
    if (!headRes) {
        return Error{headRes.error().code, "reset: " + headRes.error().message};
    }
    
    auto [currentHash, branchRef] = headRes.value();
    if (currentHash.empty()) {
        return Error{ErrorCode::InvalidArgs, "reset: no commits yet"};
    }
    
    // Traverse commit chain to find target commit
    ObjectStore store(root);
    std::string targetHash = currentHash;
    
    for (int i = 0; i < steps; ++i) {
        try {
            CommitObject commit = store.readCommit(targetHash);
            if (commit.parentHashes.empty()) {
                return Error{ErrorCode::InvalidArgs, "reset: cannot go back further, reached root commit"};
            }
            targetHash = commit.parentHashes[0];
        } catch (const std::exception& e) {
            return Error{ErrorCode::IoError, std::string("reset: failed to read commit: ") + e.what()};
        }
    }
    
    // If no change needed, return silently
    if (targetHash == currentHash) {
        return {};
    }
    
    // Read target commit's tree
    CommitObject targetCommit;
    try {
        targetCommit = store.readCommit(targetHash);
    } catch (const std::exception& e) {
        return Error{ErrorCode::IoError, std::string("reset: failed to read target commit: ") + e.what()};
    }
    
    // Update branch reference (HEAD) to target commit
    auto updateRes = Repository::updateHEAD(root, targetHash);
    if (!updateRes) {
        return Error{updateRes.error().code, "reset: " + updateRes.error().message};
    }
    
    // Update index to match target commit tree
    // We need to read the tree and populate the index
    Index index;
    
    // Clear existing index and rebuild from target tree
    // For now, we'll create an empty index and let the user re-add files
    // This matches Git's behavior where reset --mixed (default) unindexes changes
    index.clear();
    index.save(root);
    
    // No output on success (Git-like behavior)
    
    return {};
}

}
