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
│       ├── CommitCommand.*  # ✅ Fully implemented
│       ├── LogCommand.*     # ✅ Fully implemented
│       ├── StatusCommand.*  # ✅ Fully implemented (with three-way comparison)
│       ├── RestoreCommand.*
│       └── CheckoutCommand.* (stub)
│
├── core/                     # Git core logic
│   ├── Repository.*         # Repository management (Singleton)
│   ├── ObjectStore.*        # Git object storage (blobs/trees/commits)
│   ├── Index.*              # Staging area management
│   ├── TreeBuilder.*        # Builds tree objects from index
│   └── CommitObject.hpp     # Commit metadata structure
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
  - Writes blobs/trees/commits to `.gitter/objects/<aa>/<bbb...>`
  - Uses SHA-1 hasher (Git default) with SHA-256 support
  - Compresses objects with zlib
  - Parses commit objects via `readCommit()`
  
- **Index** - Staging area (Git index)
  - TSV format: `path\thash\tsize\tmtime\tmode\tctime`
  - Tracks files staged for next commit
  - Fast dirty detection via size/mtime
  - Stores file permissions (mode: 0100644, 0100755)
  - Tracks creation time (ctime)
  
- **TreeBuilder** - Builds Git tree objects
  - Converts flat index into hierarchical tree structure
  - Recursively builds trees from leaves to root
  - Creates Git-format tree entries: `<mode> <name>\0<binary-hash>`
  - Handles nested directories automatically
  
- **CommitObject** - Commit metadata structure
  - Parsed representation of Git commit objects
  - Stores tree, parents, author, committer, timestamp, message
  - Helper methods: `shortHash()`, `shortMessage()`

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
- **Where:** `IHasher` (SHA-1/SHA-256), `PatternMatcher` (future: IDiffStrategy)
- **Why:** Swap algorithms (hashing, glob matching, diff) without changing clients
- **Benefit:** Extensible for SHA-1 vs SHA-256, different diff algorithms, etc.

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
        - Hasher.digest() computes SHA-1 (default)
        - Compresses with zlib
        - Writes to .gitter/objects/<aa>/<bbb...>
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
  3. Resolve HEAD to get current commit hash
  4. Three-way comparison:
     a. Index vs HEAD:
        - Build tree from index: TreeBuilder.buildFromIndex()
        - Compare tree hash with HEAD commit tree
        - If different → "Changes to be committed"
     b. Working Tree vs Index:
        - For each indexed file: compare size/mtime
        - If suspicious: ObjectStore.hashFileContent() and compare
        - If different → "Changes not staged for commit"
     c. Working Tree vs Index:
        - Collect files in working tree not in index
        - → "Untracked files"
  5. Print categorized results
```

### Commit Command Flow
```
User: gitter commit -m "Message"
  ↓
CommitCommand:
  1. Parse arguments (-m message, optional -a flag)
  2. Repository.discoverRoot() finds .gitter/
  3. Index.load() reads staged files
  4. Check if index is empty (error if so)
  5. TreeBuilder.buildFromIndex():
     - Groups files by directory
     - Recursively builds trees from leaves to root
     - Creates tree objects: "tree <size>\0<mode> <name>\0<hash>..."
     - ObjectStore.writeTree() stores each tree
  6. Read HEAD to get parent commit hash
  7. Build commit object:
     - Format: "commit <size>\0tree <hash>\nparent <hash>\n..."
     - Include author/committer (from env vars or defaults)
     - Unix timestamp and timezone
     - Commit message
  8. ObjectStore.writeCommit() stores commit
  9. Update branch reference (.gitter/refs/heads/main)
  10. Print success: "[commit abc1237] Message"
  ↓
Success/Error returned through Expected<void>
```

### Log Command Flow
```
User: gitter log
  ↓
LogCommand:
  1. Repository.discoverRoot() finds .gitter/
  2. Resolve HEAD to get current commit hash
  3. Loop (up to 10 commits):
     a. ObjectStore.readCommit(hash):
        - Read compressed object from .gitter/objects/<aa>/<bbb...>
        - Decompress with zlib
        - Parse commit format
        - Extract: tree, parents, author, committer, message
     b. Display formatted commit:
        - Yellow commit hash
        - Author name and email
        - Formatted date and timezone
        - Indented commit message
     c. Follow first parent pointer
     d. Break if no parent (root commit)
  4. Stop after 10 commits or end of chain
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
- **ObjectStore** - understands Git object format (blob/tree/commit)
- **TreeBuilder** - understands Git tree structure and hierarchy
- **CommitObject** - understands Git commit metadata format

These have Git-specific knowledge and shouldn't move.

## Future Architecture Considerations

### Implemented Additions

1. ✅ **TreeBuilder** (core layer)
   - Builds tree objects from index entries
   - Groups files by directory recursively
   - Creates Git-format tree objects

2. ✅ **CommitObject** (core layer)
   - Struct for parsed commit metadata
   - Used by CommitCommand and LogCommand
   - Links to tree and parent commits

3. ✅ **Commit/Log Commands** (cli layer)
   - CommitCommand creates commits with trees
   - LogCommand displays commit history
   - Both fully implemented

### Planned Additions

1. **RefManager** (core layer)
   - Manages `HEAD` and branch refs
   - Resolves symbolic refs
   - Branch creation/deletion

2. **DiffEngine** (core layer)
   - Compares trees/commits
   - Generates patch format output
   - Shows file-level diffs

3. **CheckoutCommand** (cli layer)
   - Switch branches
   - Restore working tree from commit
   - Update HEAD reference

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

