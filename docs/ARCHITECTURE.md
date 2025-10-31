# Gitter Architecture Documentation

## Directory Structure

```
src/
├── cli/                      # Command-line interface layer
│   ├── ICommand.hpp         # Command interface
│   ├── CommandFactory.*     # Factory pattern for command creation
│   ├── CommandInvoker.*     # Command executor with error handling
│   └── commands/            # Individual command implementations
│       ├── HelpCommand.*
│       ├── InitCommand.*
│       ├── AddCommand.*
│       ├── StatusCommand.*
│       ├── RestoreCommand.*
│       ├── CommitCommand.*  (stub)
│       ├── LogCommand.*     (stub)
│       └── CheckoutCommand.* (stub)
│
├── core/                     # Git core logic
│   ├── Repository.*         # Repository management (Singleton)
│   ├── ObjectStore.*        # Git object storage (blobs/trees/commits)
│   └── Index.*              # Staging area management
│
└── util/                     # General-purpose utilities
    ├── Logger.*             # Leveled logging system
    ├── Expected.hpp         # Result/error handling
    ├── IHasher.hpp          # Hash algorithm interface (Strategy Pattern)
    ├── HasherFactory.*      # Factory for creating hasher instances
    ├── Sha1Hasher.*         # SHA-1 implementation (Git default)
    ├── Sha256Hasher.*       # SHA-256 implementation (Git future)
    └── PatternMatcher.*     # Glob/regex pattern matching
```

## Layer Separation

### 1. Utility Layer (`src/util/`)
**Purpose:** Reusable, generic utilities with no Git-specific logic

- **IHasher / HasherFactory** - Cryptographic hashing (Strategy Pattern)
  - Pure utility, no domain knowledge
  - Supports SHA-1 (Git default) and SHA-256
  - Factory creates hasher instances by name
  - Used by ObjectStore for content addressing
  
- **PatternMatcher** - Glob pattern to regex conversion
  - Generic file pattern matching
  - Used by Add and Restore commands
  
- **Logger** - Leveled logging (error/warn/info/debug)
  - Environment-controlled via `GITTER_LOG`
  - Used throughout for diagnostics
  
- **Expected** - Result/error monad pattern
  - Type-safe error propagation
  - Avoids exceptions for control flow

### 2. Core Layer (`src/core/`)
**Purpose:** Git-specific domain logic and data structures

- **Repository** - Repository lifecycle and discovery
  - Singleton pattern for global state
  - Initializes `.gitter/` structure
  - Discovers repo root by walking up directories
  
- **ObjectStore** - Content-addressable object storage
  - Implements Git object format: `"<type> <size>\0<content>"`
  - Writes blobs to `.gitter/objects/<hash>`
  - Uses Hasher for SHA-256 computation
  
- **Index** - Staging area (Git index)
  - TSV format: `path\thash\tsize\tmtime`
  - Tracks files staged for next commit
  - Fast dirty detection via size/mtime

### 3. CLI Layer (`src/cli/`)
**Purpose:** Command-line interface and user interaction

- **ICommand** - Command interface (Strategy pattern)
  - `execute()` - runs the command
  - `name()`, `description()` - metadata
  - `helpNameLine()`, `helpSynopsis()`, etc. - detailed help
  
- **CommandFactory** - Factory for command creation
  - Maps string name to command creator
  - Enables dynamic command registration
  
- **CommandInvoker** - Command execution orchestrator
  - Wraps command execution with logging
  - Uniform error handling and reporting
  
