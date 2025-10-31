# Reset Implementation Guide

## Overview

The `gitter reset` command moves the current HEAD reference to a previous commit and clears the index, leaving all subsequent changes in the working tree unindexed. This allows users to undo commits while preserving their working tree state.

## Command Syntax

```bash
gitter reset [HEAD~<n> | HEAD]
```

**Arguments:**
- `HEAD~n` - Reset to the commit `n` steps back from current HEAD (e.g., `HEAD~1`, `HEAD~2`)
- `HEAD` - Reset to current HEAD (no change, equivalent to doing nothing)

## Examples

### Basic Usage

```bash
# Reset to previous commit
gitter reset HEAD~1

# Reset two commits back
gitter reset HEAD~2

# Reset to current HEAD (no change)
gitter reset HEAD
```

## Architecture

### Components

```
gitter reset HEAD~1
       ↓
   ResetCommand
       ↓
   ┌───┴────┐
   │        │
ObjectStore HEAD/Index
   │        │
   └────┬───┘
        ↓
   .gitter/refs/heads/main
   .gitter/index
```

### Key Classes

1. **ResetCommand** - Orchestrates reset operation
2. **ObjectStore** - Reads commit objects and traverses parent chain
3. **Index** - Manages staging area (clearing)
4. **CommitObject** - Represents parsed commit metadata

## Implementation Details

### Reset Process Flow

```cpp
// 1. Parse target commit specifier
gitter reset HEAD~1
  ↓
steps = 1

// 2. Read current HEAD to get starting commit
Read .gitter/HEAD → "ref: refs/heads/main"
Read .gitter/refs/heads/main → "current_hash"
  ↓
currentHash = "abc123..."

// 3. Traverse parent chain to find target
CommitObject commit = store.readCommit(currentHash)
  ↓
Follow parent: parentHash = commit.parentHashes[0]
targetHash = "def456..."

// 4. Update branch reference
Write targetHash to .gitter/refs/heads/main
  ↓
HEAD now points to target commit

// 5. Clear index
Index::clear()
Index::save()
  ↓
.gitter/index now empty

// 6. No output (Git-like behavior)
```

### HEAD~n Syntax

The `HEAD~n` syntax follows Git's ancestry notation:

- `HEAD~0` or `HEAD` - Current commit
- `HEAD~1` - Parent commit (one step back)
- `HEAD~2` - Grandparent commit (two steps back)
- `HEAD~3` - Great-grandparent commit (three steps back)

**Implementation:**
1. Extract `n` from `HEAD~n` string using `std::stoi`
2. Traverse parent chain `n` times using `CommitObject::parentHashes`
3. Error if attempting to go beyond root commit

## Behavior

### What Changes

**Modified:**
- `.gitter/refs/heads/<branch>` - Branch reference updated to target commit
- `.gitter/index` - Cleared (all entries removed)

**Unchanged:**
- `.gitter/objects/` - All commits remain (history preserved)
- Working tree - All files remain unchanged
- Uncommitted changes - Still present in working directory

### Reset vs Other Commands

**Differences from `git reset`:**

| Aspect | Git (full) | Gitter (current) |
|--------|------------|------------------|
| Reset modes | `--soft`, `--mixed`, `--hard` | Always `--mixed` |
| Index handling | Clears or updates based on mode | Always clears |
| Working tree | Can be modified (--hard) | Never modified |
| Output | May show moved files | Silent |

**Current limitation:** 
- Does not restore index from target commit's tree
- Always clears index completely
- Future: Could add full tree restoration for mixed reset

## Error Handling

### Common Errors

**No arguments:**
```bash
gitter reset
# Error: reset: target commit required (e.g., HEAD~1)
```

**Invalid format:**
```bash
gitter reset invalid
# Error: reset: only HEAD and HEAD~n are supported
```

**Negative steps:**
```bash
gitter reset HEAD~-1
# Error: reset: negative steps not allowed
```

**Beyond root commit:**
```bash
gitter reset HEAD~10  # Only 3 commits exist
# Error: reset: cannot go back further, reached root commit
```

**No commits:**
```bash
gitter reset HEAD~1  # On empty repository
# Error: reset: no commits yet
```

