# Critical Real-World Scenarios Where Gitter Could Fail vs Git

## Overview

These are real-world scenarios that users encounter daily where Gitter's behavior might differ from Git, causing confusion or data loss.

---

## 🚨 HIGH PRIORITY: File System Edge Cases

### 1. **Empty Directory Handling**
**Scenario:** Git tracks only files, not empty directories

**Current Status:** ✅ **Compatible** - We don't track empty dirs

**Potential Gap:**
```bash
# Scenario
mkdir empty_dir
gitter add empty_dir
gitter commit -m "Add empty dir"

# Expected: "fatal: pathspec 'empty_dir' did not match any files"
# Our behavior: Check if we handle this gracefully
```

**Test Needed:**
- ✅ `AddCommand` should skip empty directories
- ✅ `StatusCommand` shouldn't show empty dirs as untracked

---

### 2. **File Permission Changes Without Content Changes**

**Scenario:** Git tracks mode changes separately

```bash
# Scenario
chmod +x script.sh
# Content unchanged but permissions changed

git status  # Shows as "modified"
```

**Current Status:** ✅ **Partial** - We track mode but behavior unclear

**Test Needed:**
```cpp
TEST_F(StatusCommandTest, PermissionChangeWithoutContentChange) {
    // Create file and commit
    createFile(tempDir, "script.sh", "#!/bin/bash\necho hello");
    gitter.add("script.sh");
    gitter.commit("-m", "Initial");
    
    // Change permissions only
    setExecutable(tempDir / "script.sh", true);
    
    // Status should show: "Changes not staged: modified: script.sh"
    EXPECT_TRUE(statusOutput.find("modified: script.sh") != std::string::npos);
}
```

---

### 3. **Binary Files (Large Files, Null Bytes)**

**Scenario:** Git handles binary files differently

```bash
# Scenario
dd if=/dev/zero of=large.bin bs=1M count=100  # 100MB file
```

**Current Status:** ⚠️ **Untested**

**Potential Gaps:**
- Large file hashing performance
- Memory usage with huge files
- Binary vs text detection

**Test Needed:**
```cpp
TEST_F(AddCommandTest, LargeBinaryFile) {
    // Create 100MB zero file
    createLargeFile(tempDir / "big.bin", 100 * 1024 * 1024);
    
    auto result = addCmd.execute(ctx, {"big.bin"});
    EXPECT_TRUE(result.has_value());
    
    // Verify blob stored correctly
    Index index;
    index.load(tempDir);
    auto entry = index.entries()["big.bin"];
    EXPECT_GT(entry.sizeBytes, 100000000);
}
```

---

### 4. **Unicode and International Characters**

**Scenario:** Filenames with emoji, CJK, Arabic characters

```bash
# Scenario
touch "文件📄.txt"
touch "مرحبا.py"
```

**Current Status:** ⚠️ **Untested**

**Potential Gaps:**
- Path encoding issues on Windows
- Display of UTF-8 filenames
- Pattern matching with Unicode

**Test Needed:**
```cpp
TEST_F(AddCommandTest, UnicodeFilenames) {
    createFile(tempDir, "文件.txt", "content");
    createFile(tempDir, "مرحبا.py", "print('hi')");
    createFile(tempDir, "тест.cpp", "int main() {}");
    createFile(tempDir, "文件📄.txt", "emoji");
    
    auto result = addCmd.execute(ctx, {"."});
    EXPECT_TRUE(result.has_value());
    
    // Verify all files indexed correctly
    Index index;
    index.load(tempDir);
    EXPECT_EQ(index.entries().size(), 4);
}
```

---

### 5. **Hard Links**

**Scenario:** Same file, multiple names

```bash
# Scenario
echo "content" > file1.txt
ln file1.txt file2.txt  # Hard link

gitter add file1.txt file2.txt
```

**Current Status:** ❌ **Not handled** - Won't detect they're the same file

**Git Behavior:** Treats as separate files (they have different paths)

**Our Behavior:** Should match Git

---

## 🔴 MEDIUM PRIORITY: State Transitions

### 6. **Rapid File Modifications** 

**Scenario:** File modified between status checks

