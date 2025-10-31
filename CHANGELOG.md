# Changelog

All notable changes to the Gitter project will be documented in this file.

## [Unreleased]

### Added - Reliability & Code Quality Improvements (2025-11-02)

#### Error Handling & Reliability
- **File I/O Error Checking**
  - All file write operations now verify success after writes
  - Stream state validation (`out.good()`) after all writes
  - Automatic cleanup of partial writes on failure
  - Directory creation errors are properly checked and reported
  - File read operations validate stream state and handle failures gracefully

- **Atomic Index Writes**
  - Index writes now use atomic pattern: write to `.index.tmp`, then rename
  - Prevents index corruption on crashes or write failures
  - Only renames temp file if write succeeded completely
  - Automatic cleanup of temp files on failure
  - Matches Git's lock file behavior (simplified for MVP)

- **Path Normalization**
  - All paths normalized consistently when storing in index
  - Removes `./` prefix and normalizes slashes
  - Prevents duplicate index entries for same file
  - Ensures same file always has same path representation

- **Input Validation**
  - Hash format validation (40-char hex for SHA-1) before storing in index
  - Invalid entries skipped during index load (recover from corruption)
  - Numeric field conversion wrapped in try-catch to prevent crashes
  - Graceful recovery from corrupted index files

#### Code Quality
- **Constants Extraction**
  - Created `src/core/Constants.hpp` with centralized constants
  - Replaced magic numbers (40, 2, 10) with named constants
  - Improves maintainability and readability

- **Status Command Optimization**
  - Fast path: Skip expensive hash computation when size AND mtime match
  - Matches Git's performance optimization for large repositories
  - Only hashes files when size or mtime suggests change
  - Critical for performance with large file counts

- **Error Handling Consistency**
  - ObjectStore exceptions wrapped in try-catch in commands
  - Consistent conversion to `Expected<T>` pattern
  - Graceful error handling for non-critical failures (warnings)

#### Files Modified
- `src/core/Constants.hpp` (new) - Centralized constants
- `src/core/ObjectStore.cpp` - Added error checks, improved robustness
- `src/core/Index.cpp` - Atomic writes, path normalization, validation
- `src/cli/commands/AddCommand.cpp` - Exception handling
- `src/cli/commands/CommitCommand.cpp` - Error checks, exception handling
- `src/cli/commands/StatusCommand.cpp` - Optimization, exception handling
- `src/cli/commands/LogCommand.cpp` - Error checks, constants usage

### Added - Commit and Log Commands (2025-11-01)

#### Commit Implementation
- **TreeBuilder Class**
  - Converts flat index into hierarchical tree objects
  - Recursively builds trees from leaves to root
  - Creates Git-format tree entries: `<mode> <name>\0<binary-hash>`
  - Handles nested directories automatically

- **Commit Command**
  - `gitter commit -m <message>` - Create commit with message
  - `gitter commit -a -m <message>` - Auto-stage modified files (with warning)
  - `gitter commit -am <message>` - Shorthand for `-a -m`
  - Creates tree objects from index
  - Creates commit objects with metadata (author, committer, timestamp)
  - Updates branch references (HEAD)
  - Supports parent commit references

- **Commit Objects**
  - Git format: `commit <size>\0tree...\nparent...\nauthor...\n\n<message>`
  - Stores author/committer name, email, timestamp, timezone
  - Links to parent commits (supports merge commits)
  - Environment variable support: `GIT_AUTHOR_NAME`, `GIT_AUTHOR_EMAIL`

#### Log Implementation
- **Log Command**
  - `gitter log` - Display last 10 commits (newest first)
  - Traverses commit chain via parent pointers
  - Git-style formatted output with colors
  - Shows commit hash, author, date, and message

- **CommitObject Struct**
  - Parses commit objects from storage
  - Extracts all metadata (tree, parents, author, committer, message)
  - Helper methods: `shortHash()`, `shortMessage()`

- **ObjectStore Enhancements**
  - `readCommit(hash)` - Reads and parses commit objects
  - `writeTree(content)` - Writes tree objects
  - `writeCommit(content)` - Writes commit objects
  - All objects compressed with zlib

#### Status Command Fix
- **Three-Way Comparison**
  - Fixed status to compare HEAD vs Index vs Working Tree
  - Correctly shows "nothing to commit" after commit
  - Only shows staged changes when index differs from HEAD
  - Compares tree hashes efficiently

- **Proper State Detection**
  - Changes to be committed: Index ≠ HEAD
  - Changes not staged: Working Tree ≠ Index
  - Untracked files: In working tree but not in index

#### Documentation
- **New Documentation Files**
  - `docs/COMMIT_IMPLEMENTATION.md` - Complete commit creation guide
  - `docs/LOG_IMPLEMENTATION.md` - Commit history display guide
  - `docs/STATUS_FIX.md` - Status command three-way comparison fix

### Added - SHA-1 Strategy Pattern & Git-Compliant Storage (2025-10-31)

