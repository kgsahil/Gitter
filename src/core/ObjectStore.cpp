#include "core/ObjectStore.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <iterator>

#include "util/Hasher.hpp"

namespace fs = std::filesystem;

namespace gitter {

ObjectStore::ObjectStore(const fs::path& repoRoot) : root(repoRoot) {}

fs::path ObjectStore::objectsDir() const { return root / ".gitter" / "objects"; }

std::string ObjectStore::writeBlob(const std::string& content) {
    // Git object format: "blob <size>\0<content>"
    std::string header = "blob " + std::to_string(content.size());
    header += '\0';
    std::string fullObject = header + content;
    
    Sha256 h; 
    h.update(fullObject);
    std::string hash = Sha256::toHex(h.digest());
    
    fs::path objPath = objectsDir() / hash;
    if (!fs::exists(objPath)) {
        std::ofstream out(objPath, std::ios::binary);
        out.write(fullObject.data(), static_cast<std::streamsize>(fullObject.size()));
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
    
    Sha256 h;
    h.update(fullObject);
    return Sha256::toHex(h.digest());
}

}


