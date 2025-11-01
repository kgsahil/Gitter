# Hasher Design - Strategy Pattern Implementation

## Overview

Gitter uses a pluggable hasher architecture to support multiple hash algorithms. The current implementation supports SHA-1 (Git default) and SHA-256 (Git future).

## File Structure

```
src/util/
├── IHasher.hpp              # Strategy interface
├── HasherFactory.cpp        # Factory + utilities
├── Sha1Hasher.hpp/.cpp     # SHA-1 (160-bit)
└── Sha256Hasher.hpp/.cpp   # SHA-256 (256-bit)
```

## Why Strategy Pattern?

1. **Flexibility** - Swap algorithms at runtime
2. **Open/Closed** - Add new algorithms without modifying existing code
3. **Dependency Inversion** - High-level code depends on abstraction
4. **Testability** - Easy to mock for tests

## Usage

### Default (SHA-1)
```cpp
ObjectStore store(repoRoot);  // SHA-1
```

### Specific Algorithm
```cpp
ObjectStore store(repoRoot, std::make_unique<Sha256Hasher>());
```

### Factory
```cpp
auto hasher = HasherFactory::createDefault();  // SHA-1
auto sha256 = HasherFactory::create("sha256");
```

## Adding New Algorithm

1. Create `Sha3Hasher.hpp/.cpp` implementing `IHasher`
2. Register in `HasherFactory::create()`
3. Add to `CMakeLists.txt`
4. Done - no other changes needed

## Algorithms

| Algorithm | Size | Security | Git Usage    |
|-----------|------|----------|--------------|
| SHA-1     | 20B  | Broken   | Default      |
| SHA-256   | 32B  | Strong   | Future       |

See `docs/ARCHITECTURE.md` for full context.
