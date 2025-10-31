#include "core/Index.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace gitter {

static fs::path indexPathOf(const fs::path& root) { return root / ".gitter" / "index"; }

bool Index::load(const fs::path& repoRoot) {
    pathToEntry.clear();
    std::ifstream in(indexPathOf(repoRoot), std::ios::binary);
    if (!in) return true; // treat missing as empty
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        // TSV: path\thash\tsize\tmtime\tmode\tctime
        std::istringstream iss(line);
        std::string path, hash, sizeStr, mtimeStr, modeStr, ctimeStr;
        if (!std::getline(iss, path, '\t')) continue;
        std::getline(iss, hash, '\t');
        std::getline(iss, sizeStr, '\t');
        std::getline(iss, mtimeStr, '\t');
        std::getline(iss, modeStr, '\t');
        std::getline(iss, ctimeStr, '\t');
        IndexEntry e;
        e.path = path;
        e.hashHex = hash;
        e.sizeBytes = sizeStr.empty() ? 0ULL : static_cast<uint64_t>(std::stoull(sizeStr));
        e.mtimeNs = mtimeStr.empty() ? 0ULL : static_cast<uint64_t>(std::stoull(mtimeStr));
        e.mode = modeStr.empty() ? 0U : static_cast<uint32_t>(std::stoul(modeStr));
        e.ctimeNs = ctimeStr.empty() ? 0ULL : static_cast<uint64_t>(std::stoull(ctimeStr));
        pathToEntry[e.path] = e;
    }
    return true;
}

bool Index::save(const fs::path& repoRoot) const {
    fs::create_directories(repoRoot / ".gitter");
    std::ofstream out(indexPathOf(repoRoot), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    for (const auto& kv : pathToEntry) {
        const auto& e = kv.second;
        out << e.path << '\t' << e.hashHex << '\t' << e.sizeBytes << '\t' 
            << e.mtimeNs << '\t' << e.mode << '\t' << e.ctimeNs << '\n';
    }
    return true;
}

void Index::addOrUpdate(const IndexEntry& entry) {
    pathToEntry[entry.path] = entry;
}

void Index::remove(const std::string& path) {
    pathToEntry.erase(path);
}

}