```cpp
// Scenario: Race condition
Thread 1: gitter status (checks mtime)
Thread 2: Modify file rapidly
Thread 1: File now has different content but same mtime

// Issue: Our fast check uses size+mtime, if both match we skip hash
```

**Current Status:** ⚠️ **Rare but possible**

**Test Needed:**
```cpp
TEST_F(AddCommandTest, FileModifiedDuringOperation) {
    createFile(tempDir, "file.txt", "version1");
    addCmd.execute(ctx, {"file.txt"});
    
    // Rapid modification between add and status
    std::thread writer([&]() {
        for (int i = 0; i < 100; ++i) {
            createFile(tempDir, "file.txt", "version" + std::to_string(i));
        }
    });
    
    statusCmd.execute(ctx, {});
    writer.join();
    
    // Status should detect modification eventually
}
```

---

### 7. **Staging State vs Working Tree Divergence**

**Scenario:** Complex staging scenarios

```bash
# Scenario 1: Modify staged file
echo "v1" > file.txt
gitter add file.txt
echo "v2" > file.txt  # Modify after staging

gitter status
# Expected: file.txt in BOTH "Changes to be committed" AND "Changes not staged"
```

**Current Status:** ✅ **Implemented** - Three-way comparison handles this

**Test Needed:**
```cpp
TEST_F(GitWorkflowTest, FileModifiedAfterStaging) {
    // Stage file
    createFile(tempDir, "file.txt", "v1");
    addCmd.execute(ctx, {"file.txt"});
    
    // Modify after staging
    createFile(tempDir, "file.txt", "v2");
    
    // Status should show file in both categories
    std::string status = captureStatus();
    EXPECT_TRUE(status.find("Changes to be committed") != std::string::npos);
    EXPECT_TRUE(status.find("Changes not staged") != std::string::npos);
    EXPECT_TRUE(status.find("modified: file.txt") != std::string::npos);
    // How many times? Count occurrences
}
```

---

### 8. **Branch Divergence After Reset**

**Scenario:** Reset then commit creates divergent history

```bash
# Scenario
commit1 -> commit2 -> commit3 (HEAD)
gitter reset HEAD~1  # HEAD now at commit2
# Make new commit4

# Now: commit1 -> commit2 -> commit4
# But commit3 still exists in objects/ (orphaned)
```

**Current Status:** ⚠️ **Objects orphaned but not cleaned**

**Git Behavior:** Objects remain until garbage collected

**Our Behavior:** ✅ **Matches** - Objects not deleted

**Test Needed:**
```cpp
TEST_F(ResetCommandTest, OrphanedCommitsAfterReset) {
    // Create 3 commits
    createCommit("c1");
    auto commit2Hash = createCommit("c2");
    auto commit3Hash = createCommit("c3");
    
    // Reset to commit2
    resetCmd.execute(ctx, {"HEAD~1"});
    
    // Verify commit3 still exists in objects/
    ObjectStore store(tempDir);
    EXPECT_NO_THROW(store.readCommit(commit3Hash));
    
    // Create new commit
    auto commit4Hash = createCommit("c4");
    
    // Verify new commit doesn't reference commit3
    CommitObject c4 = store.readCommit(commit4Hash);
    EXPECT_NE(c4.parentHashes[0], commit3Hash);
}
```

---

### 9. **Checkout with Staged Deletions**

**Scenario:** Complex checkout state

```bash
# Scenario
rm file1.txt
gitter add file1.txt  # Stage deletion

gitter checkout other-branch
# What happens to staged deletion?
```

**Current Status:** ⚠️ **Untested complex scenario**

**Git Behavior:** Staged deletions preserved, working tree checked out

**Test Needed:**
```cpp
TEST_F(CheckoutCommandTest, CheckoutWithStagedDeletions) {
    // Setup: two branches
    createCommit("Initial");
    checkout("-b", "feature");
    createFile("file.txt", "content");
    commit("-m", "Add file");
    
    checkout("main");  // file.txt shouldn't exist here
    
    // Delete and stage on main
    addCmd.execute(ctx, {"file.txt"});  // Stage deletion
    createFile("file.txt", "deleted");
    addCmd.execute(ctx, {"file.txt"});
    
    // Checkout with staged deletion
    checkout("feature");
    
    // file.txt should exist (from feature branch)
    // Deletion should not be staged anymore
}
```