- **Commands/** - Individual command implementations
  - Each command is self-contained
  - Uses core layer for domain operations
  - Uses util layer for common tasks

## Design Patterns Used

### 1. **Command Pattern**
- **Where:** `ICommand` interface, all command classes
- **Why:** Encapsulates operations as objects, enabling queuing and logging
- **Benefit:** Easy to add new commands without modifying existing code

### 2. **Factory Pattern**
- **Where:** `CommandFactory`
- **Why:** Creates commands dynamically from string names
- **Benefit:** Loose coupling between CLI parsing and command implementation

### 3. **Singleton Pattern**
- **Where:** `Repository`, `CommandFactory`, `Logger`
- **Why:** Single global instance for shared state
- **Benefit:** Consistent state across commands

### 4. **Facade Pattern**
- **Where:** `Repository` hides Index, ObjectStore, Refs complexity
- **Why:** Simplifies high-level repository operations
- **Benefit:** Commands don't need to know internal repo structure

### 5. **Strategy Pattern**
- **Where:** `PatternMatcher` (future: IHasher, IDiffStrategy)
- **Why:** Swap algorithms (glob matching, hashing) without changing clients
- **Benefit:** Extensible for SHA-1 vs SHA-256, etc.

### 6. **Template Method Pattern**
- **Where:** Potential `BaseCommand` (future enhancement)
- **Why:** Define skeleton algorithm with customizable steps
- **Benefit:** Reduce duplication in command validation/cleanup

## Data Flow

### Add Command Flow
```
User: gitter add *.cpp
  ↓
main.cpp parses args
  ↓
CommandFactory creates AddCommand
  ↓
CommandInvoker.invoke(AddCommand)
  ↓
AddCommand:
  1. PatternMatcher.matchFilesInWorkingTree("*.cpp")
  2. For each matched file:
     a. ObjectStore.writeBlobFromFile(file)
        - Reads file content
        - Creates Git blob: "blob <size>\0<content>"
        - Hasher.digest() computes SHA-256
        - Writes to .gitter/objects/<hash>
     b. Index.addOrUpdate(path, hash, size, mtime)
  3. Index.save()
  ↓
Success/Error returned through Expected<void>
```

### Status Command Flow
```
User: gitter status
  ↓
StatusCommand:
  1. Repository.discoverRoot() finds .gitter/
  2. Index.load() reads staged files
  3. Print "Changes to be committed"
  4. For each indexed file:
     a. Check if file exists (deleted?)
     b. Compare size/mtime (modified?)
     c. If suspicious: ObjectStore.hashFileContent() and compare
  5. PatternMatcher... (wait, Status doesn't use patterns)
     Collect untracked via filesystem scan
  6. Print categorized results
```

## Key Principles

### 1. **Separation of Concerns**
- Utilities have no Git knowledge
- Core has no CLI knowledge
- CLI orchestrates but doesn't implement Git logic

### 2. **Dependency Direction**
```
CLI Layer
   ↓ depends on
Core Layer
   ↓ depends on
Util Layer
   (Util never depends upward)
```

### 3. **Error Handling**
- Use `Expected<T, Error>` for recoverable errors
- Return errors, don't throw exceptions for control flow
- Each layer enriches error context

### 4. **Testability**
- Pure functions in util layer
- Mockable interfaces (ICommand)
- Dependency injection via constructors (future)

## Why Hashers are in Util

**Location:** `src/util/IHasher.hpp`, `Sha1Hasher.*`, `Sha256Hasher.*`, `HasherFactory.*`

**Reasoning:**
- SHA-1 and SHA-256 are **general-purpose** cryptographic hashes
- No Git-specific logic in implementation
- Could be reused for any content addressing
- Strategy Pattern allows swapping algorithms
- Aligns with util layer principle: "reusable, generic"

**Clean Separation:**
- Each hash algorithm in its own file pair (.hpp/.cpp)
- Factory pattern for creating instances
- Interface (IHasher) defines contract

**What's Git-specific:**
- `ObjectStore` knows to hash `"blob <size>\0<content>"`
- `Index` knows to store hashes for files
- Hashers just compute hashes of bytes

## Why PatternMatcher is in Util

**Reasoning:**
- Glob-to-regex conversion is generic
- File system pattern matching is reusable
- Multiple commands use it (Add, Restore, future: Commit, Checkout)
- No Git domain knowledge required

## What Stays in Core

- **Repository** - manages `.gitter/` structure
- **Index** - understands staging area format
- **ObjectStore** - understands Git object format

These have Git-specific knowledge and shouldn't move.

## Future Architecture Considerations

### Planned Additions

1. **TreeBuilder** (core layer)
   - Builds tree objects from index entries
   - Groups files by directory recursively

2. **CommitBuilder** (core layer)
   - Creates commit objects with metadata
   - Links to tree and parent commits

3. **RefManager** (core layer)
   - Manages `HEAD` and branch refs
   - Resolves symbolic refs

4. **DiffEngine** (core layer)
   - Compares trees/commits
   - Generates patch format output

### Extensibility Points

- **IHasher interface** - swap SHA-1/SHA-256/SHA3
- **IObjectStore interface** - support compression, packing
- **IDiffStrategy interface** - different diff algorithms

## Build System

CMake manages compilation:
- `gitter_lib` - static library with all logic
- `gitter` - executable linking lib + main.cpp

Benefits:
- Fast incremental builds
- Easy to add unit tests linking lib
- Can create multiple binaries if needed

## Documentation Standards

All public APIs documented with:
- `@brief` - one-line summary
- `@param` - parameter descriptions
- `@return` - return value semantics
- Usage examples in comments
- Cross-references to related components

## Summary

The architecture achieves:
- ✅ Clear separation of concerns
- ✅ Reusable utilities in dedicated layer
- ✅ Git logic isolated in core layer
- ✅ CLI as thin orchestration layer
- ✅ Extensible via design patterns
- ✅ Testable components
- ✅ Well-documented interfaces

