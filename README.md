# Gitter - Git-like Version Control System in C++20

A minimal Git-like CLI tool that mimics core Git functionality, built with C++20 for Linux.

## Features

### Implemented Commands

| Command | Description | Examples |
|---------|-------------|----------|
| `gitter help [command]` | Show help or detailed command help | `gitter help commit` |
| `gitter init [path]` | Initialize a new repository | `gitter init` or `gitter init myproject/` |
| `gitter add <pathspec>` | Stage files for commit | `gitter add file.txt`, `gitter add *.cpp`, `gitter add .` |
| `gitter commit -m <msg>` | Create a commit | `gitter commit -m "Fix bug"`, `gitter commit -am "Quick fix"`, `gitter commit -m "Title" -m "Details"` |
| `gitter log` | Display commit history | `gitter log` (shows last 10 commits) |
| `gitter status` | Show working tree status | `gitter status` |
| `gitter restore --staged <pathspec>` | Unstage files | `gitter restore --staged file.txt`, `gitter restore --staged *.cpp` |
| `gitter reset <commit>` | Reset HEAD to a commit | `gitter reset HEAD~1`, `gitter reset HEAD~2` |
| `gitter checkout <branch>` | Switch branches | `gitter checkout feature`, `gitter checkout -b new-branch` |
| `gitter cat-file <type> <hash>` | Inspect Git objects | `gitter cat-file blob abc123...`, `gitter cat-file -t abc123...` |

### Pattern Matching Support

Both `add` and `restore` support glob patterns:

```bash
# Add patterns
gitter add *.txt              # All .txt files
gitter add src/*.cpp          # All .cpp in src/
gitter add test?.py           # test1.py, test2.py, etc.
gitter add .                  # All files recursively

# Restore patterns
gitter restore --staged *.txt      # Unstage all .txt
gitter restore --staged src/*.cpp  # Unstage .cpp in src/
```

## Architecture

### Design Patterns
- **Command Pattern**: Each git command is a separate class implementing `ICommand`
- **Factory Pattern**: `CommandFactory` creates commands dynamically
- **Singleton**: `Repository` manages global repo state
- **Strategy**: Pluggable hashing (SHA-1/SHA-256) via `IHasher` interface
- **Facade**: `Repository` hides internal complexity

### Core Components

```
src/
├── cli/
│   ├── ICommand.hpp              # Command interface
│   ├── CommandFactory.*          # Factory for command creation
│   ├── CommandInvoker.*          # Command executor with logging
│   └── commands/
│       ├── HelpCommand.*         # Help and usage display
│       ├── InitCommand.*         # Repository initialization
│       ├── AddCommand.*          # Staging files (with patterns)
│       ├── StatusCommand.*       # Working tree status
│       ├── RestoreCommand.*      # Unstaging files
│       └── ...                   # Other commands (stubs)
├── core/
│   ├── Repository.*              # Repo management and discovery
│   ├── Index.*                   # Staging area (index)
│   └── ObjectStore.*             # Git object storage (blobs/trees/commits)
└── util/
    ├── Expected.hpp              # Result/error handling
    ├── Logger.*                  # Leveled logging
    ├── IHasher.hpp               # Hash algorithm interface (Strategy)
    ├── HasherFactory.*           # Factory for creating hashers
    ├── Sha1Hasher.*              # SHA-1 implementation (Git default)
    ├── Sha256Hasher.*            # SHA-256 implementation
    └── PatternMatcher.*          # Glob pattern matching
```

### Repository Structure

```
.gitter/
├── HEAD                      # Current branch reference
├── index                     # Staging area (TSV format)
├── objects/                  # Content-addressable storage (zlib compressed)
│   └── <aa>/                # First 2 chars of hash
│       └── <bbb...>         # Remaining chars (blob/tree/commit objects)
└── refs/
    └── heads/
        └── main             # Branch tip commit hash
```

### Git Object Format

Objects are stored with Git-compatible format:

```
blob <size>\0<content>
tree <size>\0<entries>
commit <size>\0<metadata>
```

Each object is:
- Identified by SHA-1 hash (40 hex chars) or SHA-256 (64 hex chars)
- Compressed with zlib before storage
- Stored in `.gitter/objects/<aa>/<bbb...>` (2-char directory structure)

