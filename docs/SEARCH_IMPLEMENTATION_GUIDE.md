# Gitter Search Command – Implementation Guide

This guide mirrors the simplified implementation now in place. The command supports two modes:
- **Working tree search** `gitter search <term ...>`
- **Index search** `gitter search <term ...> --index`

The steps below outline how to implement/extend the feature.

---

## Phase 0 – Preparation

1. Review `docs/SEARCH_IMPLEMENTATION_PLAN.md` to understand requirements.
2. Ensure build/test tools work (`cmake --build`, `ctest`).
3. Create a feature branch.

---

## Phase 1 – CLI Wiring

1. Create `SearchCommand` (hpp/cpp) following existing command patterns.
2. Register the command in `CommandFactory` / `main.cpp`.
3. Basic parsing:
   - Collect positional terms.
   - Detect optional `--index` flag.
   - Throw usage error when no terms provided.
4. Forward parsed info to `SearchService::search(terms, mode)` and format results (frequency, line numbers).

---

## Phase 2 – SearchService Skeleton

1. Define data structures:
   ```cpp
   struct SearchHit { std::string path; uint64_t frequency; std::vector<uint32_t> lineNumbers; };
   struct SearchResult { std::vector<SearchHit> hits; uint64_t totalHits; uint64_t totalFiles; };
   enum class SearchMode { WorkingTree, Index };
   ```
2. Implement `SearchService::search(terms, mode)` with stubs for two helper methods.
3. Add term normalization helper (trim + lowercase + dedupe).
4. Create shared `searchInContent` routine that:
   - Accepts text buffer + normalized terms
   - Returns frequency + line numbers for matching lines
   - Skips binary content (presence of `\0`).

---

## Phase 3 – Working Tree Search

1. Use `std::filesystem::recursive_directory_iterator` starting at `std::filesystem::current_path()`.
2. Skip paths under `.gitter/` by examining relative path components.
3. Read each file as text, skip binary content.
4. Call `searchInContent`; if matches found, record `SearchHit` with relative path.
5. Update cache entries for each term that appeared (see Phase 5).
6. Sort hits by frequency then path.

---

## Phase 4 – Index/HEAD Search

1. Discover repo root via `Repository::discoverRoot`.
2. Load staging index (`Index::load`) to gather staged paths + blob hashes.
3. Resolve HEAD (`Repository::resolveHEAD`).
4. Collect committed paths by reading the HEAD tree using `ObjectStore::readCommit` + `readTree`.
5. For each path:
   - Prefer staged blob via `ObjectStore::readBlob(indexHash)`.
   - Otherwise, load HEAD tree entry.
   - Skip missing/binary blobs.
   - Run `searchInContent`; record `SearchHit` and update cache.
6. Sort and return results.

---

## Phase 5 – Cache Management

1. Cache file: `.gitter/search/cache.txt` (text format `term|path1,path2,...`).
2. Load cache lazily on first search.
3. After every search, rewrite cache with current map.
4. Prevent duplicates when appending new paths per term.

---

## Phase 6 – Output Formatting

1. Display aggregate summary: `Found X hit(s) in Y file(s)`.
2. For each hit, print `path (frequency match(es)): line1, line2, ...`.
3. Limit displayed line numbers (e.g., first 10) for readability.

---

## Phase 7 – Testing & Validation

- Manual checks:
  - Working tree mode finds untracked edits.
  - `--index` mode ignores unstaged changes but includes staged ones.
  - Cache file grows over successive searches.
- Future automated tests can target normalization, `searchInContent`, cache load/save, and integration flows.

---

## Phase 8 – Future Enhancements

- Cache invalidation keys (HEAD hash, index timestamp).
- Additional flags (case-sensitive, OR queries, regex).
- Snippet/preview output.
- Background cache refresh triggered by `add`/`commit`.

---

## Summary Checklist

- [ ] CLI parsing & formatting
- [ ] Working tree scanning
- [ ] Index/HEAD search using `ObjectStore`
- [ ] Shared `searchInContent`
- [ ] Cache load/save logic
- [ ] Manual tests for both modes

This guide reflects the current implementation and serves as a roadmap for future improvements.
