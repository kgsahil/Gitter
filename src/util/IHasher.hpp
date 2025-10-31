#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace gitter {

/**
 * @brief Strategy interface for hash algorithms
 * 
 * Allows swapping between different hash algorithms (SHA-1, SHA-256, etc.)
 * without changing client code. Git uses SHA-1 by default.
 */
class IHasher {
public:
    virtual ~IHasher() = default;
    
    /// Reset hasher to initial state
    virtual void reset() = 0;
    
    /// Update hash with raw bytes
    virtual void update(const uint8_t* data, size_t len) = 0;
    
    /// Update hash with string
    virtual void update(const std::string& data) = 0;
    
    /// Finalize and return digest bytes
    virtual std::vector<uint8_t> digest() = 0;
    
    /// Get hash algorithm name (e.g., "sha1", "sha256")
    virtual const char* name() const = 0;
    
    /// Get digest size in bytes (20 for SHA-1, 32 for SHA-256)
    virtual size_t digestSize() const = 0;
    
    /// Convert binary hash to lowercase hex string
    static std::string toHex(const std::vector<uint8_t>& bytes);
};

/**
 * @brief Factory for creating hasher instances
 */
class HasherFactory {
public:
    /// Create default hasher (SHA-1 for Git compatibility)
    static std::unique_ptr<IHasher> createDefault();
    
    /// Create specific hasher by name
    static std::unique_ptr<IHasher> create(const std::string& algorithm);
};

}

