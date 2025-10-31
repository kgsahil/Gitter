#include "cli/commands/CatFileCommand.hpp"

#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <vector>

#include "core/Repository.hpp"
#include "core/ObjectStore.hpp"
#include "core/CommitObject.hpp"
#include "core/Constants.hpp"
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

namespace gitter {

/**
 * @brief Display blob object content
 */
static void showBlob(ObjectStore& store, const std::string& hash) {
    try {
        std::string content = store.readObject(hash);
        
        // Find header end (type size\0)
        size_t headerEnd = content.find('\0');
        if (headerEnd == std::string::npos) {
            std::cerr << "Invalid blob format\n";
            return;
        }
        
        // Extract content after header
        std::string blobContent = content.substr(headerEnd + 1);
        std::cout << blobContent;
    } catch (const std::exception& e) {
        std::cerr << "Error reading blob: " << e.what() << "\n";
    }
}

/**
 * @brief Display tree object entries
 */
static void showTree(ObjectStore& store, const std::string& hash) {
    try {
        std::string content = store.readObject(hash);
        
        // Find header end (type size\0)
        size_t headerEnd = content.find('\0');
        if (headerEnd == std::string::npos) {
            std::cerr << "Invalid tree format\n";
            return;
        }
        
        // Extract tree entries after header
        std::string treeData = content.substr(headerEnd + 1);
        
        // Parse tree entries: <mode> <name>\0<binary-hash>
        size_t pos = 0;
        while (pos < treeData.length()) {
            // Read mode (space-terminated)
            size_t modeEnd = treeData.find(' ', pos);
            if (modeEnd == std::string::npos) break;
            std::string modeStr = treeData.substr(pos, modeEnd - pos);
            pos = modeEnd + 1;
            
            // Read name (null-terminated)
            size_t nameEnd = treeData.find('\0', pos);
            if (nameEnd == std::string::npos) break;
            std::string name = treeData.substr(pos, nameEnd - pos);
            pos = nameEnd + 1;
            
            // Read hash (20 bytes binary, convert to hex)
            if (pos + 20 > treeData.length()) break;
            std::vector<uint8_t> hashBytes(20);
            std::memcpy(hashBytes.data(), treeData.data() + pos, 20);
            std::string hashHex = IHasher::toHex(hashBytes);
            pos += 20;
            
            // Format output like Git: <mode> <type> <hash>\t<name>
            // Determine type from mode
            std::string type;
            uint32_t mode = static_cast<uint32_t>(std::stoul(modeStr, nullptr, 8));
            if ((mode & 0170000) == 0040000) {
                type = "tree";
            } else if ((mode & 0170000) == 0100000) {
                type = "blob";
            } else {
                type = "blob"; // Default
            }
            
            std::cout << std::setw(6) << std::setfill('0') << std::oct << mode << std::dec
                      << " " << type << " " << hashHex << "\t" << name << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error reading tree: " << e.what() << "\n";
    }
}

/**
 * @brief Display commit object content
 */
static void showCommit(ObjectStore& store, const std::string& hash) {
    try {
        CommitObject commit = store.readCommit(hash);
        
        std::cout << "tree " << commit.treeHash << "\n";
        
        for (const auto& parent : commit.parentHashes) {
            std::cout << "parent " << parent << "\n";
        }
        
        std::cout << "author " << commit.authorName << " <" << commit.authorEmail << "> "
                  << commit.authorTimestamp << " " << commit.authorTimezone << "\n";
        std::cout << "committer " << commit.committerName << " <" << commit.committerEmail << "> "
                  << commit.committerTimestamp << " " << commit.committerTimezone << "\n";
        std::cout << "\n";
        std::cout << commit.message;
        if (!commit.message.empty() && commit.message.back() != '\n') {
            std::cout << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error reading commit: " << e.what() << "\n";
    }
}

/**
 * @brief Get object type from content
 */
static std::string getObjectType(ObjectStore& store, const std::string& hash) {
    try {
        std::string content = store.readObject(hash);
        size_t headerEnd = content.find('\0');
        if (headerEnd == std::string::npos) {
            return "unknown";
        }
        std::string header = content.substr(0, headerEnd);
        
        if (header.rfind("blob ", 0) == 0) return "blob";
        if (header.rfind("tree ", 0) == 0) return "tree";
        if (header.rfind("commit ", 0) == 0) return "commit";
        return "unknown";
    } catch (const std::exception&) {
        return "unknown";
    }
}

/**
 * @brief Get object size from content
 */
static size_t getObjectSize(ObjectStore& store, const std::string& hash) {
    try {
        std::string content = store.readObject(hash);
        size_t headerEnd = content.find('\0');
        if (headerEnd == std::string::npos) {
            return 0;
        }
        std::string header = content.substr(0, headerEnd);
        
        // Extract size from header: "type <size>"
        size_t spacePos = header.find(' ');
        if (spacePos == std::string::npos) {
            return 0;
        }
        
        std::string sizeStr = header.substr(spacePos + 1);
        return static_cast<size_t>(std::stoull(sizeStr));
    } catch (const std::exception&) {
        return 0;
    }
}

Expected<void> CatFileCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    if (args.empty()) {
        return Error{ErrorCode::InvalidArgs, "cat-file: missing argument"};
    }
    
    // Find repository root
    auto rootRes = Repository::instance().discoverRoot(fs::current_path());
    if (!rootRes) {
        return Error{rootRes.error().code, rootRes.error().message};
    }
    fs::path root = rootRes.value();
    
    ObjectStore store(root);
    
    // Check for -t or -s flags
    if (args[0] == "-t") {
        if (args.size() < 2) {
            return Error{ErrorCode::InvalidArgs, "cat-file -t: missing hash"};
        }
        std::string hash = args[1];
        if (hash.length() != Constants::SHA1_HEX_LENGTH) {
            return Error{ErrorCode::InvalidArgs, "Invalid hash length (expected 40 characters)"};
        }
        
        std::string type = getObjectType(store, hash);
        std::cout << type << "\n";
        return {};
    }
    
    if (args[0] == "-s") {
        if (args.size() < 2) {
            return Error{ErrorCode::InvalidArgs, "cat-file -s: missing hash"};
        }
        std::string hash = args[1];
        if (hash.length() != Constants::SHA1_HEX_LENGTH) {
            return Error{ErrorCode::InvalidArgs, "Invalid hash length (expected 40 characters)"};
        }
        
        size_t size = getObjectSize(store, hash);
        std::cout << size << "\n";
        return {};
    }
    
    // Regular usage: cat-file <type> <hash>
    if (args.size() < 2) {
        return Error{ErrorCode::InvalidArgs, "cat-file: missing type or hash"};
    }
    
    std::string type = args[0];
    std::string hash = args[1];
    
    if (hash.length() != Constants::SHA1_HEX_LENGTH) {
        return Error{ErrorCode::InvalidArgs, "Invalid hash length (expected 40 characters)"};
    }
    
    if (type == "blob") {
        showBlob(store, hash);
    } else if (type == "tree") {
        showTree(store, hash);
    } else if (type == "commit") {
        showCommit(store, hash);
    } else {
        return Error{ErrorCode::InvalidArgs, "Invalid object type. Use: blob, tree, or commit"};
    }
    
    return {};
}

}

