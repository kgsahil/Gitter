#include "core/ObjectStore.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>
#include <stdexcept>

#include "util/IHasher.hpp"
#include "util/Sha1Hasher.hpp"
#include "util/Sha256Hasher.hpp"
#include "core/CommitObject.hpp"
#include "core/Constants.hpp"
#include "core/TreeBuilder.hpp"

#include <zlib.h>

namespace fs = std::filesystem;

namespace gitter {

namespace {
    /**
     * @brief Compress data using zlib (Git uses zlib deflate)
     */
    std::vector<uint8_t> zlibCompress(const std::string& data) {
        z_stream stream{};
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        
        if (deflateInit(&stream, Z_DEFAULT_COMPRESSION) != Z_OK) {
            throw std::runtime_error("zlib deflateInit failed");
        }
        
        stream.avail_in = static_cast<uInt>(data.size());
        // Some zlib versions have non-const next_in, so we need to cast away const
        stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data()));
        
        std::vector<uint8_t> compressed;
        compressed.resize(deflateBound(&stream, static_cast<uLong>(data.size())));
        
        stream.avail_out = static_cast<uInt>(compressed.size());
        stream.next_out = compressed.data();
        
        if (deflate(&stream, Z_FINISH) != Z_STREAM_END) {
            deflateEnd(&stream);
            throw std::runtime_error("zlib deflate failed");
        }
        
        compressed.resize(stream.total_out);
        deflateEnd(&stream);
        return compressed;
    }
    
    /**
     * @brief Decompress zlib data
     */
    std::string zlibDecompress(const std::vector<uint8_t>& compressed) {
        z_stream stream{};
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        
        if (inflateInit(&stream) != Z_OK) {
            throw std::runtime_error("zlib inflateInit failed");
        }
        
        stream.avail_in = static_cast<uInt>(compressed.size());
        stream.next_in = const_cast<Bytef*>(compressed.data());
        
        std::string decompressed;
        std::vector<uint8_t> buffer(4096);
        
        int ret;
        do {
            stream.avail_out = static_cast<uInt>(buffer.size());
            stream.next_out = buffer.data();
            
            ret = inflate(&stream, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                inflateEnd(&stream);
                throw std::runtime_error("zlib inflate failed");
            }
            
            size_t have = buffer.size() - stream.avail_out;
            decompressed.append(reinterpret_cast<char*>(buffer.data()), have);
        } while (ret != Z_STREAM_END);
        
        inflateEnd(&stream);
        return decompressed;
    }
}

ObjectStore::ObjectStore(const fs::path& repoRoot, std::unique_ptr<IHasher> hasher)
    : root(repoRoot), hasher(hasher ? std::move(hasher) : HasherFactory::createDefault()) {}

ObjectStore::~ObjectStore() = default;

ObjectStore::ObjectStore(ObjectStore&&) noexcept = default;
ObjectStore& ObjectStore::operator=(ObjectStore&&) noexcept = default;

fs::path ObjectStore::objectsDir() const {
    return root / ".gitter" / "objects";
}

fs::path ObjectStore::getObjectPath(const std::string& hash) const {
    // Git stores objects as: .git/objects/<first-N-chars>/<remaining-chars>
    // We use first 2 chars for directory (Git uses 2)
    if (hash.length() < Constants::OBJECT_DIR_LENGTH + 1) {
        throw std::runtime_error("Invalid hash length: " + hash);
    }
    std::string dir = hash.substr(0, Constants::OBJECT_DIR_LENGTH);
    std::string file = hash.substr(Constants::OBJECT_DIR_LENGTH);
    return objectsDir() / dir / file;
}

