#include "util/IHasher.hpp"
#include "util/Sha1Hasher.hpp"
#include "util/Sha256Hasher.hpp"

namespace gitter {

// IHasher static methods
std::string IHasher::toHex(const std::vector<uint8_t>& bytes) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.resize(bytes.size() * 2);
    for (size_t i = 0; i < bytes.size(); ++i) {
        out[2*i] = hex[(bytes[i] >> 4) & 0xF];
        out[2*i+1] = hex[bytes[i] & 0xF];
    }
    return out;
}

// HasherFactory implementation
std::unique_ptr<IHasher> HasherFactory::createDefault() {
    // Git uses SHA-1 by default for compatibility
    return std::make_unique<Sha1Hasher>();
}

std::unique_ptr<IHasher> HasherFactory::create(const std::string& algorithm) {
    if (algorithm == "sha1") {
        return std::make_unique<Sha1Hasher>();
    } else if (algorithm == "sha256") {
        return std::make_unique<Sha256Hasher>();
    }
    return createDefault();
}

}

