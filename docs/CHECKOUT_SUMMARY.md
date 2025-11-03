# Checkout Command Summary

## Overview

The `gitter checkout` command allows switching between branches and creating new branches.

## Command Syntax

```bash
# Switch to existing branch
gitter checkout <branch-name>

# Create and switch to new branch
gitter checkout -b <branch-name>
```

## Key Features

### Branch Management
- ✅ Switch to existing branches
- ✅ Create new branches with `-b` flag
- ✅ Updates HEAD reference to point to branch
- ✅ Validates branch existence and conflicts

### Working Tree Operations
- ✅ **Restores files from branch commit** - Complete working directory restoration
- ✅ **Intelligent index merging** - Preserves staged uncommitted files across branches (Git-compatible)
- ✅ **File cleanup** - Removes files not in target branch
- ✅ **Directory cleanup** - Recursively removes empty directories

### Error Handling
- ✅ Branch doesn't exist: `'<branch-name>' does not exist`
- ✅ Branch already exists: `a branch named '<branch-name>' already exists`
- ✅ No commits yet: `checkout: no commits yet`

## Architecture

```
gitter checkout <branch>
       ↓
   CheckoutCommand
       ↓
   ┌───┴────────┐
   │            │
Repository  ObjectStore
   │            │
   └─────┬──────┘
         ↓
   .gitter/refs/heads/
   .gitter/HEAD
```

**Key Classes:**
- `CheckoutCommand` - Orchestrates checkout operation
- `Repository` - Manages HEAD and branch references
- `ObjectStore` - Reads commits and tree objects
- `TreeBuilder` - Traverses tree hierarchy

## Implementation Status

✅ **Phase 1:** Branch reference management - Complete  
✅ **Phase 2:** Working tree restoration - Complete (including file deletion and directory cleanup)  
⏸️ **Phase 3:** Safety and validation - Planned

The `gitter checkout` command fully supports creating and switching branches with complete working tree restoration, matching Git's core functionality.

## Test Coverage

- **13 unit tests** covering branch creation, switching, error handling, and tree operations
- **10 integration tests** covering complex workflows including staged file preservation, file deletion, directory cleanup, and Unicode support
- **Total: 23 comprehensive tests**