std::string ObjectStore::writeBlob(const std::string& content) {
    // Git object format: "blob <size>\0<content>"
    std::string header = "blob " + std::to_string(content.size());
    header += '\0';
    std::string fullObject = header + content;
    
    // Hash the full object
    hasher->reset();
    hasher->update(fullObject);
    std::string hash = IHasher::toHex(hasher->digest());
    
    // Get object path with 2-char directory structure
    fs::path objPath = getObjectPath(hash);
    
    // Only write if doesn't exist
    if (!fs::exists(objPath)) {
        // Create directory if needed
        std::error_code ec;
        fs::create_directories(objPath.parent_path(), ec);
        if (ec) {
            throw std::runtime_error("Failed to create object directory: " + ec.message());
        }
        
        // Compress with zlib
        std::vector<uint8_t> compressed = zlibCompress(fullObject);
        
        // Write compressed data
        std::ofstream out(objPath, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Failed to open object file for writing: " + hash);
        }
        out.write(reinterpret_cast<const char*>(compressed.data()), 
                  static_cast<std::streamsize>(compressed.size()));
        out.flush();
        if (!out || !out.good()) {
            fs::remove(objPath);  // Clean up partial write
            throw std::runtime_error("Failed to write object: " + hash);
        }
    }
    
    return hash;
}

std::string ObjectStore::writeTree(const std::string& content) {
    // Git object format: "tree <size>\0<content>"
    std::string header = "tree " + std::to_string(content.size());
    header += '\0';
    std::string fullObject = header + content;
    
    // Hash the full object
    hasher->reset();
    hasher->update(fullObject);
    std::string hash = IHasher::toHex(hasher->digest());
    
    // Get object path with 2-char directory structure
    fs::path objPath = getObjectPath(hash);
    
    // Only write if doesn't exist
    if (!fs::exists(objPath)) {
        // Create directory if needed
        std::error_code ec;
        fs::create_directories(objPath.parent_path(), ec);
        if (ec) {
            throw std::runtime_error("Failed to create object directory: " + ec.message());
        }
        
        // Compress with zlib
        std::vector<uint8_t> compressed = zlibCompress(fullObject);
        
        // Write compressed data
        std::ofstream out(objPath, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Failed to open object file for writing");
        }
        out.write(reinterpret_cast<const char*>(compressed.data()), 
                  static_cast<std::streamsize>(compressed.size()));
        out.flush();
        if (!out || !out.good()) {
            fs::remove(objPath);  // Clean up partial write
            throw std::runtime_error("Failed to write tree object");
        }
    }
    
    return hash;
}

std::string ObjectStore::writeCommit(const std::string& content) {
    // Git object format: "commit <size>\0<content>"
    std::string header = "commit " + std::to_string(content.size());
    header += '\0';
    std::string fullObject = header + content;
    
    // Hash the full object
    hasher->reset();
    hasher->update(fullObject);
    std::string hash = IHasher::toHex(hasher->digest());
    
    // Get object path with 2-char directory structure
    fs::path objPath = getObjectPath(hash);
    
    // Only write if doesn't exist
    if (!fs::exists(objPath)) {
        // Create directory if needed
        std::error_code ec;
        fs::create_directories(objPath.parent_path(), ec);
        if (ec) {
            throw std::runtime_error("Failed to create object directory: " + ec.message());
        }
        
        // Compress with zlib
        std::vector<uint8_t> compressed = zlibCompress(fullObject);
        
        // Write compressed data
        std::ofstream out(objPath, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Failed to open commit file for writing");
        }
        out.write(reinterpret_cast<const char*>(compressed.data()), 
                  static_cast<std::streamsize>(compressed.size()));
        out.flush();
        if (!out || !out.good()) {
            fs::remove(objPath);  // Clean up partial write
            throw std::runtime_error("Failed to write commit object");
        }
    }
    
    return hash;
}

std::string ObjectStore::writeBlobFromFile(const fs::path& filePath) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open file for reading: " + filePath.string());
    }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad() && !in.eof()) {
        throw std::runtime_error("Error reading file: " + filePath.string());
    }
    std::string bytes(reinterpret_cast<const char*>(buf.data()), buf.size());
    return writeBlob(bytes);
}

std::string ObjectStore::hashFileContent(const fs::path& filePath) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open file for hashing: " + filePath.string());
    }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad() && !in.eof()) {
        throw std::runtime_error("Error reading file for hashing: " + filePath.string());
    }
    std::string content(reinterpret_cast<const char*>(buf.data()), buf.size());
    
    // Git object format for hash computation
    std::string header = "blob " + std::to_string(content.size());
    header += '\0';
    std::string fullObject = header + content;
    
    hasher->reset();
    hasher->update(fullObject);
    return IHasher::toHex(hasher->digest());
}

