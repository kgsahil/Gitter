# Commit Implementation Guide

## Overview

This document explains the implementation of `gitter commit`, which creates Git-compliant commit objects from staged files.

## Architecture

### Components

```
gitter commit -m "message"
       ↓
   CommitCommand
       ↓
   ┌───┴────┐
   │        │
TreeBuilder ObjectStore
   │        │
   └────┬───┘
        ↓
    .gitter/objects/
```

### Key Classes

1. **TreeBuilder** - Builds hierarchical tree objects from flat index
2. **ObjectStore** - Writes blob/tree/commit objects with zlib compression
3. **CommitCommand** - Orchestrates commit creation

## Git Object Model

### 1. Blob Objects
```
blob <size>\0<file-content>
```

**Example:**
```
blob 12\0hello world\n
```

**Created by:** `ObjectStore::writeBlob()`

### 2. Tree Objects
```
tree <size>\0<entries>
```

**Entry format:** `<mode> <name>\0<20-byte-binary-hash>`

**Example tree:**
```
tree 74\0100644 file.txt\0<20-bytes>40000 src\0<20-bytes>
```

**Created by:** `TreeBuilder` + `ObjectStore::writeTree()`

### 3. Commit Objects
```
commit <size>\0tree <hash>
parent <hash>
author Name <email> <timestamp> <timezone>
committer Name <email> <timestamp> <timezone>

<commit message>
```

**Example:**
```
commit 180\0tree abc123...
parent def456...
author Gitter User <user@example.com> 1698765432 +0000
committer Gitter User <user@example.com> 1698765432 +0000

Initial commit
```

**Created by:** `CommitCommand` + `ObjectStore::writeCommit()`

## Implementation Details

### TreeBuilder Algorithm

**Input:** Flat index with entries like:
```
src/main.cpp     -> blob abc123
src/util/log.cpp -> blob def456
README.md        -> blob 789abc
```

**Process:**

1. **Group by directory**
   ```
   root/
     ├─ src/
     │   ├─ main.cpp (blob)
     │   └─ util/
     │       └─ log.cpp (blob)
     └─ README.md (blob)
   ```

2. **Build from leaves to root**
   ```
   # Build tree for src/util/
   tree(src/util): 100644 log.cpp <blob-hash>
   
   # Build tree for src/
   tree(src): 100644 main.cpp <blob-hash>
              040000 util <tree-hash-for-src/util>
   
   # Build root tree
   tree(root): 040000 src <tree-hash-for-src>
               100644 README.md <blob-hash>
   ```

3. **Return root tree hash**

**Output:** Hash of root tree object

### Commit Creation Flow

```cpp
// 1. Parse command arguments
gitter commit -m "Fix bug"
  ↓
message = "Fix bug"

// 2. Load index
Index index;
index.load(root);
  ↓
entries = {
  "src/main.cpp": { hash: "abc123...", mode: 100644, ... },
  "README.md":    { hash: "def456...", mode: 100644, ... }
}

// 3. Build tree
TreeBuilder::buildFromIndex(index, store)
  ↓
  - Build tree for src/: tree_hash_src
  - Build root tree: tree_hash_root
  ↓
treeHash = "789abc..."

// 4. Get parent commit
Read .gitter/HEAD → "ref: refs/heads/main"
Read .gitter/refs/heads/main → "parent_hash"
  ↓
parentHash = "def789..." (or empty if first commit)

// 5. Build commit content
tree 789abc...
parent def789...
author Gitter User <user@example.com> 1698765432 +0000
committer Gitter User <user@example.com> 1698765432 +0000

Fix bug

// 6. Write commit object
store.writeCommit(content)
  ↓
  - Hash: commit <size>\0<content>
  - Compress with zlib
  - Write to .gitter/objects/<aa>/<bbb...>
  ↓
commitHash = "123def..."

// 7. Update branch reference
Write commitHash to .gitter/refs/heads/main
  ↓
main now points to new commit

// 8. Print success
[commit 123def7] Fix bug
```

## File Mode Encoding

Git uses numeric file modes:

| Mode   | Meaning            | Example           |
|--------|--------------------|-------------------|
| 040000 | Directory (tree)   | `src/`            |
| 100644 | Regular file       | `file.txt`        |
| 100755 | Executable file    | `script.sh`       |
| 120000 | Symbolic link      | `link -> target`  |
| 160000 | Gitlink (submodule)| `vendor/lib`      |

Our implementation uses:
- `040000` for directories (trees)
- `100644` for regular files
- `100755` for executable files

## Command Line Flags

### `-m <message>`
**Required.** Specifies commit message.

```bash
gitter commit -m "Add feature"
```

### `-a`
**Optional.** Auto-stage all modified tracked files before committing.

```bash
gitter commit -a -m "Update docs"
```

**Note:** Currently prints warning (not fully implemented).

### `-am <message>`
**Shorthand** for `-a -m`.

```bash
gitter commit -am "Quick fix"
```

## Environment Variables

### Author Information

