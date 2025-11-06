<div align="center">

# 🚀 Gitter

### A Minimal Git-like Version Control System Built with C++20

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-238%20passing-brightgreen.svg)](#testing)

**Built from scratch to understand Git internals**  
*A production-grade Git clone with Git-compatible object storage, branch management, and intelligent staging*

</div>

---

## ✨ Key Features

### 🎯 Core Functionality
- ✅ **Full Git Workflow** - Init, add, commit, status, log, checkout, reset
- ✅ **Git-Compatible Storage** - SHA-1 hashing, zlib compression, content-addressable objects
- ✅ **Branch Management** - Create, switch, and manage multiple branches
- ✅ **Intelligent Staging** - Fast size/mtime checks, glob patterns, auto-staging
- ✅ **Reliable & Robust** - Atomic writes, error recovery, input validation

### 🏗️ Architecture Excellence
- 🎨 **Clean Design Patterns** - Command, Factory, Strategy, Facade, Singleton
- 📦 **Modular Architecture** - Separated CLI, Core, and Utility layers
- 🔒 **Type-Safe Error Handling** - `Expected<T>` monad pattern
- 🧪 **Comprehensive Testing** - 238+ tests with GoogleTest

---

## 📋 Quick Command Reference

<div align="center">

| Category | Command | Description |
|----------|---------|-------------|
| **Repo** | `gitter init [path]` | Initialize new repository |
| **Stage** | `gitter add <file>` | Stage files/directories |
| **Stage** | `gitter add *.cpp` | Stage with glob patterns |
| **Commit** | `gitter commit -m "msg"` | Create commit with message |
| **Commit** | `gitter commit -am "msg"` | Auto-stage & commit |
| **Info** | `gitter status` | Show working tree status |
| **Info** | `gitter log` | Display commit history |
| **Branch** | `gitter checkout <branch>` | Switch to branch |
| **Branch** | `gitter checkout -b <branch>` | Create & switch branch |
| **Undo** | `gitter reset HEAD~1` | Reset to previous commit |
| **Undo** | `gitter restore --staged <file>` | Unstage files |
| **Inspect** | `gitter cat-file blob <hash>` | View object content |

</div>

---

## 🎬 Getting Started

### Prerequisites

**Option 1: Docker (Easiest - No Build Required!)**
- Docker & Docker Compose installed

**Option 2: Build from Source**
- **CMake 3.20+**
- **C++20 Compiler** (GCC 10+, Clang 12+, MSVC 2022+)
- **Linux/macOS/WSL** or **Windows**

### Quick Start with Docker 🐳

```bash
# Build and run interactively with welcome message!
docker-compose build
docker-compose run --rm gitter

# Or run directly without docker-compose
docker build -t gitter-cli:latest .
docker run -it --rm gitter-cli:latest
```

[Full Docker instructions →](docs/DOCKER_USAGE.md)

### Quick Build

```bash
# Configure and build
cmake --preset linux-debug
cmake --build --preset linux-debug-build

# Run it!
./build/linux-debug/gitter help
```

### Quick Example

```bash
# Initialize repository
gitter init

# Stage files
echo "Hello World" > file.txt
gitter add file.txt

# Create commit
gitter commit -m "Initial commit"

# View history
gitter log

# Check status
gitter status
```

---

## 🏛️ Architecture Overview

<div align="center">

```
┌─────────────────────────────────────────────────────────────┐
│                        User Commands                        │
│  init  add  commit  status  log  checkout  reset  restore  │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                    CLI Layer (Commands)                     │
│  • Command Pattern - Each command is a class                │
│  • Factory Pattern - Dynamic command creation               │
│  • Error handling with Expected<T>                          │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                  Core Layer (Git Logic)                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ Repository   │  │ ObjectStore  │  │   Index      │     │
│  │ • init()     │  │ • blobs      │  │ • staging    │     │
│  │ • branches   │  │ • trees      │  │ • metadata   │     │
│  │ • HEAD mgmt  │  │ • commits    │  │ • TSV format │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│  ┌──────────────┐  ┌──────────────┐                        │
│  │ TreeBuilder  │  │ CommitObject │                        │
│  │ • hierarchy  │  │ • parsing    │                        │
│  └──────────────┘  └──────────────┘                        │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                Utility Layer (Infrastructure)               │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐             │
│  │ IHasher   │  │ Pattern   │  │ Logger    │             │
│  │ • SHA-1   │  │ Matcher   │  │ • levels  │             │
│  │ • SHA-256 │  │ • globs   │  │ • colored │             │
│  └───────────┘  └───────────┘  └───────────┘             │
│  ┌───────────┐  ┌───────────┐                             │
│  │ Expected  │  │ FileMeta  │                             │
│  │ • errors  │  │ • stats   │                             │
│  └───────────┘  └───────────┘                             │
└─────────────────────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                    Repository Storage                        │
│  .gitter/                                                   │
│  ├── HEAD              (ref: refs/heads/main)              │
│  ├── index             (TSV: staged files)                 │
│  ├── objects/          (content-addressable)               │
│  │   └── <aa>/<bbb...> (SHA-1 compressed objects)         │
│  └── refs/heads/       (branch pointers)                   │
│      ├── main          (commit hash)                       │
│      └── feature       (commit hash)                       │
└─────────────────────────────────────────────────────────────┘
```

</div>

**📊 See [UML Diagram](docs/UML_DIAGRAM.md) for a complete class diagram showing all classes, relationships, and design patterns.**

---

## 🎯 Feature Highlights

### ⚡ Performance Optimizations
- **Fast Dirty Detection**: Size/mtime checks before expensive hashing
- **Atomic Writes**: Temp file pattern prevents index corruption
- **Content Deduplication**: Objects stored once by hash
- **Zlib Compression**: Reduces disk usage by ~90%

### 🔐 Reliability Features
- **Error Recovery**: Graceful handling of corrupt data
- **Input Validation**: Hash format checking, numeric validation
- **Base64 Paths**: Supports filenames with special characters (TAB, newlines)
- **File Permissions**: Tracks executable bit (mode 0100755)

### 🎨 Advanced Features
- **Glob Patterns**: `*.cpp`, `src/**/*.h`, `test?.py`
- **Auto-Staging**: `-a` flag stages tracked files automatically
- **Multi-Paragraph Commits**: Multiple `-m` flags create separate paragraphs
- **Smart Branch Switching**: Preserves staged files across branches
- **Three-Way Status**: Compares HEAD vs Index vs Working Tree

---

## 📚 Technical Details

### Design Patterns
| Pattern | Use Case | Benefits |
|---------|----------|----------|
| **Command** | Each CLI command is a class | Queue, log, test commands independently |
| **Factory** | Dynamic command creation | Loose coupling between parsing and execution |
| **Singleton** | Repository, Logger, Factory | Single global instance for shared state |
| **Strategy** | Pluggable hashing (SHA-1/SHA-256) | Swap algorithms without changing clients |
| **Facade** | Repository hides complexity | Simple API for complex operations |

### Git Object Storage

<div align="center">

| Object Type | Format | Example |
|------------|--------|---------|
| **Blob** | `blob <size>\0<content>` | `blob 12\0Hello World` |
| **Tree** | `tree <size>\0<entries>` | `tree 64\0100644 file.txt\0<hash>` |
| **Commit** | `commit <size>\0<metadata>` | `commit 234\0tree...\nparent...\nauthor...` |

</div>

**Storage Layout** (Git-compatible):
```
.gitter/objects/
└── <first-2-chars>/      # Directory: "ab"
    └── <remaining-38>    # File: "c123def456..."
```

**Compression**: All objects zlib-compressed before storage

### Index Format (TSV)

<div align="center">

```
path<TAB>hash<TAB>size<TAB>mtime<TAB>mode<TAB>ctime
```

**Example**:
```
src/main.cpp<TAB>abc123...<TAB>1024<TAB>mtime<TAB>33188<TAB>ctime
```
(Note: Full timestamps in practice, shown simplified here)

</div>

**Fields**:
- `path` - Base64-encoded relative path (handles special characters)
- `hash` - SHA-1 blob hash (40 hex chars)
- `size` - File size in bytes
- `mtime` - Modification time (nanoseconds)
- `mode` - File permissions (0100644 = file, 0100755 = executable)
- `ctime` - Creation time (nanoseconds)

---

## 🔧 Complete Build Guide

<div align="center">

### Platform Support

| Platform | Compiler | Status |
|----------|----------|--------|
| **Linux** | GCC 10+, Clang 12+ | ✅ Fully Supported |
| **macOS** | Clang 12+ | ✅ Fully Supported |
| **Windows** | MSVC 2022+* | ✅ Fully Supported |
| **WSL** | GCC 10+ | ✅ Fully Supported |

\* **Windows Note**: Visual Studio 2022 requires the **Desktop Development with C++** workload installed. If you encounter "could not find any instance of Visual Studio", install this workload via Visual Studio Installer.

</div>

### Build Commands

```bash
# Configure build
cmake --preset linux-debug

# Compile
cmake --build --preset linux-debug-build

# Build with tests
cmake --build build/linux-debug --target gitter_tests

# Release build
cmake --preset linux-release
cmake --build --preset linux-release-build

# macOS - Xcode Generator
cmake --preset macos-debug
cmake --build --preset macos-debug-build      # Build Debug configuration
cmake --build --preset macos-release-build    # Build Release configuration

# macOS - Ninja Generator (Alternative)
cmake --preset macos-ninja-debug
cmake --build --preset macos-ninja-debug-build

# Visual Studio 2022 (Windows) - Requires Desktop Development with C++ workload
cmake --preset windows-vs2022
cmake --build --preset windows-vs2022-debug   # Build Debug configuration
cmake --build --preset windows-vs2022-release # Build Release configuration
```

### Alternative: Traditional CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
./build/gitter help
```

### Convenience: Using the Wrapper Script

After building, you can use the `gitter` wrapper script from the project root:

```bash
# The script automatically finds the first available built gitter executable
./gitter help
./gitter status
./gitter add file.txt
./gitter commit -m "My changes"

# Or add to PATH for global access
export PATH="$PWD:$PATH"
gitter help
```

The wrapper searches build directories in this order:
1. `build/linux-release/gitter`
2. `build/linux-debug/gitter`
3. `build/macos-*/gitter` (all variants)
4. `build/windows-*/gitter.exe` (all variants)
5. Any other `build/*/gitter*` executables

### 🐳 Docker Support (No Build Required!)

Try Gitter instantly without installing dependencies:

```bash
# Build the image first
docker-compose build

# Run interactively with welcome message!
docker-compose run --rm gitter

# You're now in a full Linux Ubuntu bash shell with welcome message displayed
# Demo project is in /demo directory
gitter help
cd /demo
gitter init demo-repo
cd demo-repo
echo "Hello World" > file.txt
gitter add file.txt
gitter commit -m "Initial commit"
gitter log

# Exit (container is automatically removed with --rm)
exit

# Or use Docker directly
docker build -t gitter-cli:latest .
docker run -it --rm gitter-cli:latest
```

**See [Docker Usage Guide](docs/DOCKER_USAGE.md) for complete instructions and customization options.**

---

## 📖 Detailed Usage Examples

### 🔹 Working with Files

<details>
<summary><b>Stage Files</b> - Single, multiple, directories, patterns</summary>

```bash
# Single file
gitter add file.txt

# Multiple files
gitter add file1.txt file2.cpp

# Directory (recursive)
gitter add src/

# Current directory (all files)
gitter add .

# Glob patterns
gitter add *.cpp           # All .cpp files
gitter add src/*.h         # All .h files in src/
gitter add test?.py        # test1.py, test2.py, etc.
gitter add **/*.json       # All .json files recursively
```
</details>

<details>
<summary><b>Check Status</b> - Three-way comparison</summary>

```bash
gitter status

# Output example:
# On branch main
#
# Changes to be committed:
#   modified:   file1.txt
#
# Changes not staged for commit:
#   modified:   file2.cpp
#
# Untracked files:
#   newfile.py
```
</details>

<details>
<summary><b>Unstage Files</b> - Remove from staging area</summary>

```bash
# Single file
gitter restore --staged file.txt

# Glob patterns
gitter restore --staged *.cpp
gitter restore --staged src/*.h
```
</details>

### 🔹 Commits & History

<details>
<summary><b>Create Commits</b> - Single & multi-paragraph messages</summary>

```bash
# Basic commit
gitter commit -m "Fix bug in feature X"

# Auto-stage tracked files
gitter commit -am "Quick fix for bug"

# Multi-paragraph message
gitter commit -m "Summary line" -m "Detailed explanation" -m "Notes"
```
</details>

<details>
<summary><b>View History</b> - Commit logs</summary>

```bash
gitter log

# Shows last 10 commits with:
# - Commit hash (short)
# - Author & email
# - Date & timezone
# - Commit message
```
</details>

<details>
<summary><b>Inspect Objects</b> - View Git objects</summary>

```bash
# View blob content
gitter cat-file blob abc123def456...

# View tree entries
gitter cat-file tree def456ghi789...

# View commit metadata
gitter cat-file commit ghi789jkl012...

# Show object type
gitter cat-file -t abc123...

# Show object size
gitter cat-file -s abc123...
```
</details>

### 🔹 Branches & History

<details>
<summary><b>Branch Management</b> - Create, switch, manage branches</summary>

```bash
# Switch to existing branch
gitter checkout feature

# Create and switch to new branch
gitter checkout -b new-branch

# Switch back to main
gitter checkout main
```
**Note:** Checkout preserves staged files across branches (Git-compatible)
</details>

<details>
<summary><b>Reset Commits</b> - Undo commits</summary>

```bash
# Reset to previous commit (moves HEAD back)
gitter reset HEAD~1

# Reset two commits back
gitter reset HEAD~2

# Reset to current HEAD (clear index)
gitter reset HEAD
```
**Note:** `reset` clears the index, leaving working tree files as untracked
</details>

---

## 🔍 How It Works

<details>
<summary><b>Command Implementation Details</b></summary>

### Add Command Flow
```
Pattern Detection → File Discovery → Hash Computation → 
Object Storage (zlib) → Index Update
```

### Status Command Flow
```
Three-Way Comparison:
├─ Index vs HEAD → "Changes to be committed"
├─ Working Tree vs Index (fast size/mtime check)
│  └─ Hash only if changed → "Changes not staged"
└─ Find untracked files → "Untracked files"
```

### Commit Command Flow
```
Parse Args → Load Index → Auto-Stage (if -a) → 
Build Tree (recursive) → Check Duplicates → 
Create Commit Object → Update Branch Ref → Silent Success
```

### Checkout Command Flow
```
Parse Args → Resolve Branch → Intelligent Index Merge →
Restore Working Tree → Remove Files → Clean Dirs → Update HEAD
```

**Smart Index Merging**: Preserves staged uncommitted files across branches (Git-compatible)
</details>

<details>
<summary><b>Error Handling & Reliability</b></summary>

### Expected<T> Pattern

Type-safe error propagation without exceptions:

```cpp
Expected<void> result = command.execute(ctx, args);
if (!result) {
    std::cerr << result.error().message << "\n";
    return 1;
}
```

### Error Codes
- `InvalidArgs` - Invalid command arguments
- `NotARepository` - Not in a git repository
- `IoError` - File I/O failures
- `AlreadyInitialized` - Repo already exists
- `CorruptObject` - Corrupted object data
- `RefNotFound` - Branch/commit not found
- `EmptyIndex` - Nothing to commit

### Reliability Features
- ✅ **Atomic Index Writes**: Temp file + rename pattern
- ✅ **File I/O Validation**: All writes verified for success
- ✅ **Input Validation**: Hash format checking
- ✅ **Graceful Recovery**: Skips corrupted entries
- ✅ **Path Normalization**: Consistent paths prevent duplicates
</details>

<details>
<summary><b>Logging System</b></summary>

```bash
# Set debug level
export GITTER_LOG=debug

# Run command
gitter add file.txt
# [debug] Executing command: add
# [info ] Staging file: file.txt
# [info ] Created blob: abc123...
```

**Log Levels**: `error`, `warn`, `info`, `debug`
</details>

---

## 🧪 Testing

<div align="center">

### Test Coverage: **238 Tests** ✅

| Category | Count | Coverage |
|----------|-------|----------|
| **Unit Tests** | ~85 | Core components (Index, ObjectStore, TreeBuilder) |
| **Command Tests** | ~115 | All CLI commands with edge cases |
| **Integration Tests** | ~89 | Complete Git workflows |
| **Total** | **238** | **87-93% line coverage** |

</div>

### Running Tests

```bash
# Build and run all tests
cmake --build build/linux-debug --target gitter_tests
./build/linux-debug/gitter_tests

# Verbose output
./build/linux-debug/gitter_tests --gtest_brief=1

# Or use CTest
cd build/linux-debug
ctest --output-on-failure --verbose
```

### Test Highlights
- ✅ **Add Command**: 15+ tests (patterns, special chars, errors)
- ✅ **Commit Command**: 12+ tests (multi-message, auto-stage, duplicates)
- ✅ **Status Command**: 8+ tests (three-way comparison)
- ✅ **Log Command**: 18+ tests (traversal, formatting)
- ✅ **Checkout Command**: 23 tests (branch switching, file preservation, directory cleanup)
- ✅ **Reset Command**: 6+ tests (HEAD~n syntax, chain traversal)
- ✅ **Integration**: 30+ workflow tests

### Coverage Reports

See [docs/COVERAGE.md](docs/COVERAGE.md) for detailed coverage analysis.

---

## 🗺️ Roadmap

<div align="center">

### Status: **Core Features Complete** ✅

| Feature | Status | Notes |
|---------|--------|-------|
| **Basic VCS** | ✅ Complete | Init, add, commit, status, log |
| **Branch Management** | ✅ Complete | Create, switch, checkout |
| **History Management** | ✅ Complete | Reset HEAD~n, traverse chains |
| **Object Storage** | ✅ Complete | Blobs, trees, commits (Git-compatible) |
| **Pattern Matching** | ✅ Complete | Glob patterns, wildcards |
| **Reliability** | ✅ Complete | Atomic writes, error recovery |
| **Advanced Features** | 🚧 Planned | Diff, merge, remotes, tags |

</div>

### Current Features ✅
- ✅ Full Git workflow (init, add, commit, status, log)
- ✅ Branch management (create, switch, checkout)
- ✅ History traversal (reset HEAD~n)
- ✅ Git-compatible storage (SHA-1, zlib, content-addressed)
- ✅ Intelligent staging (fast size/mtime checks)
- ✅ Pattern matching (*.cpp, src/**/*.h)
- ✅ Error recovery (atomic writes, validation)
- ✅ 238 comprehensive tests

