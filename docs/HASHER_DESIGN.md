# Hasher Architecture - Strategy Pattern Implementation

## Overview

Gitter uses a pluggable hasher architecture to support multiple hash algorithms. The current implementation supports SHA-1 (Git default) and SHA-256 (Git future).

## File Structure

```
src/util/
├── IHasher.hpp              # Strategy interface for hash algorithms
├── HasherFactory.cpp        # Factory implementation + IHasher::toHex()
├── Sha1Hasher.hpp          # SHA-1 interface
├── Sha1Hasher.cpp          # SHA-1 implementation (160-bit)
├── Sha256Hasher.hpp        # SHA-256 interface
└── Sha256Hasher.cpp        # SHA-256 implementation (256-bit)
```

## Class Diagram

```
┌─────────────────────────┐
│      <<interface>>      │
│        IHasher          │
├─────────────────────────┤
│ + reset()               │
│ + update(data, len)     │
│ + update(string)        │
│ + digest(): bytes       │
│ + name(): string        │
│ + digestSize(): size_t  │
│ + toHex(bytes): string  │ (static)
└───────────┬─────────────┘
            │
            │ implements
      ┌─────┴──────┐
      │            │
┌─────▼─────┐  ┌──▼──────────┐
│Sha1Hasher │  │Sha256Hasher │
├───────────┤  ├─────────────┤
│digestSize │  │digestSize   │
│  = 20     │  │  = 32       │
└───────────┘  └─────────────┘

┌─────────────────────────┐
│    HasherFactory        │
├─────────────────────────┤
│ + createDefault()       │ → Sha1Hasher
│ + create(algorithm)     │ → Sha1/Sha256Hasher
└─────────────────────────┘
```

## Design Rationale

### Why Strategy Pattern?

1. **Flexibility** - Swap algorithms at runtime without recompilation
2. **Open/Closed Principle** - Add new algorithms without modifying existing code
3. **Dependency Inversion** - High-level code depends on abstraction (IHasher)
4. **Testability** - Easy to mock IHasher for unit tests
5. **Single Responsibility** - Each algorithm in its own focused file

### Before vs After

#### Before (Monolithic)
```
src/util/
└── Hasher.hpp/.cpp  (120+ lines, mixed responsibilities)
    ├── IHasher interface
    ├── HasherFactory
    ├── SHA-256 implementation
    └── Utility methods
```

**Problems:**
- Hard to navigate
- Mixed responsibilities
- Difficult to extend
- Violates Single Responsibility Principle

#### After (Modular)
```
src/util/
├── IHasher.hpp              (Strategy interface - 20 lines)
├── HasherFactory.cpp        (Factory + utilities - 35 lines)
├── Sha1Hasher.hpp/.cpp     (SHA-1 implementation - 150 lines)
└── Sha256Hasher.hpp/.cpp   (SHA-256 implementation - 130 lines)
```

**Benefits:**
- Clear separation of concerns
- Easy to find specific algorithm
- Can compile algorithms independently
- Follows SOLID principles
- Easier to add new algorithms (SHA-3, BLAKE2, etc.)

## Usage Examples

### 1. Using Default Hasher (SHA-1)

```cpp
#include "util/IHasher.hpp"

// ObjectStore.cpp
ObjectStore::ObjectStore(const fs::path& root)
    : root(root), hasher(HasherFactory::createDefault()) {
    // hasher is now SHA-1 (Git default)
}
```

### 2. Using Specific Algorithm

```cpp
#include "util/IHasher.hpp"

// Create SHA-256 hasher explicitly
auto sha256 = HasherFactory::create("sha256");
sha256->update("hello world");
auto hash = sha256->digest();
std::string hexHash = IHasher::toHex(hash);
```

### 3. Adding to ObjectStore

```cpp
// Create ObjectStore with default SHA-1
ObjectStore store(repoRoot);

// Or use SHA-256
ObjectStore store2(repoRoot, std::make_unique<Sha256Hasher>());
```

## Adding a New Hash Algorithm

To add SHA-3 support:

