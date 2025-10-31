#include "cli/commands/LogCommand.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>

#include "core/Repository.hpp"
#include "core/ObjectStore.hpp"
#include "core/CommitObject.hpp"
#include "core/Constants.hpp"
// Include concrete hasher to allow ObjectStore destructor instantiation
#include "util/Sha1Hasher.hpp"

namespace gitter {

/**
 * @brief Execute 'gitter log' command
 * 
 * Displays commit history in reverse chronological order (newest first).
 * Shows up to 10 commits by default.
 * 
 * For each commit displays:
 *   - Commit hash (yellow)
 *   - Author name and email
 *   - Date and time
 *   - Commit message (indented)
 */
Expected<void> LogCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    (void)args;  // No args for basic log implementation
    
    // Find repository root
    auto rootRes = Repository::instance().discoverRoot(std::filesystem::current_path());
    if (!rootRes) return Error{rootRes.error().code, rootRes.error().message};
    std::filesystem::path root = rootRes.value();
    
    // Resolve HEAD to commit hash
    auto headRes = Repository::resolveHEAD(root);
    if (!headRes) {
        // If HEAD file doesn't exist, no commits yet
        if (headRes.error().code == ErrorCode::InvalidArgs) {
            std::cout << "`your current branch does not have any commits yet`\n";
            return {};
        }
        return Error{headRes.error().code, headRes.error().message};
    }
    
    auto [currentHash, branchRef] = headRes.value();
    if (currentHash.empty()) {
        std::cout << "`your current branch does not have any commits yet`\n";
        return {};
    }
    
    // Traverse commit chain and display
    ObjectStore store(root);
    int maxCommits = Constants::MAX_COMMIT_LOG;
    int count = 0;
    
    while (!currentHash.empty() && count < maxCommits) {
        try {
            // Read and parse commit
            CommitObject commit = store.readCommit(currentHash);
            
            // Display commit (Git-style format)
            std::cout << "\033[33mcommit " << commit.hash << "\033[0m\n";  // Yellow
            std::cout << "Author: " << commit.authorName << " <" << commit.authorEmail << ">\n";
            
            // Format date
            std::time_t timestamp = static_cast<std::time_t>(commit.authorTimestamp);
            std::tm* timeinfo = std::localtime(&timestamp);
            char buffer[80];
            std::strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", timeinfo);
            std::cout << "Date:   " << buffer << " " << commit.authorTimezone << "\n";
            
            std::cout << "\n";
            
            // Display message (indented)
            std::istringstream iss(commit.message);
            std::string line;
            while (std::getline(iss, line)) {
                std::cout << "    " << line << "\n";
            }
            
            // Move to parent commit
            if (!commit.parentHashes.empty()) {
                currentHash = commit.parentHashes[0];  // Follow first parent (ignore merges for now)
            } else {
                currentHash.clear();  // Root commit, no more parents
            }
            
            ++count;
        } catch (const std::exception& e) {
            std::cerr << "Error reading commit " << currentHash << ": " << e.what() << "\n";
            break;
        }
    }
    
    if (count == 0) {
        std::cout << "`your current branch does not have any commits yet`\n";
    }
    
    return {};
}

}
