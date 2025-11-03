# Reset Command Summary

## Overview

The `gitter reset` command moves the current HEAD reference to a previous commit and clears the index, leaving all subsequent changes in the working tree unindexed.

## Command Syntax

```bash
# Reset to previous commit
gitter reset HEAD~1

# Reset two commits back
gitter reset HEAD~2
```

## Key Features

- ✅ Moves HEAD to target commit
- ✅ Clears index (leaves files unindexed)
- ✅ Preserves working tree files
- ✅ Supports HEAD~n ancestry syntax
- ✅ Parent chain traversal
- ✅ Silent operation (Git-like)
- ✅ History preserved (commits never deleted)

## Architecture

```
gitter reset HEAD~1
       ↓
   ResetCommand
       ↓
   ┌───┴────┐
   │        │
ObjectStore Index
```

**Key Classes:**
- `ResetCommand` - Orchestrates reset operation
- `ObjectStore` - Reads commits and traverses parent chain
- `Index` - Manages staging area (clearing)

## Implementation Status

✅ Provides functional reset command matching Git's core behavior

## Test Coverage

- **6+ tests** covering HEAD~n syntax, chain traversal, error handling, and edge cases