### Step 1: Create Header
```cpp
// src/util/Sha3Hasher.hpp
#pragma once
#include "util/IHasher.hpp"

namespace gitter {
class Sha3Hasher : public IHasher {
public:
    Sha3Hasher();
    void reset() override;
    void update(const uint8_t* data, size_t len) override;
    void update(const std::string& data) override;
    std::vector<uint8_t> digest() override;
    const char* name() const override { return "sha3"; }
    size_t digestSize() const override { return 32; }
private:
    // SHA-3 state...
};
}
```

### Step 2: Implement Algorithm
```cpp
// src/util/Sha3Hasher.cpp
#include "util/Sha3Hasher.hpp"
// ... implementation ...
```

### Step 3: Register in Factory
```cpp
// src/util/HasherFactory.cpp
#include "util/Sha3Hasher.hpp"

std::unique_ptr<IHasher> HasherFactory::create(const std::string& algorithm) {
    if (algorithm == "sha1") {
        return std::make_unique<Sha1Hasher>();
    } else if (algorithm == "sha256") {
        return std::make_unique<Sha256Hasher>();
    } else if (algorithm == "sha3") {
        return std::make_unique<Sha3Hasher>();  // Add this
    }
    return createDefault();
}
```

### Step 4: Update CMakeLists.txt
```cmake
add_library(gitter_lib
  # ...
  src/util/Sha3Hasher.cpp
  # ...
)
```

**Done!** No changes needed in ObjectStore, Index, or command classes.

## Hash Algorithm Comparison

| Algorithm | Digest Size | Speed  | Security | Git Usage      |
|-----------|-------------|--------|----------|----------------|
| SHA-1     | 20 bytes    | Fast   | Broken   | Default (legacy)|
| SHA-256   | 32 bytes    | Medium | Strong   | Future default |
| SHA-3     | 32 bytes    | Medium | Strong   | Not used       |
| BLAKE2    | 32 bytes    | Fastest| Strong   | Not used       |

## Git Compatibility

### SHA-1 (Git Default)
- Used by Git since inception
- 40-character hex hash
- Example: `95d09f2b10159347eece71399a7e2e907ea3df4f`
- Cryptographically broken but maintained for backward compatibility

### SHA-256 (Git Future)
- Introduced in Git 2.29 (experimental)
- 64-character hex hash
- Opt-in via `git config`
- Stronger security for future Git versions

### Our Implementation
```cpp
// Default: SHA-1 (Git-compatible)
ObjectStore store(root);

// Optional: SHA-256 (Git future mode)
ObjectStore store(root, std::make_unique<Sha256Hasher>());
```

## Performance Considerations

### Memory Allocation
- Each hasher instance allocates ~100 bytes on stack
- No heap allocations during hashing
- Digest returned as `std::vector<uint8_t>` (small, typically optimized)

### Optimization Opportunities
1. **Object Pooling**: Reuse hasher instances with `reset()`
2. **Streaming for Large Files**: Process in chunks with `update()`
3. **SIMD Acceleration** (future): Use CPU SIMD instructions for speed

## Testing Strategy

### Unit Tests
```cpp
TEST(Sha1Hasher, KnownVector) {
    Sha1Hasher h;
    h.update("abc");
    auto digest = h.digest();
    auto hex = IHasher::toHex(digest);
    EXPECT_EQ(hex, "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST(HasherFactory, CreateSha1) {
    auto h = HasherFactory::create("sha1");
    EXPECT_EQ(h->name(), "sha1");
    EXPECT_EQ(h->digestSize(), 20);
}
```

### Integration Tests
```cpp
TEST(ObjectStore, Sha1Blobs) {
    ObjectStore store(tempDir, std::make_unique<Sha1Hasher>());
    auto hash = store.writeBlob("hello");
    EXPECT_EQ(hash.length(), 40);  // SHA-1 = 20 bytes = 40 hex chars
}
```

## Git-Compliant Object Storage

### 2-Character Directory Structure
```
Old: .gitter/objects/<full-hash>
New: .gitter/objects/<first-2-chars>/<remaining-chars>

Example: hash "abc123..." → .gitter/objects/ab/c123...
```
- Matches Git's standard object storage layout
- Prevents too many files in single directory
- Improves filesystem performance

### Zlib Compression
- All objects compressed with zlib before writing
- Uses `deflate()` for compression, `inflate()` for decompression
- Matches Git's loose object storage format
- Significantly reduces disk space usage (50-80% compression)

