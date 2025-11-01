# Critical Edge Cases and Testing Gaps

## Overview

This document identifies untested edge cases that could break the implementation or cause unexpected behavior. These scenarios should be added to the test suite to ensure robustness.

## High Priority: File System Issues

### 1. Symlink Handling (NOT IMPLEMENTED)

**Current Behavior:** 
- Code uses `fs::is_regular_file()` which **follows symlinks** on most systems
- No explicit symlink detection or handling
- Symlinks pointing outside repo could be followed

**Test Scenarios Needed:**

#### Test: Symlink to file inside repo
```cpp
TEST_F(AddCommandTest, AddSymlinkToInternalFile) {
    // Create file
    createFile(tempDir, "real.txt", "content");
    
    // Create symlink pointing to it
    fs::create_symlink("real.txt", tempDir / "link.txt");
    
    // Add should follow symlink and hash target content
    // QUESTION: Should this add link.txt as a symlink blob (Git mode 120000)?
    // Or should it follow and add as regular file?
}
```

#### Test: Symlink to file outside repo
```cpp
TEST_F(AddCommandTest, AddSymlinkToExternalFile) {
    // Create symlink pointing outside repo
    fs::create_symlink("/etc/passwd", tempDir / "passwd-link");
    
    // Current behavior: fs::is_regular_file() will likely return false
    // But what happens if we explicitly add it?
}
```

#### Test: Symlink in subdirectory
```cpp
TEST_F(AddCommandTest, SymlinkInSubdirectory) {
    fs::create_directories(tempDir / "src");
    createFile(tempDir, "src/real.cpp", "code");
    fs::create_symlink("../src/real.cpp", tempDir / "src/link.cpp");
    
    // Add entire directory
    AddCommand cmd;
    cmd.execute(ctx, {"src/"});
    // What should happen?
}
```

#### Test: Broken symlink
```cpp
TEST_F(AddCommandTest, AddBrokenSymlink) {
    fs::create_symlink("nonexistent.txt", tempDir / "broken");
    
    AddCommand cmd;
    cmd.execute(ctx, {"broken"});
    // Should fail or skip gracefully?
}
```

**Impact:** HIGH - Could cause security issues, unexpected behavior, or data loss

---

### 2. Very Long File Paths

**Current Behavior:** 
- No explicit path length limits
- `std::filesystem::path` should handle most cases
- But index storage might have issues

**Test Scenarios Needed:**

```cpp
TEST_F(AddCommandTest, VeryLongFilePath) {
    // Create nested directories with long names
    std::string longName(255, 'a');
    fs::path deepPath = tempDir;
    for (int i = 0; i < 50; ++i) {
        deepPath /= longName;
    }
    fs::create_directories(deepPath);
    
    createFile(deepPath, "file.txt", "content");
    
    // Should handle path gracefully
    AddCommand cmd;
    auto result = cmd.execute(ctx, {"."});
    EXPECT_TRUE(result.has_value());
}
```

**Impact:** MEDIUM - Could crash on some filesystems

---

### 3. Special Characters in File Names ✅ IMPLEMENTED

**Current Behavior:**
- Index uses base64-encoded paths to handle special chars
- Base64 encoding prevents TSV parsing failures
- Git-compatible: supports TAB, newline, and any valid filename

**Implementation:**
- Added `base64Encode()` and `base64Decode()` helpers to `Index.cpp`
- Paths are base64-encoded when writing to index
- Paths are base64-decoded when reading from index
- Transparent to users: same API, automatic encoding

**Test Scenarios Implemented:**

```cpp
TEST_F(AddCommandTest, AddFileWithTabsInName) {
    // ✅ Tests TAB characters in filename
    // ✅ Verifies file added and tracked correctly
}

TEST_F(AddCommandTest, AddFileWithNewlinesInName) {
    // ✅ Tests newline characters in filename
    // ✅ Verifies file added and tracked correctly
}

TEST_F(AddCommandTest, IndexRoundTripSpecialCharacters) {
    // ✅ Verifies save/load integrity with special chars
    // ✅ Confirms hash preservation
}
```

**Impact:** RESOLVED - Base64 encoding prevents all special character issues

---

### 4. Concurrent File Modifications

**Current Behavior:**
- Fast check uses size/mtime
- If file changes between check and read, hash mismatch

**Test Scenarios Needed:**

```cpp
TEST_F(AddCommandTest, FileModifiedDuringAdd) {
    // Thread 1: Modify file rapidly
    // Thread 2: Add file
    // Should handle gracefully
    
    // This is tricky to test deterministically
    // But important for real-world use
}
```

**Impact:** LOW - Race condition, but rare in practice

---

### 5. Read-Only Files

**Current Behavior:**
- `getFileMetadata()` might fail for read-only files
- Check: Can we read size/mtime of read-only files?

**Test Scenarios Needed:**

