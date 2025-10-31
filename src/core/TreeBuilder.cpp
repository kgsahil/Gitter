#include "core/TreeBuilder.hpp"
#include "core/ObjectStore.hpp"
#include <algorithm>
#include <sstream>
#include <set>

namespace gitter {

namespace {
    // Convert hex string to binary bytes
    std::vector<uint8_t> hexToBytes(const std::string& hex) {
        std::vector<uint8_t> bytes;
        bytes.reserve(hex.length() / 2);
        for (size_t i = 0; i < hex.length(); i += 2) {
            uint8_t byte = static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16));
            bytes.push_back(byte);
        }
        return bytes;
    }
}

std::string TreeBuilder::buildFromIndex(const Index& index, ObjectStore& store) {
    const auto& entries = index.entries();
    if (entries.empty()) {
        // Empty tree - should we error or allow?
        // Git allows empty commits
        return "";
    }
    
    // Build root tree (empty path)
    return buildTree("", entries, store);
}

std::string TreeBuilder::buildTree(
    const std::string& dirPath,
    const std::unordered_map<std::string, IndexEntry>& entries,
    ObjectStore& store
) {
    // Get direct children of this directory
    std::vector<TreeEntry> children = getDirectChildren(dirPath, entries, store);
    
    if (children.empty()) {
        // This shouldn't happen in normal flow
        return "";
    }
    
    // Sort entries by name (Git requirement)
    std::sort(children.begin(), children.end(), 
              [](const TreeEntry& a, const TreeEntry& b) {
                  return a.name < b.name;
              });
    
    // Build tree object content
    // Format: <mode> <name>\0<20-byte-binary-hash>
    std::vector<uint8_t> treeContent;
    
    for (const auto& entry : children) {
        // Mode (as octal string, no leading 0)
        std::string modeStr = std::to_string(entry.mode);
        treeContent.insert(treeContent.end(), modeStr.begin(), modeStr.end());
        
        // Space
        treeContent.push_back(' ');
        
        // Name
        treeContent.insert(treeContent.end(), entry.name.begin(), entry.name.end());
        
        // Null terminator
        treeContent.push_back('\0');
        
        // Binary hash (20 bytes for SHA-1, 32 for SHA-256)
        std::vector<uint8_t> hashBytes = hexToBytes(entry.hashHex);
        treeContent.insert(treeContent.end(), hashBytes.begin(), hashBytes.end());
    }
    
    // Convert to string for ObjectStore
    std::string contentStr(reinterpret_cast<const char*>(treeContent.data()), treeContent.size());
    
    // Write tree object
    return store.writeTree(contentStr);
}

std::vector<TreeEntry> TreeBuilder::getDirectChildren(
    const std::string& dirPath,
    const std::unordered_map<std::string, IndexEntry>& entries,
    ObjectStore& store
) {
    std::vector<TreeEntry> children;
    std::set<std::string> seenSubdirs;
    
    // Determine prefix to match
    std::string prefix = dirPath.empty() ? "" : dirPath + "/";
    
    for (const auto& [path, entry] : entries) {
        // Skip if not in this directory
        if (!dirPath.empty() && path.find(prefix) != 0) {
            continue;
        }
        
        // Get relative path from current directory
        std::string relPath = dirPath.empty() ? path : path.substr(prefix.length());
        
        // Check if this is a direct child or in a subdirectory
        size_t slashPos = relPath.find('/');
        
        if (slashPos == std::string::npos) {
            // Direct child file
            TreeEntry te;
            te.mode = entry.mode;
            te.name = relPath;
            te.hashHex = entry.hashHex;
            te.isTree = false;
            children.push_back(te);
        } else {
            // In a subdirectory
            std::string subdirName = relPath.substr(0, slashPos);
            
            // Only process each subdirectory once
            if (seenSubdirs.find(subdirName) != seenSubdirs.end()) {
                continue;
            }
            seenSubdirs.insert(subdirName);
            
            // Recursively build tree for subdirectory
            std::string subdirPath = prefix + subdirName;
            std::string treeHash = buildTree(subdirPath, entries, store);
            
            if (!treeHash.empty()) {
                TreeEntry te;
                te.mode = 040000;  // Directory mode
                te.name = subdirName;
                te.hashHex = treeHash;
                te.isTree = true;
                children.push_back(te);
            }
        }
    }
    
    return children;
}

}

