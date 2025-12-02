# Diff Command Implementation Plan

This document outlines a comprehensive, step-by-step plan to add `gitter diff` functionality that mirrors Git’s capabilities. The focus is on incremental delivery, testability, and alignment with existing architecture (CLI → Core → Util).

---

## 1. Requirements & Scope

- **Primary Use Cases** ✅ **IMPLEMENTED**
  - ✅ `gitter diff` → working tree vs. index
  - ✅ `gitter diff --cached` (alias `--staged`) → index vs. HEAD
  - 🚧 `gitter diff <commit>` → working tree vs. commit (TODO)
  - 🚧 `gitter diff <commit> <commit>` → commit vs. commit (TODO)
  - 🚧 Optional path filters: `gitter diff [<range>] [--] [<path>…]` (TODO)
- **Output**: ✅ unified diff format (compatible with Git's default output)
- **Non-Goals (current iteration)**: rename detection, word-diff, binary patches, colour output

---

## 2. Architectural Additions

1. **CLI Layer**
   - Add `DiffCommand` in `src/cli/commands/`
   - Register command in `CommandFactory`
   - Parse flags (`--cached|--staged`, future: `--name-only`, `--stat`)
   - Determine comparison mode and path filters, then call core services

2. **Core Layer**
   - Introduce `DiffService` (facade) handling high-level scenarios
   - Use existing `Repository`, `Index`, `ObjectStore`, `TreeBuilder`
   - Provide APIs returning structured diff data (`Expected<DiffResult>`)

3. **Util/Core Helpers**
   - Implement diff algorithm (Myers or Patience) as `DiffEngine`
   - Add text preprocessing utilities (line splitting, binary detection)
   - Introduce data structures for diff hunks, file changes, metadata

---

## 3. Data Structures

- `DiffMode` enum: `WorkTreeVsIndex`, `IndexVsHead`, `WorkTreeVsCommit`, `CommitVsCommit`
- `DiffFileChange` struct:
  - `path`, `oldMode`, `newMode`
  - `oldHash`, `newHash`
  - `changeType` (Added|Deleted|Modified|Renamed|Binary)
  - `DiffHunks diff`
- `DiffHunk` struct:
  - `oldStart`, `oldLines`, `newStart`, `newLines`
  - `vector<DiffLine>` lines (`context`, `addition`, `deletion`)
- `DiffResult` struct:
  - metadata (mode, summary counts)
  - `vector<DiffFileChange>`

These allow CLI formatting, future features (stats, json, etc.).

---

## 4. Diff Algorithm Strategy

1. **Line Tokenization**
   - Normalize line endings (LF)
   - Split into vector of strings for unified output formatting
   - Detect binary by scanning for NUL byte or high binary ratio

2. **Library Integration**
   - Vendor `google/diff-match-patch` C++ port (single header/cc pair)
   - Wrap it inside `DiffEngine` (e.g., `src/core/DiffEngine.cpp`)
   - Configure optional cleanup passes (`CleanupSemantic`, `CleanupMerge`) via `DiffConfig`
   - Convert library `Diff` sequence (equal/insert/delete) into line-oriented changes

3. **Hunk Construction**
   - Track old/new line offsets while iterating edits
   - Merge adjacent edits and add configurable context (default ±3 lines)
   - Produce `DiffHunk` objects for CLI formatting

4. **Binary Handling**
   - If binary, mark `changeType = Binary` and skip hunks
   - CLI prints placeholder (`Binary files differ`)

---

## 5. Core Workflows

### 5.1 Working Tree vs. Index

1. Discover repository root via `Repository::discoverRoot`
2. Load index (`Index::load`)
3. For each indexed file:
   - Locate working tree file, detect modifications via size/mtime fast path
   - Hash with `ObjectStore::hashFileContent` when necessary
   - If changed or removed, produce blob pairs (index blob vs. working file)
   - Feed `std::string` contents into `DiffEngine`
4. Scan working tree for untracked files matching patterns (optional future)

### 5.2 Index vs. HEAD (`--cached`)

1. Resolve HEAD commit (`Repository::resolveHEAD`)
2. Fetch tree recursively using `ObjectStore::readTree`
3. Map tree entries to path → blob hash
4. Compare with index map; for each path produce blob pairs
5. Load blob data via `ObjectStore::readBlob` and diff through `DiffEngine`

### 5.3 Commit vs. Working Tree / Commit vs. Commit

- Similar to above, but both sides obtained via tree traversal
- For commit comparisons, operate entirely on stored blobs (no filesystem I/O)
- Use `DiffEngine` to compute hunks for each pair of blob contents

### 5.4 Path Filters

- Use `PatternMatcher` to resolve globs
- Apply filters early to limit diff scope

---

## 6. CLI Rendering

1. Format unified diff:
   - File headers: `diff --git a/path b/path`
   - Mode lines for new/removed executables
   - Index line with hashes: `index <old>..<new> <mode>`
   - Hunk headers produced from `DiffEngine` output: `@@ -oldStart,oldLen +newStart,newLen @@`
2. Line prefixes:
   - `-` (deletions), `+` (additions), ` ` (context)
   - Detect and label `No newline at end of file`
3. Colour support:
   - Integrate with `Logger` or simple ANSI toggles (`--color=always|auto|never` future enhancement)

---

## 7. Testing Strategy

- **Unit Tests**
  - `DiffEngine` with curated text cases (insertions, deletions, complex scenarios)
  - Binary detection logic
  - Hunk merging rules
- **Core Integration Tests**
  - Working tree vs. index scenario using temp directories
  - Index vs. HEAD with prepared repo state
  - Commit vs. commit with multi-file changes
- **CLI Tests**
  - Validate command parsing and formatted output (golden files or snapshot assertion)
  - Cover edge cases (no differences, binary files, large files, path filters)

---

## 8. Incremental Delivery Plan

1. **Milestone 1: Infrastructure** ✅ **COMPLETE**
   - ✅ Integrated STL-based `diff_match_patch` as a vendored dependency (`third_party/diff_match_patch`)
   - ✅ Created `DiffEngine` class in `src/core/`
   - ✅ Wrapped diff-match-patch with line-oriented conversion
   - ✅ Basic CLI command structure implemented

2. **Milestone 2: WorkTree vs. Index** ✅ **COMPLETE**
   - ✅ Implemented detection + diff output for working tree vs. index
   - ✅ Uses fast path optimization (size/mtime check) like StatusCommand
   - ✅ Handles modified, deleted files

3. **Milestone 3: Index vs. HEAD** ✅ **COMPLETE**
   - ✅ Support for `--cached` and `--staged` flags
   - ✅ Integration with HEAD lookup, commit parsing, and blob loading
   - ✅ Handles new files, modified files, and deleted files

4. **Milestone 4: Commit Comparisons** 🚧 **TODO**
   - 🔲 Add `<commit>` mode (working tree vs commit)
   - 🔲 Add `<commit1> <commit2>` mode (commit vs commit)
   - 🔲 Handle tree traversal for arbitrary commits
   - 🔲 Error handling for invalid commit hashes

5. **Milestone 5: Enhancements** 🚧 **FUTURE**
   - 🔲 Colour output, optional path filters
   - 🔲 Performance tuning (streaming large files, caching)
   - 🔲 Additional flags (`--name-only`, `--stat`)
   - 🔲 Binary file detection and handling

---

## 9. Risks & Mitigations

- **Diff Library Behaviour** → Align cleanup options with Git-like output; add config switches if needed
- **Performance on Large Files** → diff-match-patch holds strings in memory; stream or chunk large blobs if necessary
- **Windows Compatibility** → Normalize line endings (`CRLF → LF`) before diffing
- **Binary Detection Accuracy** → fallback to heuristics, allow manual override in future (`--binary` flag)
- **Testing Complexity** → Use dedicated test fixtures and helper builders to keep cases manageable; compare against known diffs

---

## 10. Future Extensions

- Rename detection via similarity index (compare blob hashes + content)
- Word diff (`--word-diff`) using diff-match-patch’s character-level capabilities
- `--stat`, `--shortstat`, `--patch-with-stat`
- External diff tool integration (respect `GITTER_EXTERNAL_DIFF`)
- JSON output mode for tooling integration

---

## 11. Summary

Implementing `gitter diff` requires coordinated updates across CLI, core services, and utility layers. By delivering in milestones—starting with a reusable diff engine and building up to full Git parity—we maintain testability, ensure incremental value, and leave room for advanced features without overcomplicating the initial release.