```cpp
TEST_F(AddCommandTest, AddReadOnlyFile) {
    createFile(tempDir, "readonly.txt", "content");
    
    #ifndef _WIN32
    fs::permissions(tempDir / "readonly.txt", 
                   fs::perms::owner_read | fs::perms::group_read,
                   fs::perm_options::replace);
    #endif
    
    AddCommand cmd;
    auto result = cmd.execute(ctx, {"readonly.txt"});
    EXPECT_TRUE(result.has_value());
}
```

**Impact:** LOW - Should work on most systems

---

## High Priority: Index Integrity

### 6. Corrupt Index File

**Current Behavior:**
- Index::load() has some validation
- Skips invalid entries
- But what if entire file is garbage?

**Test Scenarios Needed:**

```cpp
TEST_F(IndexTest, LoadCorruptIndexEmptyFile) {
    // Write empty index file
    fs::path indexPath = tempDir / ".gitter" / "index";
    std::ofstream out(indexPath);
    out.close();
    
    Index index;
    EXPECT_TRUE(index.load(tempDir));
    EXPECT_TRUE(index.entries().empty());
}

TEST_F(IndexTest, LoadCorruptIndexTruncatedLine) {
    // Write index with incomplete entry
    fs::path indexPath = tempDir / ".gitter" / "index";
    std::ofstream out(indexPath);
    out << "file.txt\tabc123";  // Missing fields
    out.close();
    
    Index index;
    EXPECT_TRUE(index.load(tempDir));
    // Should skip invalid entry
    EXPECT_TRUE(index.entries().empty());
}

TEST_F(IndexTest, LoadIndexWithInvalidHash) {
    fs::path indexPath = tempDir / ".gitter" / "index";
    std::ofstream out(indexPath);
    out << "file.txt\tinvalid_hash_123\t1024\t1234567890\t33188\t1234567890\n";
    out.close();
    
    Index index;
    EXPECT_TRUE(index.load(tempDir));
    // Should skip entry with invalid hash format
    EXPECT_TRUE(index.entries().empty());
}
```

**Impact:** HIGH - Could cause silent data loss

---

### 7. Index File Permissions

**Test Scenarios Needed:**

```cpp
TEST_F(IndexTest, SaveIndexWithNoWritePermission) {
    // Make .gitter directory read-only
    fs::permissions(tempDir / ".gitter",
                   fs::perms::all & ~fs::perms::owner_write,
                   fs::perm_options::replace);
    
    Index index;
    // Try to save
    // Should fail gracefully
}
```

**Impact:** MEDIUM - Should fail gracefully with error

---

## High Priority: Object Store Issues

### 8. Duplicate Hash Collision

**Current Behavior:**
- SHA-1 has 2^160 possible values
- Collision theoretically possible
- Current code assumes unique hashes

**Test Scenarios Needed:**

```cpp
TEST_F(ObjectStoreTest, HandleHashCollision) {
    // Create two different files with same hash
    // This is extremely unlikely with SHA-1
    // But we should test behavior anyway
    
    // If collision occurs, second write should overwrite?
    // Or fail? Or deduplicate?
}
```

**Impact:** VERY LOW - Practically impossible

---

### 9. Corrupt Object Files

**Current Behavior:**
- Objects stored with zlib compression
- What if compression fails or file is truncated?

**Test Scenarios Needed:**

```cpp
TEST_F(ObjectStoreTest, ReadCorruptBlob) {
    ObjectStore store(tempDir);
    
    // Create valid blob
    std::string hash = store.writeBlob("test content");
    
    // Corrupt the file
    fs::path objPath = store.getObjectPath(hash);
    std::ofstream corrupt(objPath);
    corrupt << "corrupt data";
    corrupt.close();
    
    // Try to read
    // Should handle gracefully or throw
}
```

**Impact:** HIGH - Could crash or corrupt data

---

### 10. Object Directory Permission Issues

**Test Scenarios Needed:**

```cpp
TEST_F(ObjectStoreTest, WriteObjectWithNoPermission) {
    // Make .gitter/objects read-only
    fs::permissions(tempDir / ".gitter" / "objects",
                   fs::perms::all & ~fs::perms::owner_write,
                   fs::perm_options::replace);
    
    ObjectStore store(tempDir);
    // Try to write blob
    // Should fail gracefully
}
```

**Impact:** MEDIUM - Should handle gracefully

---

### 11. Concurrent Object Writes

**Current Behavior:**
- Objects written atomically via temp file + rename
- But what if two processes write same hash?

**Test Scenarios Needed:**

```cpp
TEST_F(ObjectStoreTest, ConcurrentWritesSameHash) {
    // Simulate two threads writing same blob simultaneously
    // Atomic writes should prevent corruption
}
```

**Impact:** LOW - Atomic writes should protect

---

## Medium Priority: Command Logic

### 12. Checkout with Large Working Tree

**Current Behavior:**
- restoreTree() recursively traverses and restores files
- Could be slow with thousands of files

**Test Scenarios Needed:**

```cpp
TEST_F(CheckoutCommandTest, CheckoutLargeWorkingTree) {
    // Create 1000+ files in complex hierarchy
    // Checkout to different branch
    // Should complete in reasonable time
    // Should remove all old files correctly
}
```

