# Diff Command Summary

## Overview

The `gitter diff` command shows changes between the working tree, the index (staging area), and the last committed state (HEAD) in unified diff format (Git-compatible).

## Command Syntax

```bash
# Show unstaged changes (working tree vs index)
gitter diff

# Show staged changes (index vs HEAD)
gitter diff --cached
gitter diff --staged
```

> Note: Commit comparisons (`gitter diff <commit>`, `gitter diff <commit1> <commit2>`) are planned but not yet implemented.

## Key Features

- ✅ **Working tree vs index**: `gitter diff` shows unstaged modifications and deletions
- ✅ **Index vs HEAD**: `gitter diff --cached` / `--staged` shows staged changes that will be committed
- ✅ **Unified diff format**: `diff --git`, hunk headers, `+` additions, `-` deletions, and context lines
- ✅ **Diff engine**: Uses a vendored STL-based `diff_match_patch` implementation via `DiffEngine`
- ✅ **Performance-aware**: Reuses existing size/mtime optimizations from `status` where possible

## Architecture

```text
gitter diff [--cached]
       ↓
   DiffCommand
       ↓
   ┌───────────────┬───────────────┬─────────────┐
   │ Repository    │ Index         │ ObjectStore │
   └──────┬────────┴───────┬───────┴───────┬─────┘
          │                │               │
          ↓                ↓               ↓
   discoverRoot()     index entries   readBlob()/readTree()/readCommit()
          ↓                                ↓
       file pairs (old/new content)  →  DiffEngine::computeDiff()
```

**Key Classes:**
- `DiffCommand` – CLI command that parses flags, decides comparison mode, and prints unified diff
- `DiffEngine` – Core diff engine wrapping STL `diff_match_patch` and producing hunk/line structures
- `Repository` – Resolves repository root and HEAD
- `Index` – Provides staged file metadata and blob hashes
- `ObjectStore` – Reads blob, tree, and commit objects

## Implementation Status

- ✅ `gitter diff` – Working tree vs index
- ✅ `gitter diff --cached` / `--staged` – Index vs HEAD
- 🚧 `gitter diff <commit>` – Working tree vs commit (planned)
- 🚧 `gitter diff <commit1> <commit2>` – Commit vs commit (planned)
- 🚧 Path filters and extra flags (`--name-only`, `--stat`) (planned)

## Test Coverage

- **Core tests** (`test/core/test_diffengine.cpp`):
  - Simple additions, deletions, and modifications
  - No-diff cases and missing trailing newline edge case
- **Command tests** (`test/commands/test_diff.cpp`):
  - No differences (empty output)
  - Unstaged changes (working tree vs index)
  - Deleted files (working tree vs index)
  - Staged changes with `--cached` (index vs HEAD)
  - Rejection of commit-like arguments until commit mode is implemented


