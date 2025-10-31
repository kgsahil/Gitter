#include "cli/commands/AddCommand.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

#include "core/Repository.hpp"
#include "core/ObjectStore.hpp"
#include "core/Index.hpp"
#include "util/PatternMatcher.hpp"

namespace fs = std::filesystem;

namespace gitter {

/**
 * @brief Helper to add a single file to the index
 * 
 * Reads file content, creates Git blob object, stores in object database,
 * and records entry (with hash, size, mtime) in the index.
 * 
 * @param filePath Absolute path to file
 * @param root Repository root path
 * @param store Object store to write blob
 * @param index Index to update with new entry
 */
static void addFileToIndex(const fs::path& filePath, const fs::path& root, ObjectStore& store, Index& index) {
    std::error_code ec;
    fs::path rel = fs::relative(filePath, root, ec);
    if (ec) rel = filePath;

    std::string hash = store.writeBlobFromFile(filePath);

    uint64_t sizeBytes = static_cast<uint64_t>(fs::file_size(filePath, ec));
    if (ec) sizeBytes = 0;
    uint64_t mtimeNs = 0;
    auto ft = fs::last_write_time(filePath, ec);
    if (!ec) {
        auto now_sys = std::chrono::system_clock::now();
        auto now_file = fs::file_time_type::clock::now();
        auto adj = ft - now_file + now_sys;
        mtimeNs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(adj.time_since_epoch()).count());
    }

    IndexEntry e;
    e.path = rel.generic_string();
    e.hashHex = hash;
    e.sizeBytes = sizeBytes;
    e.mtimeNs = mtimeNs;
    index.addOrUpdate(e);
}

/**
 * @brief Execute 'gitter add' command
 * 
 * Stages files for the next commit by:
 *   1. Computing Git blob hash for each file
 *   2. Storing blob in .gitter/objects/<hash>
 *   3. Recording path, hash, size, mtime in .gitter/index
 * 
 * Supports:
 *   - Individual files: gitter add file.txt
 *   - Multiple files: gitter add file1.txt file2.cpp
 *   - Directories (recursive): gitter add src/
 *   - Current directory: gitter add .
 * 
 * Automatically skips .gitter/ directory and non-existent paths.
 */
Expected<void> AddCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    if (args.empty()) {
        return Error{ErrorCode::InvalidArgs, "add: missing <pathspec>"};
    }

    // Find repository root
    auto rootRes = Repository::instance().discoverRoot(fs::current_path());
    if (!rootRes) return Error{rootRes.error().code, rootRes.error().message};
    fs::path root = rootRes.value();

    // Initialize object store and load current index
    ObjectStore store(root);
    Index index;
    if (!index.load(root)) return Error{ErrorCode::IoError, "Failed to read index"};

    // Precompute .gitter path to skip during traversal
    fs::path gdir = root / ".gitter";
    auto gdStr = fs::absolute(gdir).lexically_normal().string();

    // Process each pathspec (file, directory, or pattern)
    for (const auto& p : args) {
        // Check if it's a glob pattern
        if (PatternMatcher::isPattern(p)) {
            auto matches = PatternMatcher::matchFilesInWorkingTree(p, root, gdStr);
            if (matches.empty()) {
                std::cerr << "warning: no files match pattern: " << p << "\n";
            }
            for (const auto& matched : matches) {
                addFileToIndex(matched, root, store, index);
            }
            continue;
        }
        
        // Regular file or directory
        fs::path abs = fs::absolute(p);
        if (!fs::exists(abs)) {
            std::cerr << "warning: path does not exist: " << abs.string() << "\n";
            continue;
        }

        std::error_code ec;
        
        // If it's a directory, recursively add all files
        if (fs::is_directory(abs, ec)) {
            for (auto it = fs::recursive_directory_iterator(abs, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
                if (ec) break;
                const auto& entry = it->path();
                
                // Skip .gitter directory
                auto entryStr = entry.lexically_normal().string();
                if (entryStr.rfind(gdStr, 0) == 0) {
                    it.disable_recursion_pending();
                    continue;
                }
                
                if (fs::is_regular_file(entry, ec)) {
                    addFileToIndex(entry, root, store, index);
                }
            }
        } else if (fs::is_regular_file(abs, ec)) {
            // Skip anything under .gitter
            auto apStr = abs.lexically_normal().string();
            if (apStr.rfind(gdStr, 0) == 0) {
                std::cerr << "warning: skipping path inside .gitter: " << abs.string() << "\n";
                continue;
            }
            
            addFileToIndex(abs, root, store, index);
        }
    }

    if (!index.save(root)) return Error{ErrorCode::IoError, "Failed to write index"};
    return {};
}

}


