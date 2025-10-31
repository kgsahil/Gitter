#pragma once

#include "util/IHasher.hpp"
#include <cstdint>

namespace gitter {

/**
 * @brief SHA-1 hash implementation (Git's default)
 * 
 * Implements the SHA-1 hashing algorithm as used by Git.
 * Produces 160-bit (20-byte) digests.
 * 
 * Note: SHA-1 is cryptographically broken but still used by Git
 * for backward compatibility. Git is transitioning to SHA-256.
 */
class Sha1Hasher : public IHasher {
public:
    Sha1Hasher();
    
    void reset() override;
    void update(const uint8_t* data, size_t len) override;
    void update(const std::string& data) override;
    std::vector<uint8_t> digest() override;
    const char* name() const override { return "sha1"; }
    size_t digestSize() const override { return 20; }

private:
    void transform(const uint8_t* chunk);
    
    uint32_t state[5];      // Hash state (A, B, C, D, E)
    uint64_t bitlen;        // Total bits processed
    uint8_t buffer[64];     // Pending block buffer
    size_t bufferLen;       // Current buffer fill
};

}

