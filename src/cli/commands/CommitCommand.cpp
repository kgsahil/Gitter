#include "cli/commands/CommitCommand.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "core/Repository.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"
#include "core/TreeBuilder.hpp"
// Include concrete hasher to allow ObjectStore destructor instantiation
#include "util/Sha1Hasher.hpp"

namespace gitter {

/**
 * @brief Execute 'gitter commit' command
 * 
 * Creates a commit from the current index:
 *   1. Builds tree objects from staged files
 *   2. Creates commit object with metadata
 *   3. Updates current branch reference
 * 
 * Supports:
 *   -m <msg> : Commit message (required)
 *   -a       : Auto-stage all modified tracked files (optional)
 *   -am <msg>: Combine -a and -m flags
 */
Expected<void> CommitCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    // Parse arguments: -m <message> and optional -a
    std::string message;
    bool autoStage = false;
    
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-m" && i + 1 < args.size()) {
            message = args[i + 1];
            ++i;  // Skip next arg
        } else if (args[i] == "-a") {
            autoStage = true;
        } else if (args[i] == "-am" && i + 1 < args.size()) {
            autoStage = true;
            message = args[i + 1];
            ++i;
        }
    }
    
    if (message.empty()) {
        return Error{ErrorCode::InvalidArgs, "commit: no commit message provided (-m required)"};
    }
    
    // Find repository root
    auto rootRes = Repository::instance().discoverRoot(std::filesystem::current_path());
    if (!rootRes) return Error{rootRes.error().code, rootRes.error().message};
    std::filesystem::path root = rootRes.value();
    
    // Load index
    Index index;
    if (!index.load(root)) {
        return Error{ErrorCode::IoError, "Failed to read index"};
    }
    
    // If -a flag, auto-stage all modified tracked files
    if (autoStage) {
        // TODO: Implement auto-staging of modified files
        // For now, just use what's in the index
        std::cout << "warning: -a flag not fully implemented yet\n";
    }
    
    // Check if index is empty
    if (index.entries().empty()) {
        return Error{ErrorCode::InvalidArgs, "nothing to commit (index is empty)"};
    }
    
    // Create tree from index
    ObjectStore store(root);
    std::string treeHash;
    try {
        treeHash = TreeBuilder::buildFromIndex(index, store);
    } catch (const std::exception& e) {
        return Error{ErrorCode::IoError, std::string("Failed to create tree object: ") + e.what()};
    }
    
    if (treeHash.empty()) {
        return Error{ErrorCode::IoError, "Failed to create tree object: empty tree"};
    }
    
    // Read parent commit (current HEAD)
    std::string parentHash;
    std::filesystem::path headPath = root / ".gitter" / "HEAD";
    if (std::filesystem::exists(headPath)) {
        std::ifstream headFile(headPath);
        if (headFile) {
            std::string headContent;
            std::getline(headFile, headContent);
            
            // HEAD format: "ref: refs/heads/main"
            if (headContent.rfind("ref: ", 0) == 0) {
                std::string refPath = headContent.substr(5);
                std::filesystem::path refFile = root / ".gitter" / refPath;
                if (std::filesystem::exists(refFile)) {
                    std::ifstream rf(refFile);
                    if (rf) {
                        std::getline(rf, parentHash);
                    }
                }
            }
        }
    }
    
    // Build commit object
    std::ostringstream commitContent;
    commitContent << "tree " << treeHash << "\n";
    
    if (!parentHash.empty()) {
        commitContent << "parent " << parentHash << "\n";
    }
    
    // Author and committer (use environment or defaults)
    const char* authorName = std::getenv("GIT_AUTHOR_NAME");
    const char* authorEmail = std::getenv("GIT_AUTHOR_EMAIL");
    std::string author = authorName ? authorName : "Gitter User";
    std::string email = authorEmail ? authorEmail : "user@example.com";
    
    // Unix timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    commitContent << "author " << author << " <" << email << "> " << timestamp << " +0000\n";
    commitContent << "committer " << author << " <" << email << "> " << timestamp << " +0000\n";
    commitContent << "\n";
    commitContent << message << "\n";
    
    // Write commit object
    std::string commitHash;
    try {
        commitHash = store.writeCommit(commitContent.str());
    } catch (const std::exception& e) {
        return Error{ErrorCode::IoError, std::string("Failed to write commit object: ") + e.what()};
    }
    
    // Update HEAD (write to current branch ref)
    std::ifstream headFileRead(headPath);
    if (!headFileRead) {
        return Error{ErrorCode::IoError, "Failed to read HEAD file"};
    }
    std::string headContent;
    std::getline(headFileRead, headContent);
    headFileRead.close();
    
    if (headContent.rfind("ref: ", 0) == 0) {
        std::string refPath = headContent.substr(5);
        std::filesystem::path refFile = root / ".gitter" / refPath;
        
        // Create parent directories if needed
        std::error_code ec;
        std::filesystem::create_directories(refFile.parent_path(), ec);
        if (ec) {
            return Error{ErrorCode::IoError, "Failed to create ref directory: " + ec.message()};
        }
        
        // Write commit hash to branch ref
        std::ofstream rf(refFile, std::ios::binary);
        if (!rf) {
            return Error{ErrorCode::IoError, "Failed to write ref file"};
        }
        rf << commitHash << "\n";
        rf.flush();
        if (!rf || !rf.good()) {
            return Error{ErrorCode::IoError, "Failed to write commit hash to ref"};
        }
    }
    
    // Print success message
    std::cout << "[" << (parentHash.empty() ? "root-commit" : "commit") << " " 
              << commitHash.substr(0, 7) << "] " << message << "\n";
    
    return {};
}

}
