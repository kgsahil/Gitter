# Search Command Implementation Plan

This plan captures the simplified `gitter search` design currently implemented:

- `gitter search <term ...>` scans every file in the working directory (ignoring `.gitter/`).
- `gitter search <term ...> --index` inspects the staged index plus HEAD commit using blobs stored in `.gitter/`.
- A lightweight cache (`.gitter/search/cache.txt`) records term ➜ [paths] mappings to speed up repeated queries.

---

## 1. Requirements & Scope

- **Working tree search**: Treat repository like a fast, recursive grep of the working directory. Include tracked + untracked files; skip `.gitter/`.
- **Index search**: Ignore working-directory edits; search staged entries (from `Index`) plus committed files reachable from HEAD.
- **Cache**: Persist list of file paths per term to avoid re-scanning unchanged content. Cache refreshes opportunistically on every search.
- **Output**: For each matching file report number of matches + line numbers; results sorted by frequency.
- **Non-goals (v1)**: fuzzy search, ranking, phrase queries, background daemons, per-command incremental updates.

---

## 2. Architecture Overview

| Layer | Responsibility |
|-------|----------------|
| CLI (`SearchCommand`) | Parse terms/flags, choose search mode, format results |
| Core (`SearchService`) | Normalize terms, run working-tree or index-backed search, update cache |
| Utilities | Reuse existing `Repository`, `Index`, `ObjectStore` helpers |

Supporting helpers:
- `searchInContent()` – shared routine that scans a text buffer and returns frequency + matching lines.
- Simple text-based cache stored under `.gitter/search/cache.txt` (`term|path1,path2,...`).

---

## 3. Detailed Behaviour

### Working Tree Mode
1. Normalize terms (trim + lowercase + dedupe).
2. Recursively enumerate files under current directory; skip anything beneath `.gitter/`.
3. Read file contents; skip binaries (`'\0'` detection).
4. Run `searchInContent` to count matches and record line numbers.
5. Update cache entries for any term present in file; accumulate results.

### Index Mode
1. Discover repo root via `Repository::discoverRoot`.
2. Load staging index (`Index::load`) and collect staged paths + blob hashes.
3. Resolve HEAD via `Repository::resolveHEAD`; read tree/commit with `ObjectStore` to gather committed paths not already staged.
4. For each path, fetch blob content using `ObjectStore::readBlob`; skip binaries.
5. Invoke `searchInContent`; update cache for terms present; accumulate results.

### Cache Handling
- Cache loads lazily on first search; simple parse of `term|path1,path2` lines.
- After each search, flush in-memory cache to disk (overwriting file).
- Duplicate paths per term are prevented by `updateCacheEntry`.

---

## 4. CLI Behaviour & UX

- `gitter search apple banana` → working-tree scan
- `gitter search foo --index` → staged + HEAD scan
- Output format:
  ```
  Found 6 hit(s) in 2 file(s)
  src/foo.cpp (4 match(es)): 12, 18, 34, 56
  docs/readme.md (2 match(es)): 5, 98
  ```
- Limit to showing first ~10 line numbers for readability; append `...` when more exist.

---

## 5. Implementation Checklist

1. **CLI Parsing**
   - Split args into terms vs `--index` flag.
   - Call `SearchService::search(terms, mode)`.

2. **Term Normalization**
   - Trim whitespace.
   - Lowercase using ASCII rules.
   - Deduplicate using `unordered_set`.

3. **Working Tree Scan**
   - Use `std::filesystem::recursive_directory_iterator`.
   - Convert paths to relative format for output + cache.

4. **Index Scan**
   - Reuse `Index` to obtain staged entries.
   - Use `ObjectStore` to fetch blob contents for staged + HEAD commits.
   - Handle missing blobs / read failures gracefully.

5. **Cache**
   - Lazy-load text file; ignore malformed entries.
   - Ensure directory `.gitter/search` exists before saving.
   - Persist deterministic ordering (`term|path1,path2`).

6. **Shared Content Search**
   - `searchInContent` returns frequency + line numbers (one per matching line).
   - Skip binary buffers.

---

## 6. Testing Strategy

- **Unit tests (future)**:
  - Term normalization.
  - Content search (`searchInContent`).
  - Cache load/save round trips.
- **Manual verification**:
  - Working tree search finds untracked changes.
  - `--index` ignores unstaged edits but respects staged ones.
  - Cache file grows as search queries run.

---

## 7. Future Enhancements

- Add cache invalidation that tracks timestamp + HEAD hash to skip stale entries.
- Support additional operators (`OR`, case-sensitive, regex).
- Display line snippets / colored diffs.
- Optional: integrate with `add/commit/checkout` to pre-refresh cache.

---

## 8. Summary

`gitter search` now offers two complementary views:
- **Working tree mode** for fast local grepping of every file under the repo.
- **Index mode** for staged + committed snapshots using Git objects.

A simple on-disk cache provides quick lookup without complicated metadata. This plan reflects the current implementation and highlights areas for incremental improvements.