### Index Format

`.gitter/index` stores staged files in TSV:

```
path<TAB>hash<TAB>size<TAB>mtime<TAB>mode<TAB>ctime
src/main.cpp<TAB>abc123...<TAB>1024<TAB>1234567890000000000<TAB>33188<TAB>1234567890000000000
```

Fields:
- `path` - Relative path from repo root
- `hash` - SHA-1 or SHA-256 blob hash
- `size` - File size in bytes
- `mtime` - Modification time (nanoseconds)
- `mode` - File permissions (0100644 or 0100755)
- `ctime` - Creation/change time (nanoseconds)

## Build Instructions

### Prerequisites
- CMake 3.20+
- C++20 compiler (GCC 10+, Clang 12+)
- Linux environment (WSL supported)

### Build

```bash
# Configure
cmake --preset linux-debug

# Build
cmake --build --preset linux-debug-build

# Binary location
./build/linux-debug/gitter
```

Or use traditional CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/gitter help
```

## Usage Examples

### Initialize Repository

```bash
gitter init
# Creates .gitter/ in current directory
```

### Stage Files

```bash
# Single file
gitter add file.txt

# Multiple files
gitter add file1.txt file2.cpp

# Directory (recursive)
gitter add src/

# Current directory (all files)
gitter add .

# Pattern matching
gitter add *.cpp
gitter add src/*.h
```

### Check Status

```bash
gitter status
# Shows:
#  - Changes to be committed (staged)
#  - Changes not staged (modified/deleted)
#  - Untracked files
```

### Unstage Files

```bash
# Single file
gitter restore --staged file.txt

# Pattern
gitter restore --staged *.cpp
gitter restore --staged src/*.h
```

### Reset Commits

```bash
# Reset to previous commit
gitter reset HEAD~1

# Reset two commits back
gitter reset HEAD~2

# Reset to current HEAD (no change)
gitter reset HEAD
```

**Note:** `gitter reset` moves HEAD to the specified commit and clears the index, leaving all changes in the working tree unindexed.

### Checkout Branches

```bash
# Switch to existing branch
gitter checkout feature

# Create and switch to new branch
gitter checkout -b new-branch

# Switch back to main
gitter checkout main
```

**Note:** Checkout fully restores the working tree from the target branch's commit, including all files and subdirectories, and rebuilds the index to match.

### Inspect Objects

```bash
# View blob content
gitter cat-file blob abc123def456...

# View tree entries
gitter cat-file tree def456ghi789...

# View commit content
gitter cat-file commit ghi789jkl012...

# Show object type
gitter cat-file -t abc123...

# Show object size
gitter cat-file -s abc123...
```

## Implementation Details

### How Add Works

1. **Pattern Resolution**: If pathspec contains `*`, `?`, or `[`, treat as glob pattern
2. **File Discovery**: Recursively scan directories or match patterns
3. **Hash Computation**: Create Git blob object: `"blob <size>\0<content>"`
4. **Object Storage**: Compress with zlib and write to `.gitter/objects/<aa>/<bbb...>` (SHA-1)
5. **Index Update**: Record `path`, `hash`, `size`, `mtime`, `mode`, `ctime` in index

### How Status Works

1. **Three-Way Comparison**:
   - **Index vs HEAD**: Build tree from index, compare with HEAD tree hash
     - If different → "Changes to be committed"
   - **Working Tree vs Index**: Compare file hashes and metadata
     - If different → "Changes not staged for commit"
   - **Working Tree vs Index**: Find files not in index
     - → "Untracked files"
2. **Fast Detection (Git Optimization)**: 
   - Size/mtime check first - if both match, skip expensive hash computation
   - Only hash files when size OR mtime differs
   - Critical for performance with large repositories
   - Matches Git's behavior exactly
3. **Print Results**: Categorize and display in Git-style format

### How Commit Works

1. **Parse Arguments**: Extract `-m <message>` and optional `-a` flag
2. **Load Index**: Read all staged files
3. **Auto-Stage (if `-a` flag)**: For modified tracked files, re-hash and update index
4. **Build Tree**: `TreeBuilder` converts flat index into hierarchical tree objects
   - Groups files by directory recursively
   - Creates tree objects: `"tree <size>\0<mode> <name>\0<hash>..."`
   - Stores in `.gitter/objects/<aa>/<bbb...>`
5. **Check for Duplicates**: Compare tree hash with parent commit's tree hash
   - If identical, return "nothing to commit, working tree clean"
6. **Create Commit**: Build commit object with tree, parent, author, committer, message
   - Format: `"commit <size>\0tree...\nparent...\nauthor...\n\n<message>"`
   - Compress with zlib and store
7. **Update Branch**: Write commit hash to `.gitter/refs/heads/main`
8. **Silent Success**: No output (Git-like behavior)

### How Log Works

1. **Resolve HEAD**: Read `.gitter/HEAD` and follow to branch reference
2. **Traverse Chain**: Follow parent pointers from HEAD (up to 10 commits)
3. **Parse Commits**: For each commit:
   - Read compressed object from `.gitter/objects/<aa>/<bbb...>`
   - Decompress with zlib
   - Parse commit format: tree, parents, author, committer, message
4. **Display**: Git-style formatted output (hash, author, date, message)
5. **Stop**: At root commit or after 10 commits

### How Reset Works

1. **Parse Target**: Extract commit specifier (HEAD, HEAD~n)
2. **Resolve HEAD**: Read current commit hash from branch reference
3. **Traverse Chain**: Follow parent pointers back `n` steps to find target commit
4. **Update HEAD**: Write target commit hash to branch reference
5. **Clear Index**: Remove all staged files (changes become unindexed)
6. **Silent Success**: No output (Git-like behavior)

### How Checkout Works

1. **Parse Arguments**: Extract branch name and `-b` flag
2. **Resolve HEAD**: Get current commit hash for branch creation
3. **Branch Creation** (if `-b`):
   - Check if branch already exists
   - Create `.gitter/refs/heads/<branch>` with current commit hash
   - Update `.gitter/HEAD` to point to branch
   - Output: "Switched to a new branch '<branch-name>'"
4. **Branch Switching**:
   - Check if branch exists
   - Read target branch's commit hash
   - Read commit's tree object
   - **Restore Working Tree**: Recursively traverse tree and restore all files
   - **Rebuild Index**: Update index to match branch state
   - Update `.gitter/HEAD` to point to branch
   - Output: "Switched to branch '<branch-name>'"
5. **Error Handling**: Validate arguments, branch existence, and commits

### Tree Storage

See [docs/TREE_STORAGE.md](docs/TREE_STORAGE.md) for detailed explanation of how Git stores directory trees.

**Summary:**
- Index stores flat file list
- Trees built recursively at commit time by `TreeBuilder`
- Each tree object represents one directory level
- Commit points to root tree
- ✅ **Implemented**: `TreeBuilder` class creates trees from index

## Error Handling

Uses `Expected<T, Error>` pattern for explicit error propagation:

```cpp
Expected<void> result = command.execute(ctx, args);
if (!result) {
    std::cerr << result.error().message << "\n";
    return 1;
}
```

Error codes: `InvalidArgs`, `NotARepository`, `IoError`, `AlreadyInitialized`, etc.

### Reliability Features

- **File I/O Error Checking**: All write/read operations verified for success
- **Atomic Index Writes**: Index writes use temp file pattern to prevent corruption
- **Path Normalization**: Consistent path storage prevents duplicate entries
- **Input Validation**: Hash format validation prevents corrupted index entries
- **Graceful Recovery**: Corrupted index entries skipped automatically

## Logging

Set log level via environment variable:

```bash
export GITTER_LOG=debug
gitter add file.txt
# [debug] Executing command: add
# [info ] ...
```

Levels: `error`, `warn`, `info`, `debug`

## Testing

The project includes comprehensive unit and integration tests using GoogleTest.

### Running Tests

```bash
# Build tests (included in default build)
cmake --build build/linux-debug --target gitter_tests

# Run all tests
./build/linux-debug/gitter_tests

# Run with verbose output
./build/linux-debug/gitter_tests --gtest_brief=1

# Or use CTest
cd build/linux-debug
ctest --output-on-failure
```

### Test Coverage

- **Unit Tests**: Individual components (Index, ObjectStore, TreeBuilder, etc.)
- **Command Tests**: All CLI commands (init, add, commit, status, log, restore, reset)
- **Integration Tests**: Complete Git-like workflows combining multiple commands
- **Edge Cases**: Positive and negative scenarios, error handling

### Manual Testing

```bash
# Create test repository
mkdir test && cd test
gitter init

# Add files
echo "hello" > file1.txt
echo "world" > file2.txt
gitter add *.txt
gitter status

# Create first commit (single message)
gitter commit -m "Initial commit"
gitter status  # Should show: nothing to commit, working tree clean

# Multi-paragraph commit message
gitter commit -m "Summary" -m "Detailed explanation" -m "Notes"
gitter log  # View commit with blank lines separating paragraphs

# View commit history
gitter log

# Modify file
echo "more" >> file1.txt
gitter status  # Should show: Changes not staged for commit: modified: file1.txt

# Stage and commit
gitter add file1.txt
gitter commit -m "Update file1"
gitter log  # Should show 2 commits

# Unstage
gitter restore --staged file1.txt
gitter status

# Reset to previous commit
gitter reset HEAD~1
gitter log  # Should show only 1 commit
gitter status  # Reset files should be untracked
```

## Roadmap

### Current State ✅
- Repository initialization
- File staging with glob patterns
- **Commit creation with tree and commit objects**
- **Hierarchical tree building from index**
- **Commit history with parent references**
- **Log display (last 10 commits, newest first)**
- **Commit object parsing and traversal**
- **Auto-staging with `-a` and `-am` flags**
- **Duplicate commit prevention**
- **Silent commit output (Git-like)**
- **Reset command with HEAD~n syntax**
- Status detection (staged/modified/untracked) with Git optimization
- Unstaging with patterns
- Git-compliant blob/tree/commit object storage
- Zlib compression for objects
- 2-char directory structure (`.gitter/objects/<aa>/<bbb...>`)
- SHA-1 hashing (Git default) with SHA-256 support
- Strategy Pattern for hash algorithms
- File permissions tracking (executable bit)
- **Reliability**: Error checking, atomic writes, path normalization
- **Code quality**: Constants extraction, input validation, graceful recovery

### Next Steps 🚧
- Branch management (delete, rename branches)
- Diff output for file changes
- Merge commits (basic merge support)
- Remote operations (push/pull/fetch)
- Tag support

## Documentation

The code is well-documented with Doxygen-style comments and comprehensive guides:

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** - Overall project architecture and design patterns
- **[docs/COMMIT_IMPLEMENTATION.md](docs/COMMIT_IMPLEMENTATION.md)** - Commit creation with trees and objects
- **[docs/LOG_IMPLEMENTATION.md](docs/LOG_IMPLEMENTATION.md)** - Commit history display and parsing
- **[docs/RESET_IMPLEMENTATION.md](docs/RESET_IMPLEMENTATION.md)** - Reset command to undo commits
- **[docs/STATUS_FIX.md](docs/STATUS_FIX.md)** - Status command three-way comparison fix
- **[docs/HASHER_ARCHITECTURE.md](docs/HASHER_ARCHITECTURE.md)** - Strategy Pattern for hash algorithms
- **[docs/SHA1_STRATEGY_PATTERN.md](docs/SHA1_STRATEGY_PATTERN.md)** - Git-compliant object storage details
- **[docs/REFACTORING_SUMMARY.md](docs/REFACTORING_SUMMARY.md)** - Recent hasher refactoring changes
- **[docs/TREE_STORAGE.md](docs/TREE_STORAGE.md)** - How Git stores directory trees

Code Documentation:
- Class-level documentation explains purpose and usage
- Method documentation covers parameters, return values, and algorithms
- Inline comments explain complex logic

## References

- [Git Internals - Git Objects](https://git-scm.com/book/en/v2/Git-Internals-Git-Objects)
- [Git Index Format](https://git-scm.com/docs/index-format)
- [Pro Git Book](https://git-scm.com/book/en/v2)

## License

Educational project - feel free to use and modify.

