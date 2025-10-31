#pragma once

#include <filesystem>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include "core/Index.hpp"

namespace gitter {

class ObjectStore;

/**
 * @brief Tree entry representing a file or subdirectory in a tree object
 * 
 * Git tree format:
 *   <mode> <name>\0<20-byte-sha1>
 * Example:
 *   100644 file.txt\0<binary-hash>
 *   040000 subdir\0<binary-hash>
 */
struct TreeEntry {
    uint32_t mode;          // File mode: 040000 (dir), 100644 (file), 100755 (executable)
    std::string name;       // Filename or directory name
    std::string hashHex;    // SHA-1 hash (40 hex chars)
    bool isTree;            // true if this is a subdirectory
};

/**
 * @brief Builds Git tree objects from index entries
 * 
 * Converts flat index structure into hierarchical tree objects:
 * - Groups files by directory
 * - Recursively builds tree for each directory
 * - Creates tree objects in Git format
 * 
 * Example:
 *   Index entries:
 *     src/main.cpp -> blob abc123
 *     src/util.cpp -> blob def456
 *     README.md    -> blob 789abc
 *   
 *   Creates trees:
 *     tree(root):    040000 src <tree-hash>\n100644 README.md <blob-hash>
 *     tree(src):     100644 main.cpp <blob-hash>\n100644 util.cpp <blob-hash>
 */
class TreeBuilder {
public:
    /**
     * @brief Build tree object from index entries
     * 
     * @param index Index containing staged files
     * @param store ObjectStore to write tree objects
     * @return Hash of root tree object
     * 
     * Process:
     *   1. Group index entries by directory path
     *   2. Recursively build trees from leaves to root
     *   3. Write each tree object to ObjectStore
     *   4. Return root tree hash
     */
    static std::string buildFromIndex(const Index& index, ObjectStore& store);

private:
    /**
     * @brief Build tree for a specific directory path
     * 
     * @param dirPath Directory path (empty for root)
     * @param entries All index entries
     * @param store ObjectStore to write tree objects
     * @return Hash of tree object for this directory
     */
    static std::string buildTree(
        const std::string& dirPath,
        const std::unordered_map<std::string, IndexEntry>& entries,
        ObjectStore& store
    );
    
    /**
     * @brief Collect direct children of a directory
     * 
     * For dirPath="src", finds:
     *   - Files: src/main.cpp -> "main.cpp"
     *   - Subdirs: src/util/helper.cpp -> "util" (as tree)
     * 
     * @param dirPath Parent directory path
     * @param entries All index entries
     * @return Vector of TreeEntry for direct children
     */
    static std::vector<TreeEntry> getDirectChildren(
        const std::string& dirPath,
        const std::unordered_map<std::string, IndexEntry>& entries,
        ObjectStore& store
    );
};

}

