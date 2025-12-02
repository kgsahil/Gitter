# Merge Command Implementation Plan

This document outlines the strategy for adding `gitter merge`, focusing on incremental delivery, reuse of the diff infrastructure, and alignment with Git semantics. The plan assumes the diff engine (backed by google/diff-match-patch) is available for textual comparisons.

---

## 1. Goals & Supported Scenarios

- **Fast-forward merge** when target branch is ancestor of the source
- **Three-way merge** (`HEAD`, merge base, source) with textual conflict markers
- **Detection of trivial conflicts** (binary files, deleted/modified cases)
- **Abort with clear errors** for unsupported situations (octopus merge, unrelated histories)

**Initial Non-Goals:**
- Automatic conflict resolution beyond textual markers
- Rename detection / automatic rename handling
- Merge commit signing, notes, rerere, or advanced strategies
- Full interactive conflict resolution UI

---

## 2. High-Level Architecture

1. **CLI Layer (`MergeCommand`)**
   - Parse syntax: `gitter merge <branch-or-commit>`
   - Flags: `--no-ff`, `--ff-only`, `--no-commit`, `--squash` (phased rollout)
   - Resolve target reference, invoke merge service, report conflicts/status

2. **Core Layer (`MergeService`)**
   - Locate merge base using commit graph utilities
   - Decide merge strategy (fast-forward vs. three-way)
   - Perform working tree/index updates, create merge commit when appropriate
   - Surface conflicts as structured results (for CLI rendering)

3. **Supporting Components**
   - `CommitGraph` helper: determine ancestry, merge base, detect divergent histories
   - `MergeResult` data class summarizing outcome (fast-forward, auto-merged, conflicts)
   - `Conflict` structure: file path, conflict type, optional conflict content
   - Reuse `DiffService`/`DiffEngine` for three-way merge and conflict detection

---

## 3. Data Structures