```bash
export GIT_AUTHOR_NAME="John Doe"
export GIT_AUTHOR_EMAIL="john@example.com"
gitter commit -m "Commit with custom author"
```

**Defaults:**
- Name: `"Gitter User"`
- Email: `"user@example.com"`

### Committer Information

Uses same environment variables as author:
- `GIT_AUTHOR_NAME`
- `GIT_AUTHOR_EMAIL`

Git also supports `GIT_COMMITTER_*` variables (not yet implemented).

## Example Usage

### First Commit (Root Commit)

```bash
gitter init
echo "hello" > file.txt
gitter add file.txt
gitter commit -m "Initial commit"
# Output: [root-commit 123abc7] Initial commit
```

**Creates:**
- Blob object for `file.txt`
- Tree object for root directory
- Commit object (no parent)
- Updates `.gitter/refs/heads/main`

### Subsequent Commit

```bash
echo "world" >> file.txt
gitter add file.txt
gitter commit -m "Add world"
# Output: [commit 456def7] Add world
```

**Creates:**
- New blob object for updated `file.txt`
- New tree object for root directory
- Commit object with parent reference
- Updates `.gitter/refs/heads/main`

### Commit with Subdirectories

```bash
mkdir src
echo "int main() {}" > src/main.cpp
echo "void log() {}" > src/util/log.cpp
gitter add .
gitter commit -m "Add C++ files"
# Output: [commit 789ghi7] Add C++ files
```

**Creates:**
- 2 blob objects (main.cpp, log.cpp)
- 2 tree objects (src/, src/util/)
- 1 root tree object
- 1 commit object
- Updates `.gitter/refs/heads/main`

## Verification

### Inspect Objects

```bash
# Find all objects
find .gitter/objects -type f

# Example output:
# .gitter/objects/12/3abc...
# .gitter/objects/45/6def...
# .gitter/objects/78/9ghi...
```

### Check Object Types

```bash
# Read object (requires decompression)
# Use git cat-file or custom tool
```

### Verify Commit Chain

```bash
# Read current commit
cat .gitter/refs/heads/main
# Output: 789ghi...

# Trace parent chain
# Read .gitter/objects/78/9ghi... → parent 456def...
# Read .gitter/objects/45/6def... → parent 123abc...
# Read .gitter/objects/12/3abc... → no parent (root)
```

## Error Handling

### Empty Index

```bash
gitter commit -m "Empty"
# Error: nothing to commit (index is empty)
```

**Solution:** Stage files with `gitter add`

### No Message

```bash
gitter commit
# Error: commit: no commit message provided (-m required)
```

**Solution:** Provide `-m` flag

### Not a Repository

```bash
cd /tmp
gitter commit -m "Test"
# Error: not a gitter repository
```

**Solution:** Run `gitter init` or `cd` to repository

## Technical Details

### SHA-1 vs SHA-256

Current implementation uses SHA-1 by default (Git standard).

**SHA-1 commit hash:**
```
[commit 1a2b3c4] Message
        ^^^^^^^ - 7 chars of 40-char SHA-1 hash
```

To use SHA-256, modify `ObjectStore` constructor:
```cpp
ObjectStore store(root, std::make_unique<Sha256Hasher>());
```

### Zlib Compression

All objects compressed before writing:

```cpp
// Compress
std::vector<uint8_t> compressed = zlibCompress(fullObject);

// Write
fs::path objPath = getObjectPath(hash);
std::ofstream out(objPath, std::ios::binary);
out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
```

**Benefits:**
- Reduced disk usage (typically 50-80% compression)
- Git-compatible format
- Fast compression/decompression with zlib

### 2-Character Directory Structure

Objects stored as: `.gitter/objects/<aa>/<bbb...>`

**Example:**
```
Hash: 123abc...
Path: .gitter/objects/12/3abc...
```

**Reason:** Prevents too many files in single directory, improving filesystem performance.

## Future Enhancements

### Implement -a Flag (Auto-Stage)

Currently prints warning. Should:
1. Compare working tree with index
2. Auto-stage all modified tracked files
3. Then create commit

### Multiple -m Flags

Git supports multiple `-m` flags:
```bash
git commit -m "Title" -m "Body paragraph 1" -m "Body paragraph 2"
```

Each becomes a separate paragraph.

### Amend Previous Commit

```bash
gitter commit --amend -m "Updated message"
```

Should replace previous commit instead of creating new one.

### Interactive Staging

```bash
gitter commit -p
```

Interactively select hunks to stage before committing.

### Commit Signing

```bash
gitter commit -S -m "Signed commit"
```

Sign commits with GPG key.

## Summary

The commit implementation:

✅ **Creates Git-compliant objects** - blob, tree, commit  
✅ **Builds hierarchical trees** - from flat index  
✅ **Compresses with zlib** - matches Git format  
✅ **Uses SHA-1 hashing** - Git default  
✅ **Updates branch references** - maintains commit chain  
✅ **Supports parent commits** - builds commit history  
✅ **Handles empty repositories** - root-commit without parent  

The implementation is fully functional and Git-compatible! 🎉