std::string ObjectStore::readObject(const std::string& hash) {
    fs::path objPath = getObjectPath(hash);
    
    if (!fs::exists(objPath)) {
        throw std::runtime_error("Object not found: " + hash);
    }
    
    // Read compressed data
    std::ifstream in(objPath, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open object file for reading: " + hash);
    }
    std::vector<uint8_t> compressed((std::istreambuf_iterator<char>(in)), 
                                     std::istreambuf_iterator<char>());
    if (in.bad() && !in.eof()) {
        throw std::runtime_error("Error reading object file: " + hash);
    }
    if (compressed.empty()) {
        throw std::runtime_error("Object file is empty: " + hash);
    }
    
    // Decompress
    return zlibDecompress(compressed);
}

std::string ObjectStore::readBlob(const std::string& hash) {
    // Read and decompress blob object
    std::string fullObject = readObject(hash);
    
    // Find header end (type size\0)
    size_t headerEnd = fullObject.find('\0');
    if (headerEnd == std::string::npos) {
        throw std::runtime_error("Invalid blob object format");
    }
    
    // Extract header and verify it's a blob
    std::string header = fullObject.substr(0, headerEnd);
    if (header.rfind("blob ", 0) != 0) {
        throw std::runtime_error("Not a blob object");
    }
    
    // Extract content after header
    return fullObject.substr(headerEnd + 1);
}

CommitObject ObjectStore::readCommit(const std::string& hash) {
    // Read and decompress commit object
    std::string fullObject = readObject(hash);
    
    // Find header end (type size\0)
    size_t headerEnd = fullObject.find('\0');
    if (headerEnd == std::string::npos) {
        throw std::runtime_error("Invalid commit object format");
    }
    
    // Extract header and verify it's a commit
    std::string header = fullObject.substr(0, headerEnd);
    if (header.rfind("commit ", 0) != 0) {
        throw std::runtime_error("Not a commit object");
    }
    
    // Extract content after header
    std::string content = fullObject.substr(headerEnd + 1);
    
    // Parse commit content
    CommitObject commit;
    commit.hash = hash;
    
    std::istringstream iss(content);
    std::string line;
    bool inMessage = false;
    std::ostringstream messageBuilder;
    
    while (std::getline(iss, line)) {
        if (inMessage) {
            // Everything after blank line is message
            messageBuilder << line << "\n";
            continue;
        }
        
        if (line.empty()) {
            // Blank line marks start of message
            inMessage = true;
            continue;
        }
        
        // Parse header lines
        if (line.rfind("tree ", 0) == 0) {
            // Tree hash is exactly SHA1_HEX_LENGTH characters (SHA-1 hex)
            std::string hashPart = line.substr(5);
            // Trim whitespace (in case of trailing spaces)
            size_t first = hashPart.find_first_not_of(" \t");
            if (first != std::string::npos) {
                hashPart.erase(0, first);
                size_t last = hashPart.find_last_not_of(" \t");
                if (last != std::string::npos) {
                    hashPart.erase(last + 1);
                }
            } else {
                hashPart.clear();
            }
            if (hashPart.length() >= Constants::SHA1_HEX_LENGTH) {
                commit.treeHash = hashPart.substr(0, Constants::SHA1_HEX_LENGTH);
            } else {
                throw std::runtime_error("Invalid tree hash length in commit: " + hash);
            }
        } else if (line.rfind("parent ", 0) == 0) {
            // Parent hash is exactly SHA1_HEX_LENGTH characters (SHA-1 hex)
            std::string hashPart = line.substr(7);
            // Trim whitespace (in case of trailing spaces)
            size_t first = hashPart.find_first_not_of(" \t");
            if (first != std::string::npos) {
                hashPart.erase(0, first);
                size_t last = hashPart.find_last_not_of(" \t");
                if (last != std::string::npos) {
                    hashPart.erase(last + 1);
                }
            } else {
                hashPart.clear();
            }
            if (hashPart.length() >= Constants::SHA1_HEX_LENGTH) {
                commit.parentHashes.push_back(hashPart.substr(0, Constants::SHA1_HEX_LENGTH));
            } else {
                throw std::runtime_error("Invalid parent hash length in commit: " + hash);
            }
        } else if (line.rfind("author ", 0) == 0) {
            // Format: author Name <email> timestamp timezone
            std::string authorLine = line.substr(7);
            size_t emailStart = authorLine.find('<');
            size_t emailEnd = authorLine.find('>');
            if (emailStart != std::string::npos && emailEnd != std::string::npos) {
                commit.authorName = authorLine.substr(0, emailStart - 1);
                commit.authorEmail = authorLine.substr(emailStart + 1, emailEnd - emailStart - 1);
                
                // Parse timestamp and timezone
                std::string rest = authorLine.substr(emailEnd + 2);
                std::istringstream restStream(rest);
                restStream >> commit.authorTimestamp >> commit.authorTimezone;
            }
        } else if (line.rfind("committer ", 0) == 0) {
            // Format: committer Name <email> timestamp timezone
            std::string committerLine = line.substr(10);
            size_t emailStart = committerLine.find('<');
            size_t emailEnd = committerLine.find('>');
            if (emailStart != std::string::npos && emailEnd != std::string::npos) {
                commit.committerName = committerLine.substr(0, emailStart - 1);
                commit.committerEmail = committerLine.substr(emailStart + 1, emailEnd - emailStart - 1);
                
                // Parse timestamp and timezone
                std::string rest = committerLine.substr(emailEnd + 2);
                std::istringstream restStream(rest);
                restStream >> commit.committerTimestamp >> commit.committerTimezone;
            }
        }
    }
    
    commit.message = messageBuilder.str();
    
    // Keep trailing newline - Git stores commit messages with trailing newline
    
    return commit;
}

