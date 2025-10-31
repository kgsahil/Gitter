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
    // Git stores objects as: .git/objects/<first-2-chars>/<remaining-chars>
    // Example: abc123... → .git/objects/ab/c123...
    if (hash.length() < 3) {
        throw std::runtime_error("Invalid hash length");
    }
    std::string dir = hash.substr(0, 2);
    std::string file = hash.substr(2);
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
        fs::create_directories(objPath.parent_path());
        
        // Compress with zlib
        std::vector<uint8_t> compressed = zlibCompress(fullObject);
        
        // Write compressed data
        std::ofstream out(objPath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(compressed.data()), 
                  static_cast<std::streamsize>(compressed.size()));
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
        fs::create_directories(objPath.parent_path());
        
        // Compress with zlib
        std::vector<uint8_t> compressed = zlibCompress(fullObject);
        
        // Write compressed data
        std::ofstream out(objPath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(compressed.data()), 
                  static_cast<std::streamsize>(compressed.size()));
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
        fs::create_directories(objPath.parent_path());
        
        // Compress with zlib
        std::vector<uint8_t> compressed = zlibCompress(fullObject);
        
        // Write compressed data
        std::ofstream out(objPath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(compressed.data()), 
                  static_cast<std::streamsize>(compressed.size()));
    }
    
    return hash;
}

std::string ObjectStore::writeBlobFromFile(const fs::path& filePath) {
    std::ifstream in(filePath, std::ios::binary);
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string bytes(reinterpret_cast<const char*>(buf.data()), buf.size());
    return writeBlob(bytes);
}

std::string ObjectStore::hashFileContent(const fs::path& filePath) {
    std::ifstream in(filePath, std::ios::binary);
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
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
    std::vector<uint8_t> compressed((std::istreambuf_iterator<char>(in)), 
                                     std::istreambuf_iterator<char>());
    
    // Decompress
    return zlibDecompress(compressed);
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
            commit.treeHash = line.substr(5);
        } else if (line.rfind("parent ", 0) == 0) {
            commit.parentHashes.push_back(line.substr(7));
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
    
    // Remove trailing newline from message
    if (!commit.message.empty() && commit.message.back() == '\n') {
        commit.message.pop_back();
    }
    
    return commit;
}

}