### Object Format
```
blob <size>\0<content>
tree <size>\0<entries>
commit <size>\0<metadata>
```

Each object is:
1. Created with Git format header
2. Hashed (SHA-1 or SHA-256)
3. Compressed with zlib
4. Stored in `.gitter/objects/<aa>/<bbb...>`

## Enhanced Index (Staging Area)

### New Fields in IndexEntry

1. **`mode` (uint32_t)** - File permissions
   - Git uses octal mode: `0100644` (regular file) or `0100755` (executable)
   - Tracks whether file is executable
   - Essential for preserving permissions across platforms

2. **`ctimeNs` (uint64_t)** - Creation/status change time
   - Git tracks both mtime and ctime
   - Used for detecting file metadata changes
   - Approximated with mtime on systems without true ctime

### Updated On-Disk Format
```
Old: path<TAB>hash<TAB>size<TAB>mtime
New: path<TAB>hash<TAB>size<TAB>mtime<TAB>mode<TAB>ctime
```

Example:
```
src/main.cpp    a3b2c1...    1024    mtime    33188    ctime
```
(Full nanosecond timestamps in practice)

## Index to Blob Linking

1. **File Added to Staging**
   ```bash
   gitter add file.txt
   ```

2. **Blob Created**
   - File content read
   - Git blob object created: `"blob <size>\0<content>"`
   - Hash computed (SHA-1 by default)
   - Compressed and stored in `.gitter/objects/<aa>/<bbbb...>`

3. **Index Entry Created**
   - Path: `file.txt`
   - Hash: `<blob-hash>`
   - Size: `<file-size>`
   - Mtime: `<modification-time>`
   - Mode: `0100644` or `0100755`
   - Ctime: `<creation-time>`

4. **Index Saved**
   - TSV format written to `.gitter/index`

## Benefits of This Design

### 1. Git Compatibility
- ✅ Matches Git's object storage layout exactly
- ✅ Uses same blob format: `"blob <size>\0<content>"`
- ✅ Implements 2-char directory structure
- ✅ Uses zlib compression
- ✅ Defaults to SHA-1 for compatibility

### 2. Extensibility
- ✅ Strategy Pattern allows swapping hash algorithms
- ✅ SHA-256 support already implemented
- ✅ Can support future hash algorithms (SHA3, BLAKE2, etc.)
- ✅ No modifications needed to core logic when adding algorithms

### 3. Performance
- ✅ Zlib compression reduces disk usage significantly
- ✅ 2-char directory structure improves filesystem performance
- ✅ Index tracks file metadata for fast dirty detection
- ✅ Streaming support for large files

### 4. Correctness
- ✅ Preserves file permissions (executable bit)
- ✅ Tracks both mtime and ctime
- ✅ Proper Git blob format ensures interoperability
- ✅ Comprehensive test coverage

### 5. Code Quality
- ✅ Single Responsibility Principle per file
- ✅ Clear separation of concerns
- ✅ Easy to navigate and maintain
- ✅ Compilation benefits from modular structure

## Summary

The hasher architecture provides:

✅ **Clean Separation** - One file per algorithm  
✅ **Strategy Pattern** - Runtime algorithm selection  
✅ **Extensibility** - Easy to add new algorithms  
✅ **Git Compatibility** - SHA-1 default, SHA-256 available  
✅ **Maintainability** - Clear, focused files  
✅ **Testability** - Mock-friendly interface  
✅ **Performance** - Zlib compression, fast metadata checks  
✅ **Correctness** - File permissions and timestamps preserved  

This design positions gitter for future growth while maintaining clean, professional code organization.

## Related Documentation

- See `docs/ARCHITECTURE.md` for overall project context
- See `docs/TREE_STORAGE.md` for directory tree storage details
- See `docs/ARCHITECTURE.md` for ObjectStore integration

## References

- [Git Internals - Git Objects](https://git-scm.com/book/en/v2/Git-Internals-Git-Objects)
- [SHA-1 vs SHA-256 in Git](https://git-scm.com/docs/hash-function-transition)
- [zlib Documentation](https://zlib.net/manual.html)