**Impact:** MEDIUM - Performance issue, not correctness

---

### 13. Status with Thousands of Untracked Files

**Test Scenarios Needed:**

```cpp
TEST_F(StatusCommandTest, StatusWithManyUntrackedFiles) {
    // Create 1000+ untracked files
    // Run status
    // Should list them all (or at least indicate count)
}
```

**Impact:** LOW - UX issue

---

### 14. Commit with Empty Tree (After All Deletions)

**Test Scenarios Needed:**

```cpp
TEST_F(CommitCommandTest, CommitEmptyTreeAfterDeletions) {
    // Create commit with files
    // Delete all files from working tree
    // Stage all deletions
    // Try to commit
    
    // What should happen?
    // Git allows empty tree commits
}
```

**Impact:** LOW - Edge case

---

### 15. Reset with Detached HEAD

**Current Behavior:**
- Reset operates on branch references
- Detached HEAD not explicitly tested

**Test Scenarios Needed:**

```cpp
TEST_F(ResetCommandTest, ResetDetachedHead) {
    // Checkout specific commit hash (detached HEAD)
    // Try to reset
    // Should handle gracefully
}
```

**Impact:** MEDIUM - Not currently supported

---

### 16. File Permissions During Checkout

**Current Behavior:**
- Checkout restores files but doesn't explicitly set permissions
- What about executable files?

**Test Scenarios Needed:**

```cpp
TEST_F(CheckoutCommandTest, CheckoutPreservesExecutablePermissions) {
    // On one branch: executable file
    // On another branch: non-executable version
    // Checkout between them
    // Verify permissions change correctly
}
```

**Impact:** MEDIUM - Permissions important for scripts

---

## Medium Priority: Pattern Matching

### 17. Pattern Matching Edge Cases

**Test Scenarios Needed:**

```cpp
TEST_F(AddCommandTest, PatternWithEscapedSpecialChars) {
    // What if user wants literal * or ? in filename?
    // Currently no escaping mechanism
}

TEST_F(AddCommandTest, PatternMatchingSymlinks) {
    // Pattern match might hit symlinks
    // Should it follow them?
}

TEST_F(AddCommandTest, PatternWithSpecialDirSeparators) {
    // Pattern like "src\\*.cpp" on Unix
    // Or "src/*.cpp" on Windows
}
```

**Impact:** LOW - Edge cases

---

## Low Priority: Encoding Issues

### 18. Unicode File Names

**Current Behavior:**
- `std::filesystem::path` should handle UTF-8 on most systems
- But path comparisons might have issues

**Test Scenarios Needed:**

```cpp
TEST_F(AddCommandTest, AddFileWithUnicodeName) {
    // Create file with emoji or CJK characters
    std::string unicodeName = "文件📄.txt";
    
    // Add, commit, checkout
    // Verify filename preserved correctly
}
```

**Impact:** LOW on Unix, HIGH on Windows

---

### 19. Binary File Handling

**Test Scenarios Needed:**

```cpp
TEST_F(AddCommandTest, AddBinaryFileWithNullBytes) {
    // Create file with null bytes
    std::vector<uint8_t> binaryData{0, 1, 2, 3, 0, 255};
    
    // Write to file
    // Add to index
    // Verify blob stored correctly
    
    // Read back
    // Verify content matches
}

TEST_F(AddCommandTest, AddVeryLargeBinaryFile) {
    // Create file > 1GB
    // Add, commit
    // Checkout
    // Verify integrity
}
```

**Impact:** MEDIUM - Important for real-world use

---

## Recommendations

### Immediate Priority

1. **Symlink handling** - Determine policy and implement consistently
2. ✅ **Special characters in filenames** - IMPLEMENTED (base64 encoding in index)
3. **Corrupt index recovery** - Robust parsing with error recovery
4. **Corrupt object handling** - Graceful decompression failures
5. **Unicode support** - Test on all target platforms

### Short Term

6. Permission handling during checkout
7. Large file support (>1GB)
8. Concurrent access protection
9. Detached HEAD support

### Long Term

10. Performance testing with thousands of files
11. Memory usage with large repositories
12. Stress testing with rapid file modifications

---

## Test Organization

**New Test Files Needed:**

1. ✅ Special characters now in `test_add.cpp` - TAB/newline filename support
2. `test/core/test_index_integrity.cpp` - Corrupt index, permissions, concurrent access
3. `test/core/test_object_store_edge_cases.cpp` - Corrupt objects, collisions, permissions
4. `test/integration/test_unicode.cpp` - Unicode filenames, encoding issues
5. `test/integration/test_large_repository.cpp` - Performance with many files
6. `test/integration/test_concurrent_operations.cpp` - Race conditions

**Integration with CI:**

- Add Windows runner to test permission behavior
- Add large-file tests with disk space monitoring
- Add memory leak detection for large repositories
- Add stress tests with rapid modifications