---

### 10. **Commit -am with Staged and Unstaged Changes**

**Scenario:** Auto-stage only affects tracked files

```bash
# Scenario
touch tracked.txt
gitter add tracked.txt && gitter commit -m "Add"
echo "change" > tracked.txt  # Modified

touch new.txt  # Untracked

gitter commit -am "Update"
```

**Expected:** Only `tracked.txt` auto-staged, `new.txt` stays untracked

**Current Status:** ✅ **Correct** - -a only affects tracked files

**Need:** Explicit test confirming this

---

## 🟡 LOW PRIORITY: Advanced Behaviors

### 11. **Case Sensitivity Issues**

**Scenario:** Case-insensitive filesystems (Windows, macOS)

```bash
# On macOS (case-insensitive by default)
file.txt
File.txt  # Same file!

gitter add file.txt File.txt
```

**Current Status:** ❌ **Not handled**

**Git Behavior:** Treats as same file on case-insensitive FS

**Impact:** MEDIUM - Cross-platform compatibility

---

### 12. **Timestamp Precision and Time Zone**

**Scenario:** mtime comparison across timezones

```bash
# File modified at 2024-01-01 23:59:00 in one timezone
# Checked at 2024-01-02 00:01:00 in another timezone
```

**Current Status:** ✅ **Nanosecond precision** - Should be fine

---

### 13. **Whitespace-Only Changes**

**Scenario:** Tabs vs spaces

```bash
# File content changes but Git diff shows only whitespace
```

**Current Status:** ✅ **Content-based** - Hash includes whitespace

---

### 14. **Line Ending Differences**

**Scenario:** CRLF vs LF

**Current Status:** ❌ **Not normalized**

**Git Behavior:** Can configure line ending normalization

**Impact:** LOW - File content hash includes line endings

---

## 🔍 Gaps in Our Test Coverage

Based on analysis:

| Category | Missing Tests | Priority | Impact |
|----------|--------------|----------|--------|
| **Unicode** | All Unicode scenarios | HIGH | Data loss on international files |
| **Symlinks** | All symlink scenarios | HIGH | Security issues |
| **Binary Files** | Large files, null bytes | MEDIUM | Performance issues |
| **Corruption** | Corrupt index/objects | HIGH | Data loss |
| **Permissions** | Read-only, executable | LOW | Minor inconvenience |
| **Hard Links** | Hard link detection | LOW | Edge case |
| **Case Sensitivity** | Cross-platform | MEDIUM | Cross-platform bugs |

---

## 📝 Recommended Test Priority

### Phase 1: Critical Stability (Must Have)
1. ✅ Corruption tests (index, objects)
2. ✅ Unicode filename tests
3. ✅ Symlink tests
4. ✅ Large binary file tests

### Phase 2: Compatibility (Should Have)
5. ✅ Case-insensitive filesystem tests
6. ✅ Permission change detection tests
7. ✅ Complex staging state tests
8. ✅ Orphaned commit tests

### Phase 3: Polish (Nice to Have)
9. Race condition tests
10. Hard link tests
11. Line ending tests
12. Performance tests with 1000s of files

---

## 🎯 Most Likely Real-World Failures

Based on user behavior patterns:

1. **#1 Risk:** Unicode filenames - Users creating files with emoji/chinese/arabic characters
2. **#2 Risk:** Symlinks - Following symlinks outside repo (security issue)
3. **#3 Risk:** Large files - Git is slow but ours might crash/use too much memory
4. **#4 Risk:** Corrupt index - One I/O error corrupts entire index
5. **#5 Risk:** Case sensitivity - macOS/Windows users hitting this

---

## Summary

We have **excellent coverage** of happy-path workflows (193+ tests).

**Missing:** Edge cases that cause real-world failures:
- File system edge cases (symlinks, Unicode, large files)
- Corruption and recovery
- Cross-platform differences
- Race conditions and concurrent operations

**Recommendation:** Prioritize Unicode, symlinks, and corruption tests as these are most likely to cause user-reported bugs.