### Next Steps 🚧
- 🔲 **Diff Output** - Show file changes between commits
- 🔲 **Merge Commits** - Basic three-way merge
- 🔲 **Remote Operations** - push/pull/fetch
- 🔲 **Tags** - Annotated tags for releases
- 🔲 **Advanced Branch Ops** - Delete, rename branches

---

## 📚 Documentation

### Architecture & Design
- **[UML_DIAGRAM.md](docs/UML_DIAGRAM.md)** - Complete UML class diagram showing all classes and relationships
- **[ARCHITECTURE.md](docs/ARCHITECTURE.md)** - System architecture, design patterns, data flow
- **[HASHER_DESIGN.md](docs/HASHER_DESIGN.md)** - Strategy pattern for hash algorithms
- **[TREE_STORAGE.md](docs/TREE_STORAGE.md)** - How Git stores directory trees

### Implementation Guides
- **[COMMIT_IMPLEMENTATION.md](docs/COMMIT_IMPLEMENTATION.md)** - Commit object creation
- **[LOG_IMPLEMENTATION.md](docs/LOG_IMPLEMENTATION.md)** - Commit history traversal
- **[RESET_IMPLEMENTATION.md](docs/RESET_IMPLEMENTATION.md)** - Reset command design
- **[CHECKOUT_SUMMARY.md](docs/CHECKOUT_SUMMARY.md)** - Branch switching and checkout

