#pragma once

#include <string>
#include <vector>

namespace gitter {

/**
 * @brief Parsed Git commit object
 * 
 * Represents a commit with all its metadata extracted from the
 * Git object format.
 * 
 * Git commit format:
 *   commit <size>\0tree <hash>
 *   parent <hash>
 *   author Name <email> <timestamp> <timezone>
 *   committer Name <email> <timestamp> <timezone>
 *   
 *   <commit message>
 */
struct CommitObject {
    std::string hash;              // SHA-1 hash of this commit
    std::string treeHash;          // Tree object hash
    std::vector<std::string> parentHashes;  // Parent commit hashes (0 for root, 1+ for merges)
    std::string authorName;        // Author name
    std::string authorEmail;       // Author email
    int64_t authorTimestamp{0};    // Unix timestamp
    std::string authorTimezone;    // Timezone (+0000, -0800, etc.)
    std::string committerName;     // Committer name
    std::string committerEmail;    // Committer email
    int64_t committerTimestamp{0}; // Unix timestamp
    std::string committerTimezone; // Timezone
    std::string message;           // Full commit message
    
    /**
     * @brief Get short commit message (first line only)
     */
    std::string shortMessage() const {
        size_t newlinePos = message.find('\n');
        if (newlinePos != std::string::npos) {
            return message.substr(0, newlinePos);
        }
        return message;
    }
    
    /**
     * @brief Get short hash (first 7 characters)
     */
    std::string shortHash() const {
        return hash.length() >= 7 ? hash.substr(0, 7) : hash;
    }
};

}