## Use Cases

### 1. Undo Last Commit (Keep Changes)

```bash
# Create two commits
echo "v1" > file.txt
gitter add file.txt
gitter commit -m "First version"

echo "v2" > file.txt
gitter add file.txt
gitter commit -m "Second version"

# Undo last commit but keep changes
gitter reset HEAD~1

# Result:
# - HEAD points to "First version"
# - file.txt still contains "v2"
# - file.txt is now untracked
```

### 2. Undo Multiple Commits

```bash
# Create 3 commits
gitter commit -m "First"
gitter commit -m "Second"
gitter commit -m "Third"

# Go back 2 commits
gitter reset HEAD~2

# Result: HEAD now at "First"
# - "Second" and "Third" commits still exist (unreachable)
# - All changes from Second and Third are unindexed
```

### 3. No-Op Reset

```bash
gitter reset HEAD
# Result: No change, HEAD stays same
```

## Git Compatibility

### What Matches Git

✅ `HEAD~n` syntax supported  
✅ Parent chain traversal  
✅ Branch reference updates  
✅ Index clearing  
✅ Silent operation  
✅ Error messages for invalid operations  

### What Differs

⚠️ **No reset modes** - Git supports `--soft`, `--mixed`, `--hard`  
⚠️ **No tree restoration** - Index always cleared, never restored from target  
⚠️ **No ref reflog** - No history of resets (reflog not implemented)  

## Technical Details

### Commit Chain Traversal

```cpp
// Start at current commit
std::string targetHash = currentHash;

// Follow parent chain n steps back
for (int i = 0; i < steps; ++i) {
    CommitObject commit = store.readCommit(targetHash);
    if (commit.parentHashes.empty()) {
        // Error: reached root commit
        return Error{...};
    }
    targetHash = commit.parentHashes[0];
}
```

### Index Clearing

```cpp
Index index;
index.clear();  // Removes all entries
index.save(root);  // Writes empty index to disk
```

The index file is written with atomic pattern:
1. Write to `.index.tmp`
2. Atomically rename to `.index`
3. Ensures index integrity on crashes

### Silent Operation

Resets produce no output on success, matching Git's behavior:
- No "HEAD moved to..." message
- No list of unindexed files
- Only errors are displayed

## Safety Considerations

### Data Preservation

✅ **Commits are never deleted** - All objects remain in `.gitter/objects/`  
✅ **Working tree preserved** - Files unchanged, only index cleared  
✅ **Branch history intact** - Can recover by resetting forward  

### Data Loss Scenarios

⚠️ **Unreachable commits** - Commits after reset become orphaned  
⚠️ **Lost staging** - Cleared index cannot be recovered  
⚠️ **No reflog** - No automatic backup of HEAD movements  

## Future Enhancements

### 1. Reset Modes

**--soft:**
```bash
gitter reset --soft HEAD~1
# Moves HEAD but keeps index intact
```

**--mixed (default):**
```bash
gitter reset --mixed HEAD~1
# Moves HEAD and clears index (current behavior)
```

**--hard:**
```bash
gitter reset --hard HEAD~1
# Moves HEAD, clears index, and modifies working tree
```

### 2. Index Restoration

Currently, reset always clears the index. Future enhancement:
- Read target commit's tree object
- Recursively traverse tree structure
- Populate index with all files from tree
- Match Git's `--mixed` reset behavior

### 3. Reflog Support

Implement reference log (reflog) to track HEAD movements:
```bash
gitter reset HEAD@{1}  # Reset to previous HEAD position
```

### 4. Branch-Specific Resets

```bash
gitter reset origin/main  # Reset to remote branch
gitter reset feature~2    # Reset to specific branch commit
```

## Summary

The reset implementation:

✅ **Moves HEAD** - Updates branch reference to target commit  
✅ **Clears index** - Removes all staged files  
✅ **Preserves working tree** - Files unchanged  
✅ **Supports HEAD~n** - Intuitive ancestry syntax  
✅ **Error handling** - Validates input and bounds  
✅ **Silent operation** - Git-like behavior  
✅ **History preserved** - Commits never deleted  

The implementation provides a functional reset command that matches Git's core behavior for undoing commits! 🎉