std::vector<TreeEntry> ObjectStore::readTree(const std::string& hash) {
    // Read and decompress tree object
    std::string fullObject = readObject(hash);
    
    // Find header end (type size\0)
    size_t headerEnd = fullObject.find('\0');
    if (headerEnd == std::string::npos) {
        throw std::runtime_error("Invalid tree object format");
    }
    
    // Extract header and verify it's a tree
    std::string header = fullObject.substr(0, headerEnd);
    if (header.rfind("tree ", 0) != 0) {
        throw std::runtime_error("Not a tree object");
    }
    
    // Extract content after header
    std::string content = fullObject.substr(headerEnd + 1);
    
    // Parse tree entries
    std::vector<TreeEntry> entries;
    size_t pos = 0;
    
    // Tree format: <mode> <name>\0<20-byte-binary-hash>
    while (pos < content.size()) {
        TreeEntry entry;
        
        // Parse mode (octal number followed by space)
        size_t spacePos = content.find(' ', pos);
        if (spacePos == std::string::npos) {
            throw std::runtime_error("Invalid tree entry: missing mode");
        }
        std::string modeStr = content.substr(pos, spacePos - pos);
        entry.mode = static_cast<uint32_t>(std::stoi(modeStr, nullptr, 10));
        
        // Skip space and parse name
        size_t nullPos = content.find('\0', spacePos + 1);
        if (nullPos == std::string::npos) {
            throw std::runtime_error("Invalid tree entry: missing null terminator");
        }
        entry.name = content.substr(spacePos + 1, nullPos - spacePos - 1);
        
        // Determine if this is a tree (directory)
        entry.isTree = (entry.mode == 040000);
        
        // Parse binary hash and convert to hex
        size_t hashStart = nullPos + 1;
        size_t hashSize = hasher->digestSize();
        
        if (hashStart + hashSize > content.size()) {
            throw std::runtime_error("Invalid tree entry: incomplete hash");
        }
        
        // Convert binary hash to hex string
        std::string hashBytes = content.substr(hashStart, hashSize);
        entry.hashHex = IHasher::toHex(std::vector<uint8_t>(
            reinterpret_cast<const uint8_t*>(hashBytes.data()),
            reinterpret_cast<const uint8_t*>(hashBytes.data() + hashSize)
        ));
        
        entries.push_back(entry);
        
        // Move to next entry
        pos = hashStart + hashSize;
    }
    
    return entries;
}

}
