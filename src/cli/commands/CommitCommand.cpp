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
#include "core/CommitObject.hpp"
// Include concrete hasher to allow ObjectStore destructor instantiation
#include "util/Sha1Hasher.hpp"

namespace gitter {

namespace fs = std::filesystem;

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
    
    // Create ObjectStore instance (used for auto-staging and commit creation)
    ObjectStore store(root);
    
    // If -a flag, auto-stage all modified tracked files
    if (autoStage) {
        std::error_code ec;
        
        // Iterate through all tracked files in the index
        auto entries = index.entriesMut();
        for (auto& kv : entries) {
            const std::string& path = kv.first;
            fs::path p = root / path;
            
            // Skip if file doesn't exist (deleted files)
            if (!fs::exists(p, ec)) {
                continue;
            }
            
            // Check if file is modified using fast size/mtime check
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
            
            bool sizeMatches = sizeBytes == kv.second.sizeBytes;
            bool mtimeMatches = mtimeNs == kv.second.mtimeNs;
            
            // If file is modified, re-stage it
            if (!sizeMatches || !mtimeMatches) {
                try {
                    std::string hash = store.hashFileContent(p);
                    if (hash != kv.second.hashHex) {
                        // Get file permissions (Git tracks mode)
                        uint32_t mode = 0;
                        auto status = fs::status(p, ec);
                        if (!ec) {
                            auto perms = status.permissions();
                            // Git uses octal mode: 0100644 (regular file) or 0100755 (executable)
                            if ((perms & fs::perms::owner_exec) != fs::perms::none ||
                                (perms & fs::perms::group_exec) != fs::perms::none ||
                                (perms & fs::perms::others_exec) != fs::perms::none) {
                                mode = 0100755; // Executable
                            } else {
                                mode = 0100644; // Regular file
                            }
                        }
                        
                        // Update index entry with new hash and metadata
                        IndexEntry newEntry = kv.second;
                        newEntry.hashHex = hash;
                        newEntry.sizeBytes = sizeBytes;
                        newEntry.mtimeNs = mtimeNs;
                        newEntry.mode = mode;
                        newEntry.ctimeNs = mtimeNs;
                        
                        index.addOrUpdate(newEntry);
                    }
                } catch (const std::exception&) {
                    // Skip files we can't hash
                }
            }
        }
        
        // Save updated index
        index.save(root);
        
        // Reload index to get any auto-staged changes
        if (!index.load(root)) {
            return Error{ErrorCode::IoError, "Failed to read index"};
        }
    }
    
    // Check if index is empty
    if (index.entries().empty()) {
        return Error{ErrorCode::InvalidArgs, "nothing to commit (index is empty)"};
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
    
    // Create tree from index
    std::string treeHash;
    try {
        treeHash = TreeBuilder::buildFromIndex(index, store);
    } catch (const std::exception& e) {
        return Error{ErrorCode::IoError, std::string("Failed to create tree object: ") + e.what()};
    }
    
    if (treeHash.empty()) {
        return Error{ErrorCode::IoError, "Failed to create tree object: empty tree"};
    }
    
    // Check if tree hash is same as parent (nothing to commit)
    if (!parentHash.empty()) {
        try {
            CommitObject parentCommit = store.readCommit(parentHash);
            if (treeHash == parentCommit.treeHash) {
                return Error{ErrorCode::InvalidArgs, "nothing to commit, working tree clean"};
            }
        } catch (const std::exception&) {
            // If we can't read parent, proceed with commit
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
    
    // No output on successful commit (Git-like behavior)
    
    return {};
}

}
