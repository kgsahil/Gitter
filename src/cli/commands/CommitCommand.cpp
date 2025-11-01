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
#include "util/FileMetadata.hpp"
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
 *   -m <msg>  : Commit message (required, multiple allowed for multi-line)
 *   -a        : Auto-stage all modified tracked files (optional)
 *   -am <msg> : Combine -a and -m flags
 * 
 * Multiple -m flags create multi-paragraph messages separated by blank lines.
 * Example: `-m "First line" -m "Second paragraph"` creates:
 *   First line
 *   
 *   Second paragraph
 */
Expected<void> CommitCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    // Parse arguments: -m <message> (multiple allowed) and optional -a
    std::vector<std::string> messageParts;
    bool autoStage = false;
    
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-m" && i + 1 < args.size()) {
            messageParts.push_back(args[i + 1]);
            ++i;  // Skip next arg
        } else if (args[i] == "-a") {
            autoStage = true;
        } else if (args[i] == "-am" && i + 1 < args.size()) {
            autoStage = true;
            messageParts.push_back(args[i + 1]);
            ++i;
        }
    }
    
    if (messageParts.empty()) {
        return Error{ErrorCode::InvalidArgs, "commit: no commit message provided (-m required)"};
    }
    
    // Combine message parts with blank lines (Git behavior)
    // Each -m adds a new paragraph separated by a blank line
    std::string message;
    for (size_t i = 0; i < messageParts.size(); ++i) {
        if (i > 0) {
            message += "\n\n";  // Add blank line between paragraphs (two newlines)
        }
        message += messageParts[i];
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
            FileMetadata metadata = getFileMetadata(p);
            
            bool sizeMatches = metadata.sizeBytes == kv.second.sizeBytes;
            bool mtimeMatches = metadata.mtimeNs == kv.second.mtimeNs;
            
            // If file is modified, re-stage it
            if (!sizeMatches || !mtimeMatches) {
                try {
                    std::string hash = store.hashFileContent(p);
                    if (hash != kv.second.hashHex) {
                        // Update index entry with new hash and metadata
                        IndexEntry newEntry = kv.second;
                        newEntry.hashHex = hash;
                        newEntry.sizeBytes = metadata.sizeBytes;
                        newEntry.mtimeNs = metadata.mtimeNs;
                        newEntry.mode = metadata.mode;
                        newEntry.ctimeNs = metadata.ctimeNs;
                        
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
    auto headRes = Repository::resolveHEAD(root);
    if (headRes) {
        parentHash = headRes.value().first;
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
    auto updateRes = Repository::updateHEAD(root, commitHash);
    if (!updateRes) {
        return Error{updateRes.error().code, updateRes.error().message};
    }
    
    // No output on successful commit (Git-like behavior)
    
    return {};
}

}
