#pragma once

#include "util/IHasher.hpp"
#include <cstdint>

namespace gitter {

/**
 * @brief SHA-256 cryptographic hash implementation
 * 
 * Implements the SHA-256 hashing algorithm. Git is transitioning
 * to SHA-256 for improved security over SHA-1.
 * Produces 256-bit (32-byte) digests.
 */
class Sha256Hasher : public IHasher {
public:
    Sha256Hasher();
    
    void reset() override;
    void update(const uint8_t* data, size_t len) override;
    void update(const std::vector<uint8_t>& data);
    void update(const std::string& data) override;
    std::vector<uint8_t> digest() override;
    const char* name() const override { return "sha256"; }
    size_t digestSize() const override { return 32; }

private:
    void transform(const uint8_t* chunk);
    uint32_t state[8];      // Current hash state
    uint64_t bitlen;        // Total bits processed
    uint8_t buffer[64];     // Pending block buffer
    size_t bufferLen;       // Current buffer fill
};

}

