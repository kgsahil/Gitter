#include "cli/commands/StatusCommand.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>

#include "core/Repository.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"
#include "util/Hasher.hpp"

namespace fs = std::filesystem;

namespace gitter {

/**
 * @brief Helper to read index file into a set of paths (legacy; unused now that Index class exists)
 */
static void readIndexLines(const fs::path& indexPath, std::unordered_set<std::string>& out) {
    std::ifstream in(indexPath, std::ios::binary);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) out.insert(line);
    }
}

/**
 * @brief Collect untracked files by scanning working tree
 * 
 * Recursively walks working directory and finds files not in the index.
 * Automatically skips .gitter/ directory.
 * 
 * @param root Repository root path
 * @param indexed Set of paths currently in index (staged files)
 * @param untracked Output vector of relative paths to untracked files
 */
static void collectUntracked(const fs::path& root, const std::unordered_set<std::string>& indexed, std::vector<std::string>& untracked) {
    fs::path gdir = root / ".gitter";
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        const auto& p = it->path();
        if (!it->is_regular_file(ec)) continue;
        if (p.lexically_normal().string().rfind(gdir.lexically_normal().string(), 0) == 0) continue;
        fs::path rel = fs::relative(p, root, ec);
        if (ec) rel = p;
        auto relStr = rel.generic_string();
        if (indexed.find(relStr) == indexed.end()) untracked.push_back(relStr);
    }
}

/**
 * @brief Execute 'gitter status' command
 * 
 * Shows the working tree status in three categories:
 * 
 * 1. Changes to be committed (staged):
 *    Files in the index that will be included in next commit
 * 
 * 2. Changes not staged for commit (modified/deleted):
 *    Tracked files that have been modified or deleted since staging
 *    Detection: Compare size/mtime first (fast), then hash if suspicious
 * 
 * 3. Untracked files:
 *    Files in working tree not present in index
 * 
 * This mimics 'git status' output format.
 */
Expected<void> StatusCommand::execute(const AppContext&, const std::vector<std::string>&) {
    // Find repository root
    auto rootRes = Repository::instance().discoverRoot(fs::current_path());
    if (!rootRes) return Error{rootRes.error().code, rootRes.error().message};
    fs::path root = rootRes.value();

    // Load current index (staging area)
    Index index;
    index.load(root);
    const auto& entries = index.entries();

    // Build set of staged paths for untracked detection
    std::unordered_set<std::string> stagedSet;
    for (const auto& kv : entries) stagedSet.insert(kv.first);

    //std::cout << "On branch main\n\n";

    // Untracked
    std::vector<std::string> untracked;
    collectUntracked(root, stagedSet, untracked);
    if (!untracked.empty()) {
        std::cout << "Untracked files:\n";
        for (const auto& p : untracked) std::cout << "  " << p << "\n";
        std::cout << "\n";
    }

    // Changes to be committed (staged)
    if (!entries.empty()) {
        std::cout << "Changes to be committed:\n";
        for (const auto& kv : entries) {
            std::cout << "  " << kv.second.path << "\n";
        }
        std::cout << "\n";
    }

    // Changes not staged for commit (modified/deleted)
    std::vector<std::string> modified;
    std::vector<std::string> deleted;
    std::error_code ec;
    for (const auto& kv : entries) {
        const auto& e = kv.second;
        fs::path p = root / e.path;
        if (!fs::exists(p, ec)) { deleted.push_back(e.path); continue; }
        uint64_t sizeBytes = static_cast<uint64_t>(fs::file_size(p, ec)); if (ec) sizeBytes = 0;
        uint64_t mtimeNs = 0; auto ft = fs::last_write_time(p, ec); if (!ec) { auto now_sys = std::chrono::system_clock::now(); auto now_file = fs::file_time_type::clock::now(); auto adj = ft - now_file + now_sys; mtimeNs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(adj.time_since_epoch()).count()); }
        bool suspect = (sizeBytes != e.sizeBytes) || (mtimeNs != e.mtimeNs);
        if (suspect) {
            // Hash to confirm using Git blob format
            ObjectStore store(root);
            std::string nowHash = store.hashFileContent(p);
            if (nowHash != e.hashHex) modified.push_back(e.path);
        }
    }
    if (!modified.empty() || !deleted.empty()) {
        std::cout << "Changes not staged for commit:\n";
        for (const auto& p : modified) std::cout << "  modified: " << p << "\n";
        for (const auto& p : deleted) std::cout << "  deleted:  " << p << "\n";
        std::cout << "\n";
    }


    if (entries.empty() && modified.empty() && deleted.empty() && untracked.empty()) {
        std::cout << "nothing to commit, working tree clean\n";
    }

    return {};
}

}


