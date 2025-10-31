# Gitter - Git-like Version Control System in C++20

A minimal Git-like CLI tool that mimics core Git functionality, built with C++20 for Linux.

## Features

### Implemented Commands

| Command | Description | Examples |
|---------|-------------|----------|
| `gitter help [command]` | Show help or detailed command help | `gitter help commit` |
| `gitter init [path]` | Initialize a new repository | `gitter init` or `gitter init myproject/` |
| `gitter add <pathspec>` | Stage files for commit | `gitter add file.txt`, `gitter add *.cpp`, `gitter add .` |
| `gitter commit -m <msg>` | Create a commit | `gitter commit -m "Fix bug"`, `gitter commit -am "Quick fix"` |
| `gitter status` | Show working tree status | `gitter status` |
| `gitter restore --staged <pathspec>` | Unstage files | `gitter restore --staged file.txt`, `gitter restore --staged *.cpp` |

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

## Implementation Details

### How Add Works

1. **Pattern Resolution**: If pathspec contains `*`, `?`, or `[`, treat as glob pattern
2. **File Discovery**: Recursively scan directories or match patterns
3. **Hash Computation**: Create Git blob object: `"blob <size>\0<content>"`
4. **Object Storage**: Write to `.gitter/objects/<sha256-hash>`
5. **Index Update**: Record `path`, `hash`, `size`, `mtime` in index

### How Status Works

1. **Load Index**: Read all staged files
2. **Print Staged**: Show "Changes to be committed"
3. **Detect Modified**:
   - Fast path: Compare size and mtime
   - Slow path: Recompute hash if suspicious
4. **Find Untracked**: Scan working tree for files not in index
5. **Print Results**: Categorize and display

### Tree Storage (for future commit)

See [docs/TREE_STORAGE.md](docs/TREE_STORAGE.md) for detailed explanation of how Git stores directory trees.

**Summary:**
- Index stores flat file list
- Trees built recursively at commit time
- Each tree object represents one directory level
- Commit points to root tree

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

```bash
# Create test repository
mkdir test && cd test
gitter init

# Add files
echo "hello" > file1.txt
echo "world" > file2.txt
gitter add *.txt
gitter status

# Unstage
gitter restore --staged file1.txt
gitter status
```

## Roadmap

### Current State ✅
- Repository initialization
- File staging with glob patterns
- **Commit creation with tree and commit objects**
- **Hierarchical tree building from index**
- **Commit history with parent references**
- Status detection (staged/modified/untracked)
- Unstaging with patterns
- Git-compliant blob/tree/commit object storage
- Zlib compression for objects
- 2-char directory structure (`.gitter/objects/<aa>/<bbb...>`)
- SHA-1 hashing (Git default) with SHA-256 support
- Strategy Pattern for hash algorithms
- File permissions tracking (executable bit)

### Next Steps 🚧
- `log` - Display commit history
- `checkout` - Switch branches and restore working tree
- Branch management (create, delete, list)
- Diff output for changes
- Merge detection (basic)
- Remote operations (future)

## Documentation

The code is well-documented with Doxygen-style comments and comprehensive guides:

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** - Overall project architecture and design patterns
- **[docs/COMMIT_IMPLEMENTATION.md](docs/COMMIT_IMPLEMENTATION.md)** - Commit creation with trees and objects
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

