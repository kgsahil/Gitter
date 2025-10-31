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
    
    // Read current HEAD to get starting commit
    std::filesystem::path headPath = root / ".gitter" / "HEAD";
    if (!std::filesystem::exists(headPath)) {
        std::cout << "`your current branch does not have any commits yet`\n";
        return {};
    }
    
    std::ifstream headFile(headPath);
    if (!headFile) {
        std::cout << "`your current branch does not have any commits yet`\n";
        return {};
    }
    std::string headContent;
    std::getline(headFile, headContent);
    if (headFile.bad()) {
        return Error{ErrorCode::IoError, "Failed to read HEAD file"};
    }
    headFile.close();
    
    // Resolve HEAD to commit hash
    std::string currentHash;
    if (headContent.rfind("ref: ", 0) == 0) {
        // HEAD points to a branch
        std::string refPath = headContent.substr(5);
        std::filesystem::path refFile = root / ".gitter" / refPath;
        
        if (!std::filesystem::exists(refFile)) {
            std::cout << "`your current branch does not have any commits yet`\n";
            return {};
        }
        
        std::ifstream rf(refFile);
        if (!rf) {
            std::cout << "`your current branch does not have any commits yet`\n";
            return {};
        }
        std::getline(rf, currentHash);
        if (rf.bad()) {
            return Error{ErrorCode::IoError, "Failed to read ref file"};
        }
    } else {
        // Detached HEAD (direct commit hash)
        currentHash = headContent;
    }
    
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