- `MergeMode` enum: `FastForward`, `Recursive`, `Squash`
- `ConflictType` enum: `Textual`, `BothModified`, `DeleteModify`, `Binary`
- `MergeResult` struct:
  - `status` (FastForwarded | Merged | Conflicts)
  - `createdCommitHash` (optional)
  - `conflicts` vector
  - `summary` stats (#autoMerged, #conflicts)
- `Conflict` struct:
  - `path`
  - `type`
  - `oursHash`, `theirsHash`, `baseHash`
  - `content` (for textual conflicts with conflict markers or segments)

---

## 4. Workflow Breakdown

### 4.1 Pre-Merge Checks

1. Verify clean working tree (no unstaged changes) unless `--commit` suppressed
2. Ensure repository root discovered; load index
3. Resolve references:
   - `ours` = `HEAD`
   - `theirs` = user-specified branch/commit
4. Reject merges with unrelated histories (no merge base) unless `--allow-unrelated-histories` (future)

### 4.2 Determine Merge Strategy

- **Fast-forward** if `ours` is ancestor of `theirs`
  - Update `HEAD` ref, move branch pointer, update working tree via checkout
  - Skip merge commit unless `--no-ff`
- **Normal merge** otherwise
  - Identify merge base (`mergeBase = LCA(ours, theirs)`)
  - Gather file maps (tree → path → blob hash) for base, ours, theirs

### 4.3 Three-Way Merge Algorithm

For each path in union of base/ours/theirs:
1. Build triad of blob hashes (`base`, `ours`, `theirs`)
2. Classify change case:
   - All identical → no action
   - Fast-add/delete cases (one side equals base, the other differs)
   - Both modified → run textual merge
3. Use `DiffEngine` for textual merge:
   - Run diff between `base`↔`ours` and `base`↔`theirs`
   - Perform standard three-way merge combining edit sequences
   - When conflicts arise, create conflict markers:
     ```
     <<<<<<< HEAD
     ours
     =======
     theirs
     >>>>>>> branch
     ```
4. Apply results to staging area / working tree:
   - Auto-merged files: write merged content, stage blob in index
   - Conflicted files: write conflict markers to working tree, stage as conflict (or leave unstaged); record in `MergeResult`
5. Handle binary or delete/modify conflicts by marking conflicts and leaving manual resolution

### 4.4 Finalization Steps

- If `--no-commit`: leave changes staged (or conflicts) without creating commit
- Otherwise:
  - Build `TreeBuilder` from index (which now contains merged state)
  - Create merge commit via `ObjectStore::writeCommit`, referencing both parents (`ours`, `theirs`)
  - Update branch ref (`Repository::updateHEAD`)

### 4.5 Fast-Forward Implementation Details

1. Update branch reference to target hash
2. Run checkout logic to sync working tree/index (reuse `CheckoutCommand` helpers)
3. Return `MergeResult` with status `FastForwarded`

---

## 5. CLI Behaviour & Messaging

- On success: print summary (`Fast-forward`, `Merge made by the 'recursive' strategy.`)
- On conflicts: list paths requiring resolution, instruct `gitter status` or `gitter merge --abort`
- Provide `--abort` and `--continue` commands (future enhancement) using merge state tracking (`MERGE_HEAD`, `MERGE_MSG`, `MERGE_MODE` files) similar to Git

---

## 6. Reusing Diff Infrastructure

- `DiffService` provides pairwise diffs required for three-way merge
- Three-way merge logic combines two sets of diffs to produce merged content
- Diff engine used for conflict detection (identifying overlapping edits)
- Unified diff also reused for conflict diagnostics (`gitter diff` after merge start)

---

## 7. Storage & Metadata

- Create `.gitter/MERGE_HEAD` to track second parent during conflicts
- Optionally store `.gitter/MERGE_MSG`, `.gitter/MERGE_MODE` for resumable merges
- Update index entries with merged blobs; mark conflict stages akin to Git (Stage 1/2/3) for advanced workflows (future)

---

## 8. Testing Strategy

- **Unit Tests**
  - Merge base calculation (linear, branching histories)
  - Three-way merge textual cases (non-overlapping edits, overlapping conflicts)
  - Conflict marker formatting

- **Integration Tests**
  - Fast-forward merges across branches
  - Merges with auto-resolved changes
  - Merges resulting in conflicts (textual, delete/modify)
  - `--no-commit` workflows

- **Regression Tests**
  - Ensure merge leaves repository in deterministic state (index and working tree)
  - Test merge abort/continue flows once implemented

---

## 9. Incremental Delivery Plan

1. **Milestone 1: Graph Utilities**
   - Implement commit ancestry checks, merge base calculation
   - Tests covering various commit graph shapes

2. **Milestone 2: Fast-Forward**
   - Implement `gitter merge` fast-forward path
   - Reuse existing checkout to move working tree

3. **Milestone 3: Three-Way Merge Infrastructure**
   - Introduce `MergeService`, triad comparison, conflict data structures
   - Integrate diff engine for textual merges, produce merged buffers with conflict markers

4. **Milestone 4: Merge Commit Creation**
   - Stage merged changes, create merge commit (two parents)
   - Respect `--no-commit`, `--ff-only`

5. **Milestone 5: Conflict Management Enhancements**
   - Persist merge state (`MERGE_HEAD`, etc.)
   - Add `gitter merge --abort/--continue`
   - Improve messaging and status output

6. **Milestone 6: Advanced Options (Future)**
   - Squash merges, fast-forward prevention, merge strategies (ours/theirs), rename detection

---

## 10. Risks & Mitigations

- **Complex conflict cases** → Extensive tests; allow manual resolution fallback
- **Binary files** → Flag conflicts early; require manual handling
- **Performance** → Cache tree/diff results, stream large blobs
- **User experience** → Provide clear guidance on resolving conflicts and completing merge
- **State management** → Ensure operations are atomic; if merge fails mid-way, provide safe abort path

---

## 11. Future Enhancements

- Merge strategy selection (`recursive`, `ours`, `theirs`)
- Automatic conflict resolution heuristics (take theirs/ours for specific paths)
- Enhanced conflict visualization via `gitter diff --merge`
- Support for octopus merges and merge drivers

---

## 12. Summary

By layering the merge implementation over the existing diff engine and core repository services, we can deliver a Git-compatible `gitter merge`. The roadmap guides us from foundational graph operations through fast-forward support to full three-way merging with conflict handling, ensuring incremental value and maintainability.
