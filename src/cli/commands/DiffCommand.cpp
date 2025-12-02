#include "cli/commands/DiffCommand.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Repository.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"
#include "core/CommitObject.hpp"
#include "core/TreeBuilder.hpp"
#include "core/DiffEngine.hpp"
#include "util/PathUtils.hpp"
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

namespace gitter {

/**
 * @brief Format and print a unified diff
 */
static void printUnifiedDiff(const std::string& filePath, 
                             const std::string& oldContent, 
                             const std::string& newContent,
                             const std::string& oldHash = "",
                             const std::string& newHash = "") {
    // File header
    std::cout << "diff --git a/" << filePath << " b/" << filePath << "\n";
    
    // Index line (if hashes provided)
    if (!oldHash.empty() && !newHash.empty()) {
        std::cout << "index " << oldHash.substr(0, 7) << ".." << newHash.substr(0, 7) << " 100644\n";
    }
    
    // Compute diff using DiffEngine
    auto diffResult = DiffEngine::computeDiff(oldContent, newContent);
    
    if (diffResult.hunks.empty()) {
        return; // No changes
    }
    
    // Print hunks
    for (const auto& hunk : diffResult.hunks) {
        // Hunk header: @@ -oldStart,oldLines +newStart,newLines @@
        std::cout << "@@ -" << hunk.oldStart;
        if (hunk.oldLines > 1) {
            std::cout << "," << hunk.oldLines;
        }
        std::cout << " +" << hunk.newStart;
        if (hunk.newLines > 1) {
            std::cout << "," << hunk.newLines;
        }
        std::cout << " @@\n";
        
        // Print lines
        for (const auto& line : hunk.lines) {
            switch (line.type) {
                case DiffEngine::DiffLine::Context:
                    std::cout << " " << line.content << "\n";
                    break;
                case DiffEngine::DiffLine::Addition:
                    std::cout << "+" << line.content << "\n";
                    break;
                case DiffEngine::DiffLine::Deletion:
                    std::cout << "-" << line.content << "\n";
                    break;
            }
        }
    }
}

/**
 * @brief Read file content from working tree
 */
static std::string readWorkingTreeFile(const fs::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

/**
 * @brief Build a map of path -> blob hash from a tree recursively
 */
static void buildTreeMap(ObjectStore& store, const std::string& treeHash, 
                         const std::string& basePath, 
                         std::unordered_map<std::string, std::string>& pathToHash) {
    if (treeHash.empty()) return;
    
    try {
        std::vector<TreeEntry> entries = store.readTree(treeHash);
        for (const auto& entry : entries) {
            std::string fullPath = basePath.empty() ? entry.name : basePath + "/" + entry.name;
            if (entry.isTree) {
                buildTreeMap(store, entry.hashHex, fullPath, pathToHash);
            } else {
                pathToHash[fullPath] = entry.hashHex;
            }
        }
    } catch (const std::exception&) {
        // Tree not found or error - skip
    }
}

Expected<void> DiffCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    // Find repository root
    auto rootRes = Repository::instance().discoverRoot(fs::current_path());
    if (!rootRes) {
        return Error{rootRes.error().code, rootRes.error().message};
    }
    fs::path root = rootRes.value();
    
    // Parse arguments
    bool cached = false;
    std::vector<std::string> unknownArgs;
    
    for (const auto& arg : args) {
        if (arg == "--cached" || arg == "--staged") {
            cached = true;
        } else if (!arg.empty() && arg[0] != '-') {
            unknownArgs.push_back(arg);
        }
    }
    
    // TODO: Support commit comparisons (gitter diff <commit>, gitter diff <commit1> <commit2>)
    if (!unknownArgs.empty()) {
        std::cerr << "Error: Commit comparisons are not yet supported.\n";
        std::cerr << "TODO: Implement support for 'gitter diff <commit>' and 'gitter diff <commit1> <commit2>'\n";
        return Error{ErrorCode::InvalidArgs, "Commit comparisons not yet implemented"};
    }
    
    ObjectStore store(root);
    Index index;
    index.load(root);
    
    // Determine diff mode
    if (cached) {
        // Index vs HEAD
        auto headRes = Repository::resolveHEAD(root);
        if (!headRes || headRes.value().first.empty()) {
            std::cout << "No commits yet\n";
            return {};
        }
        
        std::string headHash = headRes.value().first;
        CommitObject commit = store.readCommit(headHash);
        
        // Build map of HEAD tree
        std::unordered_map<std::string, std::string> headTree;
        buildTreeMap(store, commit.treeHash, "", headTree);
        
        // Compare index with HEAD
        for (const auto& kv : index.entries()) {
            const auto& indexEntry = kv.second;
            auto it = headTree.find(indexEntry.path);
            
            if (it == headTree.end()) {
                // New file - show addition
                std::string newContent = store.readBlob(indexEntry.hashHex);
                printUnifiedDiff(indexEntry.path, "", newContent, "", indexEntry.hashHex);
            } else if (it->second != indexEntry.hashHex) {
                // Modified file
                std::string oldContent = store.readBlob(it->second);
                std::string newContent = store.readBlob(indexEntry.hashHex);
                printUnifiedDiff(indexEntry.path, oldContent, newContent, it->second, indexEntry.hashHex);
            }
        }
        
        // Check for deleted files (in HEAD but not in index)
        for (const auto& kv : headTree) {
            if (index.entries().find(kv.first) == index.entries().end()) {
                std::string oldContent = store.readBlob(kv.second);
                printUnifiedDiff(kv.first, oldContent, "", kv.second, "");
            }
        }
        
    } else {
        // Working tree vs index (default)
        for (const auto& kv : index.entries()) {
            const auto& indexEntry = kv.second;
            fs::path filePath = root / indexEntry.path;
            
            if (!fs::exists(filePath)) {
                // File deleted
                std::string oldContent = store.readBlob(indexEntry.hashHex);
                printUnifiedDiff(indexEntry.path, oldContent, "", indexEntry.hashHex, "");
                continue;
            }
            
            // Check if file changed
            std::string currentHash = store.hashFileContent(filePath);
            if (currentHash != indexEntry.hashHex) {
                std::string oldContent = store.readBlob(indexEntry.hashHex);
                std::string newContent = readWorkingTreeFile(filePath);
                printUnifiedDiff(indexEntry.path, oldContent, newContent, indexEntry.hashHex, currentHash);
            }
        }
    }
    
    return {};
}

}

