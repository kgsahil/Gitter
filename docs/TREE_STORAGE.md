# Git Tree Storage in Gitter

## How Git Stores Directory Structure

### 1. Index (Staging Area) - Flat File List

The index (`.gitter/index`) stores a **flat list** of all staged files with their full paths:

```
src/main.cpp        <blob-hash-1>  1024  1234567890
src/utils/helper.cpp <blob-hash-2>  512   1234567891
README.md           <blob-hash-3>  2048  1234567892
```

**Key Points:**
- Index does NOT store tree objects
- Each entry has: `path`, `blob-hash`, `size`, `mtime`
- Paths are relative to repository root
- Used for fast dirty detection (size/mtime check)

### 2. Tree Objects - Created at Commit Time

When you run `gitter commit`, the system:

1. **Reads all entries from the index**
2. **Groups files by directory**
3. **Creates tree objects recursively**

Example transformation:

**Index entries:**
```
src/main.cpp        -> blob abc123
src/utils/helper.cpp -> blob def456
README.md           -> blob 789xyz
```

**Created tree objects:**

#### Root Tree (hash: root_tree_hash):
```
tree <size>\0
100644 README.md <blob-789xyz>
040000 src <tree-src_tree_hash>
```

#### src/ Tree (hash: src_tree_hash):
```
tree <size>\0
100644 main.cpp <blob-abc123>
040000 utils <tree-utils_tree_hash>
```

#### src/utils/ Tree (hash: utils_tree_hash):
```
tree <size>\0
100644 helper.cpp <blob-def456>
```

### 3. Tree Object Format

Each tree object is stored in `.gitter/objects/<hash>` with format:

```
tree <content-size>\0
<mode> <name>\0<20-byte-hash>
<mode> <name>\0<20-byte-hash>
...
```

**Entry types:**
- `100644` - Regular file (blob)
- `100755` - Executable file (blob)
- `040000` - Directory (tree)
- `120000` - Symbolic link (blob)

### 4. Commit Object References Tree

Commit objects point to the **root tree**:

```
commit <size>\0
tree <root_tree_hash>
parent <parent_commit_hash>
author John Doe <john@example.com> 1234567890
committer John Doe <john@example.com> 1234567890

Commit message here
```

## Implementation in Gitter

### Current State
✅ Index stores flat file list with blob hashes  
✅ Blob objects created and stored  
✅ Pattern matching (glob) for add/restore  

### TODO (for commit command)
- Build tree objects from index entries
- Recursively create subtrees for directories
- Store tree objects in `.gitter/objects/`
- Create commit object pointing to root tree

### Example Workflow

```bash
# 1. Add files (creates blobs, updates index)
gitter add src/main.cpp src/utils/helper.cpp README.md

# Index now contains:
#   src/main.cpp -> abc123 (blob stored in objects/)
#   src/utils/helper.cpp -> def456 (blob stored)
#   README.md -> 789xyz (blob stored)

# 2. Commit (will create trees + commit)
gitter commit -m "Initial commit"

# Creates:
#   objects/abc123 (blob: main.cpp content)
#   objects/def456 (blob: helper.cpp content)
#   objects/789xyz (blob: README.md content)
#   objects/utils_tree_hash (tree: src/utils/)
#   objects/src_tree_hash (tree: src/)
#   objects/root_tree_hash (tree: root)
#   objects/commit_hash (commit: points to root_tree_hash)

# Updates:
#   refs/heads/main -> commit_hash
```

## Advantages of This Design

1. **Deduplication**: Identical files share same blob regardless of path
2. **Efficient diffs**: Compare tree hashes to detect directory changes
3. **Space efficient**: Unchanged subtrees reuse same tree object
4. **Fast operations**: Index provides fast staging, trees built only on commit

## Pattern Support

### Add Command Patterns
```bash
gitter add *.txt              # All .txt in current dir and subdirs
gitter add src/*.cpp          # All .cpp in src/
gitter add test?.py           # test1.py, test2.py, etc.
gitter add .                  # All files recursively
```

### Restore Command Patterns
```bash
gitter restore --staged *.txt       # Unstage all .txt files
gitter restore --staged src/*.cpp   # Unstage all .cpp in src/
```

Pattern matching uses regex internally:
- `*` → `.*` (any characters)
- `?` → `.` (single character)
- `.` → `\.` (literal dot)

## References

- [Git Internals - Git Objects](https://git-scm.com/book/en/v2/Git-Internals-Git-Objects)
- [Git Index Format](https://git-scm.com/docs/index-format)
- [Pro Git Book - Chapter 10](https://git-scm.com/book/en/v2/Git-Internals-Plumbing-and-Porcelain)

