#include "core/Index.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <vector>
#include "core/Constants.hpp"

namespace fs = std::filesystem;

namespace gitter {

static fs::path indexPathOf(const fs::path& root) { return root / ".gitter" / "index"; }

/**
 * @brief Base64 encode for path to handle special characters (TAB/newlines)
 * 
 * Git stores special characters directly in binary index, but our TSV format
 * can't handle TAB/newline separators. We base64-encode paths to support
 * any filename that Git supports, including those with TAB, newline, etc.
 * 
 * This matches Git's ability to track files with any valid filename.
 */
static std::string base64Encode(const std::string& input) {
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int val = 0, valb = -6;
    
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        encoded.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (encoded.size() % 4) {
        encoded.push_back('=');
    }
    
    return encoded;
}

/**
 * @brief Base64 decode path
 */
static std::string base64Decode(const std::string& input) {
    const unsigned char chars[] = 
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
        "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x3e\xff\xff\xff\x3f"
        "\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\xff\xff\xff\xff\xff\xff"
        "\xff\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
        "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\xff\xff\xff\xff\xff"
        "\xff\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28"
        "\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\xff\xff\xff\xff\xff";
    
    std::string decoded;
    int val = 0, valb = -8;
    
    for (unsigned char c : input) {
        if (chars[c] == 0xff) continue;
        val = (val << 6) + chars[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return decoded;
}

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
        
        // TSV: base64path\thash\tsize\tmtime\tmode\tctime
        // Path is base64-encoded to handle TAB/newline characters
        std::istringstream iss(line);
        std::string encodedPath, hash, sizeStr, mtimeStr, modeStr, ctimeStr;
        if (!std::getline(iss, encodedPath, '\t')) continue;
        std::getline(iss, hash, '\t');
        std::getline(iss, sizeStr, '\t');
        std::getline(iss, mtimeStr, '\t');
        std::getline(iss, modeStr, '\t');
        std::getline(iss, ctimeStr, '\t');
        
        // Decode path from base64
        std::string path;
        try {
            path = base64Decode(encodedPath);
        } catch (const std::exception&) {
            // Skip invalid base64 entries
            continue;
        }
        
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
    
    // Write all entries (paths base64-encoded to handle TAB/newlines)
    for (const auto& kv : pathToEntry) {
        const auto& e = kv.second;
        std::string encodedPath = base64Encode(e.path);
        out << encodedPath << '\t' << e.hashHex << '\t' << e.sizeBytes << '\t' 
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

void Index::clear() {
    pathToEntry.clear();
}

}


