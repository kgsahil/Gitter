#include "core/Index.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include "core/Constants.hpp"

namespace fs = std::filesystem;

namespace gitter {

static fs::path indexPathOf(const fs::path& root) { return root / ".gitter" / "index"; }

/**
 * @brief Normalize path for consistent storage in index
 * 
 * Normalizes path to use forward slashes and removes unnecessary components
 * like "./" prefix. Ensures same file always has same path representation.
 */
static std::string normalizePath(const std::string& path) {
    fs::path p(path);
    std::string normalized = p.lexically_normal().generic_string();
    
    // Remove leading ./ if present
    if (normalized.length() >= 2 && normalized.substr(0, 2) == "./") {
        normalized = normalized.substr(2);
    }
    
    return normalized;
}

/**
 * @brief Validate hash is 40-character hex string (SHA-1)
 */
static bool isValidHash(const std::string& hash) {
    return hash.length() == Constants::SHA1_HEX_LENGTH &&
           std::all_of(hash.begin(), hash.end(), 
                      [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); });
}

bool Index::load(const fs::path& repoRoot) {
    pathToEntry.clear();
    fs::path indexPath = indexPathOf(repoRoot);
    std::ifstream in(indexPath, std::ios::binary);
    if (!in) {
        // Treat missing index as empty (first time use)
        return true;
    }
    
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
        
        // Validate hash format
        if (!isValidHash(hash)) {
            // Skip invalid entries (corrupted index)
            continue;
        }
        
        // Normalize path for consistent storage
        std::string normalizedPath = normalizePath(path);
        
        IndexEntry e;
        e.path = normalizedPath;
        e.hashHex = hash;
        
        // Parse numeric fields with error handling
        try {
            e.sizeBytes = sizeStr.empty() ? 0ULL : static_cast<uint64_t>(std::stoull(sizeStr));
            e.mtimeNs = mtimeStr.empty() ? 0ULL : static_cast<uint64_t>(std::stoull(mtimeStr));
            e.mode = modeStr.empty() ? 0U : static_cast<uint32_t>(std::stoul(modeStr));
            e.ctimeNs = ctimeStr.empty() ? 0ULL : static_cast<uint64_t>(std::stoull(ctimeStr));
        } catch (const std::exception&) {
            // Skip entries with invalid numeric fields
            continue;
        }
        
        pathToEntry[normalizedPath] = e;
    }
    
    // Check if read failed unexpectedly
    if (in.bad()) {
        return false;
    }
    
    return true;
}

bool Index::save(const fs::path& repoRoot) const {
    fs::path indexPath = indexPathOf(repoRoot);
    fs::path tempIndexPath = indexPath.string() + ".tmp";
    
    // Ensure .gitter directory exists
    std::error_code ec;
    fs::create_directories(repoRoot / ".gitter", ec);
    if (ec) {
        return false;
    }
    
    // Write to temporary file first (atomic write pattern)
    std::ofstream out(tempIndexPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    
    // Write all entries
    for (const auto& kv : pathToEntry) {
        const auto& e = kv.second;
        out << e.path << '\t' << e.hashHex << '\t' << e.sizeBytes << '\t' 
            << e.mtimeNs << '\t' << e.mode << '\t' << e.ctimeNs << '\n';
    }
    
    // Flush and verify write succeeded
    out.flush();
    if (!out || !out.good()) {
        out.close();
        fs::remove(tempIndexPath, ec);  // Clean up temp file
        return false;
    }
    out.close();
    
    // Atomic rename: only move temp to actual if write succeeded
    fs::rename(tempIndexPath, indexPath, ec);
    if (ec) {
        fs::remove(tempIndexPath, ec);  // Clean up on failure
        return false;
    }
    
    return true;
}

void Index::addOrUpdate(const IndexEntry& entry) {
    // Normalize path for consistent storage
    IndexEntry normalizedEntry = entry;
    normalizedEntry.path = normalizePath(entry.path);
    
    // Validate hash before storing
    if (!isValidHash(normalizedEntry.hashHex)) {
        throw std::invalid_argument("Invalid hash format: " + normalizedEntry.hashHex);
    }
    
    pathToEntry[normalizedEntry.path] = normalizedEntry;
}

void Index::remove(const std::string& path) {
    // Normalize path before removal
    std::string normalizedPath = normalizePath(path);
    pathToEntry.erase(normalizedPath);
}

}


