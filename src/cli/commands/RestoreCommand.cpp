#include "cli/commands/RestoreCommand.hpp"

#include <filesystem>
#include <iostream>
#include <algorithm>

#include "core/Repository.hpp"
#include "core/Index.hpp"
#include "util/PatternMatcher.hpp"

namespace fs = std::filesystem;

namespace gitter {

/**
 * @brief Execute 'gitter restore' command
 * 
 * Currently supports only: gitter restore --staged <file>...
 * 
 * Unstages files by removing them from the index (.gitter/index).
 * Does NOT modify the working tree - files remain with their current content.
 * 
 * This mimics 'git restore --staged' which undoes 'git add'.
 * 
 * Usage:
 *   gitter restore --staged file.txt      # Unstage single file
 *   gitter restore --staged src/*.cpp     # Unstage multiple files
 * 
 * Future: Support restoring working tree files from index/HEAD
 */
Expected<void> RestoreCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    if (args.empty()) {
        return Error{ErrorCode::InvalidArgs, "restore: missing pathspec or --staged flag"};
    }

    // Parse arguments: extract --staged flag and file paths
    bool staged = false;
    std::vector<std::string> paths;
    for (const auto& arg : args) {
        if (arg == "--staged") {
            staged = true;
        } else {
            paths.push_back(arg);
        }
    }

    if (!staged) {
        return Error{ErrorCode::InvalidArgs, "restore: only --staged is currently supported"};
    }

    if (paths.empty()) {
        return Error{ErrorCode::InvalidArgs, "restore: missing pathspec"};
    }

    // Find repository root
    auto rootRes = Repository::instance().discoverRoot(fs::current_path());
    if (!rootRes) return Error{rootRes.error().code, rootRes.error().message};
    fs::path root = rootRes.value();

    // Load index (staging area)
    Index index;
    if (!index.load(root)) return Error{ErrorCode::IoError, "Failed to read index"};

    // Remove specified paths from index (unstage)
    // Supports patterns like "*.txt" or "src/*.cpp"
    for (const auto& p : paths) {
        // Check if it's a glob pattern
        if (PatternMatcher::isPattern(p)) {
            auto matches = PatternMatcher::matchPathsInIndex(p, index.entries());
            if (matches.empty()) {
                std::cerr << "warning: no staged files match pattern: " << p << "\n";
            }
            for (const auto& matched : matches) {
                index.remove(matched);
                std::cout << "Unstaged: " << matched << "\n";
            }
            continue;
        }
        
        // Regular path
        fs::path abs = fs::absolute(p);
        std::error_code ec;
        fs::path rel = fs::relative(abs, root, ec);
        if (ec) rel = abs;
        std::string relStr = rel.generic_string();
        
        const auto& entries = index.entries();
        if (entries.find(relStr) != entries.end()) {
            index.remove(relStr);
            std::cout << "Unstaged: " << relStr << "\n";
        } else {
            std::cerr << "warning: path not in index: " << relStr << "\n";
        }
    }

    if (!index.save(root)) return Error{ErrorCode::IoError, "Failed to write index"};
    return {};
}

}

