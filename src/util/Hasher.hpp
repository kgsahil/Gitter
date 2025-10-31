#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gitter {

/**
 * @brief SHA-256 cryptographic hash implementation
 * 
 * Implements the SHA-256 hashing algorithm used by Git to create
 * content-addressable object identifiers. This provides streaming
 * hash computation for files of any size.
 * 
 * Usage:
 *   Sha256 h;
 *   h.update("blob 5\0hello");
 *   std::string hash = Sha256::toHex(h.digest());
 */
class Sha256 {
public:
    Sha256();
    
    /// Reset hasher state to initial values
    void reset();
    
    /// Update hash with raw byte data
    void update(const uint8_t* data, size_t len);
    
    /// Update hash with vector of bytes
    void update(const std::vector<uint8_t>& data);
    
    /// Update hash with string content
    void update(const std::string& data);
    
    /// Finalize and return 32-byte digest (resets state after)
    std::vector<uint8_t> digest();
    
    /// Convert binary hash to lowercase hex string (64 chars)
    static std::string toHex(const std::vector<uint8_t>& bytes);

private:
    void transform(const uint8_t* chunk);
    uint32_t state[8];      // Current hash state
    uint64_t bitlen;        // Total bits processed
    uint8_t buffer[64];     // Pending block buffer
    size_t bufferLen;       // Current buffer fill
};

}