#### Hash Algorithm Refactoring
- **Strategy Pattern Implementation**
  - Created `IHasher` interface for pluggable hash algorithms
  - Implemented `Sha1Hasher` (Git's default, 20-byte digests)
  - Implemented `Sha256Hasher` (Git's future, 32-byte digests)
  - Added `HasherFactory` for creating hasher instances
  - Separated each algorithm into its own file pair (.hpp/.cpp)

- **File Structure**
  - `src/util/IHasher.hpp` - Hash algorithm interface
  - `src/util/HasherFactory.cpp` - Factory + utilities
  - `src/util/Sha1Hasher.hpp/.cpp` - SHA-1 implementation
  - `src/util/Sha256Hasher.hpp/.cpp` - SHA-256 implementation
  - Removed monolithic `Hasher.hpp/.cpp`

#### Git-Compliant Object Storage
- **2-Character Directory Structure**
  - Objects now stored in `.gitter/objects/<aa>/<bbb...>`
  - First 2 chars of hash = directory name
  - Remaining chars = filename
  - Matches Git's standard layout exactly

- **Zlib Compression**
  - All objects compressed with zlib before writing
  - Implements `deflate()` for compression
  - Implements `inflate()` for decompression
  - Matches Git's loose object format

- **Enhanced ObjectStore**
  - Constructor accepts `std::unique_ptr<IHasher>` for algorithm selection
  - Defaults to SHA-1 for Git compatibility
  - Added `readObject(hash)` method to decompress objects
  - Added `getObjectPath(hash)` helper for 2-char directory structure

#### Enhanced Index Format
- **New Fields**
  - `mode` (uint32_t) - File permissions (0100644 or 0100755)
  - `ctime` (uint64_t) - Creation/status change time

- **Updated Format**
  - Old: `path<TAB>hash<TAB>size<TAB>mtime`
  - New: `path<TAB>hash<TAB>size<TAB>mtime<TAB>mode<TAB>ctime`

- **File Permissions**
  - Detects executable bit on Unix-like systems
  - Normalizes to Git mode: 0100644 (regular) or 0100755 (executable)
  - Preserves across platforms

#### Documentation
- **New Documentation Files**
  - `docs/HASHER_ARCHITECTURE.md` - Strategy Pattern details and usage
  - `docs/SHA1_STRATEGY_PATTERN.md` - Git object storage implementation
  - `docs/REFACTORING_SUMMARY.md` - Hasher refactoring changelog
  - Updated `docs/ARCHITECTURE.md` - Reflected new structure
  - Updated `README.md` - Added new features and documentation links

### Changed
- **ObjectStore** now uses Strategy Pattern for hash algorithms
- **Index** now stores file permissions and ctime
- **AddCommand** now captures file permissions and ctime
- **CMakeLists.txt** updated to include new hasher files

### Technical Details

#### Hash Algorithm Comparison
| Algorithm | Digest Size | Git Status   | Our Support |
|-----------|-------------|--------------|-------------|
| SHA-1     | 20 bytes    | Default      | ✅ Default  |
| SHA-256   | 32 bytes    | Future       | ✅ Available|

#### Benefits
- ✅ Git-compatible object storage
- ✅ Proper compression reduces disk usage
- ✅ 2-char directory structure improves performance
- ✅ Strategy Pattern allows algorithm swapping
- ✅ File permissions preserved
- ✅ Modular, maintainable code structure

#### Migration Notes
- Existing `.gitter` repositories should be reinitialized
- Old flat object storage is incompatible with new 2-char structure
- Old index format is forward-compatible (missing fields default to 0)

## [Previous] - Initial Implementation

### Added
- Repository initialization (`gitter init`)
- File staging (`gitter add`)
  - Single files
  - Multiple files
  - Directories (recursive)
  - Glob pattern matching (`*.cpp`, `src/*.h`, etc.)
- Working tree status (`gitter status`)
  - Staged files
  - Modified files (with fast mtime/size check)
  - Untracked files
- Unstaging (`gitter restore --staged`)
  - Single files
  - Glob patterns
- Blob object storage
- Index management (TSV format)
- Pattern matching utilities
- Command Pattern architecture
- Factory Pattern for commands
- Singleton Repository manager
- Result/Error handling with `Expected<T>`
- Leveled logging system

### Design Patterns
- Command Pattern (all commands)
- Factory Pattern (`CommandFactory`, `HasherFactory`)
- Singleton (`Repository`, `Logger`)
- Strategy (`IHasher` interface)
- Facade (`Repository` hides complexity)

### Documentation
- Comprehensive inline documentation
- `docs/TREE_STORAGE.md` - Git tree object explanation
- `docs/ARCHITECTURE.md` - Project architecture guide
- `README.md` - Usage and examples

### Build System
- CMake 3.20+ with presets
- Linux-only configuration (WSL supported)
- C++20 standard
- zlib dependency for compression

---

## Version History

- **Current** - Commit/Log commands + Three-way status comparison + Git-compliant storage
- **Previous** - SHA-1 Strategy Pattern + Git-compliant storage
- **Initial** - Basic Git functionality (init, add, status, restore)

## Future Plans

See [README.md Roadmap](README.md#roadmap) for upcoming features:
- ✅ Commit command with tree object creation (DONE)
- ✅ Log command for commit history (DONE)
- Checkout command for branch switching
- Branch management (create, delete, list)
- Diff output for file changes
- Merge commits (basic merge support)
- Remote operations (push/pull/fetch)
- Tag support

