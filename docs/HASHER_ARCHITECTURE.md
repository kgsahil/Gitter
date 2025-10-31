# Hasher Architecture - Strategy Pattern Implementation

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

1. **Flexibility**
   - Swap hash algorithms at runtime
   - No recompilation needed to change algorithm
   - Easy to add new algorithms (SHA3, BLAKE2, etc.)

2. **Open/Closed Principle**
   - Open for extension (add new hashers)
   - Closed for modification (don't touch existing code)

3. **Dependency Inversion**
   - High-level code (ObjectStore) depends on abstraction (IHasher)
   - Not on concrete implementations (Sha1Hasher, Sha256Hasher)

4. **Testability**
   - Easy to mock IHasher for unit tests
   - Can inject test hasher with known outputs

### Why Separate Files?

#### Before (Monolithic)
```
src/util/
├── Hasher.hpp    # Contains: IHasher + HasherFactory + Sha256
└── Hasher.cpp    # 120+ lines of mixed code
```

**Problems:**
- Hard to navigate
- Mixed responsibilities
- Difficult to extend
- Violates Single Responsibility Principle

#### After (Modular)
```
src/util/
├── IHasher.hpp           # Interface only (20 lines)
├── HasherFactory.cpp     # Factory + utilities (35 lines)
├── Sha1Hasher.hpp       # SHA-1 interface (35 lines)
├── Sha1Hasher.cpp       # SHA-1 implementation (115 lines)
├── Sha256Hasher.hpp     # SHA-256 interface (35 lines)
└── Sha256Hasher.cpp     # SHA-256 implementation (95 lines)
```

**Benefits:**
- ✅ Clear separation of concerns
- ✅ Easy to find specific algorithm
- ✅ Can compile algorithms independently
- ✅ Follows Single Responsibility Principle
- ✅ Easier to add SHA3, BLAKE2, etc.

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
// Create ObjectStore with SHA-256
ObjectStore store(repoRoot, std::make_unique<Sha256Hasher>());

// Or use default SHA-1
ObjectStore store(repoRoot);  // Uses SHA-1
```

### 4. Computing Hash Directly

```cpp
#include "util/Sha1Hasher.hpp"

Sha1Hasher hasher;
hasher.update("blob 11\0hello world");
std::vector<uint8_t> digest = hasher.digest();
std::string hash = IHasher::toHex(digest);
// hash = "95d09f2b10159347eece71399a7e2e907ea3df4f"
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

## Performance Considerations

### Memory Allocation
- Each hasher instance allocates ~100 bytes on stack
- No heap allocations during hashing
- Digest returned as `std::vector<uint8_t>` (small, typically optimized)

### Optimization Opportunities
1. **Object Pooling**
   ```cpp
   // Reuse hasher instances
   hasher->reset();  // Clear state
   hasher->update(newData);  // Reuse
   ```

2. **Streaming for Large Files**
   ```cpp
   Sha1Hasher h;
   while (read chunk from file) {
       h.update(chunk);  // Process in chunks
   }
   auto hash = h.digest();
   ```

3. **SIMD Acceleration** (future)
   - SHA-1/SHA-256 can use CPU SIMD instructions
   - Add compile-time flags for AVX2/NEON

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

## Git Compatibility

### SHA-1 (Git Default)
- Used by Git since inception
- 40-character hex hash
- Example: `95d09f2b10159347eece71399a7e2e907ea3df4f`

### SHA-256 (Git Future)
- Introduced in Git 2.29 (experimental)
- 64-character hex hash
- Opt-in via `git config`

### Our Implementation
```cpp
// Default: SHA-1 (Git-compatible)
ObjectStore store(root);

// Optional: SHA-256 (Git future mode)
ObjectStore store(root, std::make_unique<Sha256Hasher>());
```

## File Organization Benefits

### Before
```
Hasher.cpp:120 lines
  - IHasher::toHex()       (10 lines)
  - HasherFactory methods  (15 lines)
  - SHA-256 implementation (95 lines)
```

### After
```
HasherFactory.cpp:35 lines   # Factory + utilities
Sha1Hasher.cpp:115 lines     # SHA-1 only
Sha256Hasher.cpp:95 lines    # SHA-256 only
```

**Advantages:**
1. **Compile Time** - Only recompile changed algorithm
2. **Code Navigation** - Jump directly to algorithm
3. **Parallel Development** - Work on SHA-1 and SHA-256 independently
4. **Binary Size** - Can exclude unused algorithms (link-time optimization)

## Summary

The refactored hasher architecture provides:

✅ **Clean Separation** - One file per algorithm  
✅ **Strategy Pattern** - Runtime algorithm selection  
✅ **Extensibility** - Easy to add new algorithms  
✅ **Git Compatibility** - SHA-1 default, SHA-256 available  
✅ **Maintainability** - Clear, focused files  
✅ **Testability** - Mock-friendly interface  

This design positions gitter for future growth while maintaining clean, professional code organization.