### Testing & Quality
- **[COVERAGE.md](docs/COVERAGE.md)** - Test coverage analysis (238 tests)

### Deployment & Distribution
- **[DOCKER_USAGE.md](docs/DOCKER_USAGE.md)** - Run Gitter with Docker (no build required!)

### Code Quality
- **Doxygen Comments** - All public APIs documented
- **Inline Documentation** - Complex logic explained
- **Design Rationale** - Decisions documented in code

---

## 📖 Learning Resources

### Git Internals
- [Git Internals - Git Objects](https://git-scm.com/book/en/v2/Git-Internals-Git-Objects)
- [Git Index Format](https://git-scm.com/docs/index-format)
- [Pro Git Book](https://git-scm.com/book/en/v2)

### Design Patterns
- Command Pattern for CLI architecture
- Strategy Pattern for extensible hashing
- Factory Pattern for dynamic creation

---

## 🔧 Troubleshooting

### CMake Cache Issues

If you see errors like:
```
CMake Error: The current CMakeCache.txt directory ... is different than the directory ...
```

**Solution:** Delete the old build directory and reconfigure:
```bash
rm -rf build/linux-debug build/linux-release
cmake --preset linux-debug
cmake --build --preset linux-debug-build
```

This happens when the project is moved or copied to a different location, as CMake stores absolute paths in the cache.

### Build Fails

**Windows:** Ensure you have Visual Studio 2022 with "Desktop Development with C++" workload, or use WSL.

**macOS:** Install Xcode Command Line Tools: `xcode-select --install`

**Linux:** Install build essentials: `sudo apt install build-essential cmake ninja-build`

### Tests Fail

Some tests require timing differences. If tests are flaky, add small delays or run them individually.

---

## 📄 License

**Educational Project** - Feel free to use, modify, and learn from this codebase.

---

<div align="center">

**Built with ❤️ to understand Git internals**

*Stars appreciated if this project helped you learn! ⭐*

</div>

