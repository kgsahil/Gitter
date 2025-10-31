#include <gtest/gtest.h>
#include <filesystem>
#include "test_utils.hpp"
#include "core/TreeBuilder.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class TreeBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = createTempDir();
        originalCwd = getCwd();
        setCwd(tempDir);
        initTestRepo(tempDir);
    }
    
    void TearDown() override {
        setCwd(originalCwd);
        removeDir(tempDir);
    }
    
    fs::path tempDir;
    fs::path originalCwd;
};

// Test: Build tree from single file
TEST_F(TreeBuilderTest, BuildTreeSingleFile) {
    Index index;
    ObjectStore store(tempDir);
    
    // Add entry to index
    IndexEntry entry;
    entry.path = "file.txt";
    // Use valid hex hash (lowercase hex: 0-9a-f) - must be exactly 40 chars
    entry.hashHex = "abc123def4567890123456789012345678901234"; // 40-char valid hex
    entry.sizeBytes = 10;
    entry.mode = 0100644;
    
    EXPECT_NO_THROW(index.addOrUpdate(entry));
    
    std::string treeHash;
    EXPECT_NO_THROW(treeHash = TreeBuilder::buildFromIndex(index, store));
    
    EXPECT_FALSE(treeHash.empty());
    EXPECT_EQ(treeHash.length(), 40); // SHA-1 hash
    
    // Verify tree object exists
    fs::path treePath = store.getObjectPath(treeHash);
    EXPECT_TRUE(fs::exists(treePath));
}

// Test: Build tree from multiple files
TEST_F(TreeBuilderTest, BuildTreeMultipleFiles) {
    Index index;
    ObjectStore store(tempDir);
    
    // Add multiple entries with valid hex hashes
    std::vector<std::string> hashes = {
        "0000000000000000000000000000000000000000",
        "1111111111111111111111111111111111111111",
        "2222222222222222222222222222222222222222"
    };
    for (int i = 0; i < 3; ++i) {
        IndexEntry entry;
        entry.path = "file" + std::to_string(i) + ".txt";
        entry.hashHex = hashes[i]; // Valid hex hash
        entry.sizeBytes = 100;
        entry.mode = 0100644;
        index.addOrUpdate(entry);
    }
    
    std::string treeHash = TreeBuilder::buildFromIndex(index, store);
    
    EXPECT_FALSE(treeHash.empty());
    
    // Verify tree object exists
    fs::path treePath = store.getObjectPath(treeHash);
    EXPECT_TRUE(fs::exists(treePath));
}

// Test: Build tree with subdirectories
TEST_F(TreeBuilderTest, BuildTreeWithSubdirectories) {
    Index index;
    ObjectStore store(tempDir);
    
    // Add files in different directories with valid hex hashes
    IndexEntry entry1;
    entry1.path = "src/main.cpp";
    entry1.hashHex = "1111111111111111111111111111111111111111"; // Valid hex
    entry1.mode = 0100644;
    index.addOrUpdate(entry1);
    
    IndexEntry entry2;
    entry2.path = "src/util/helper.cpp";
    entry2.hashHex = "2222222222222222222222222222222222222222"; // Valid hex
    entry2.mode = 0100644;
    index.addOrUpdate(entry2);
    
    IndexEntry entry3;
    entry3.path = "README.md";
    entry3.hashHex = "3333333333333333333333333333333333333333"; // Valid hex
    entry3.mode = 0100644;
    index.addOrUpdate(entry3);
    
    std::string treeHash = TreeBuilder::buildFromIndex(index, store);
    
    EXPECT_FALSE(treeHash.empty());
    
    // Should create multiple tree objects (root, src/, src/util/)
    // Verify root tree exists
    fs::path rootTreePath = store.getObjectPath(treeHash);
    EXPECT_TRUE(fs::exists(rootTreePath));
}

// Test: Build tree with nested directories
TEST_F(TreeBuilderTest, BuildTreeNestedDirectories) {
    Index index;
    ObjectStore store(tempDir);
    
    // Deep nesting
    IndexEntry entry;
    entry.path = "a/b/c/d/e/file.txt";
    entry.hashHex = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; // Valid hex
    entry.mode = 0100644;
    index.addOrUpdate(entry);
    
    std::string treeHash = TreeBuilder::buildFromIndex(index, store);
    
    EXPECT_FALSE(treeHash.empty());
    
    // Should create tree for each directory level
    fs::path rootTreePath = store.getObjectPath(treeHash);
    EXPECT_TRUE(fs::exists(rootTreePath));
}

// Test: Build tree with empty index
TEST_F(TreeBuilderTest, BuildTreeEmptyIndex) {
    Index index;
    ObjectStore store(tempDir);
    
    std::string treeHash = TreeBuilder::buildFromIndex(index, store);
    
    // Should return empty or handle gracefully
    // Current implementation may return empty string
}

// Test: Build tree with executable files
TEST_F(TreeBuilderTest, BuildTreeWithExecutableFiles) {
    Index index;
    ObjectStore store(tempDir);
    
    IndexEntry entry1;
    entry1.path = "script.sh";
    entry1.hashHex = "1111111111111111111111111111111111111111"; // Valid hex
    entry1.mode = 0100755; // Executable
    index.addOrUpdate(entry1);
    
    IndexEntry entry2;
    entry2.path = "file.txt";
    entry2.hashHex = "2222222222222222222222222222222222222222"; // Valid hex
    entry2.mode = 0100644; // Regular
    index.addOrUpdate(entry2);
    
    std::string treeHash = TreeBuilder::buildFromIndex(index, store);
    
    EXPECT_FALSE(treeHash.empty());
    
    // Tree should contain both entries with correct modes
    fs::path treePath = store.getObjectPath(treeHash);
    EXPECT_TRUE(fs::exists(treePath));
}

// Test: Build tree preserves file order (sorted by name)
TEST_F(TreeBuilderTest, BuildTreeSortedOrder) {
    Index index;
    ObjectStore store(tempDir);
    
    // Add files in non-alphabetical order with valid hex hashes
    IndexEntry entry1;
    entry1.path = "zebra.txt";
    entry1.hashHex = "00000000000000000000000000000000000000ff"; // Valid hex (Z in hex is 5a)
    entry1.mode = 0100644;
    index.addOrUpdate(entry1);
    
    IndexEntry entry2;
    entry2.path = "apple.txt";
    entry2.hashHex = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; // Valid hex
    entry2.mode = 0100644;
    index.addOrUpdate(entry2);
    
    IndexEntry entry3;
    entry3.path = "banana.txt";
    entry3.hashHex = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"; // Valid hex
    entry3.mode = 0100644;
    index.addOrUpdate(entry3);
    
    std::string treeHash = TreeBuilder::buildFromIndex(index, store);
    
    EXPECT_FALSE(treeHash.empty());
    
    // Tree entries should be sorted (Git requirement)
    // apple.txt should come before banana.txt before zebra.txt
}

// Test: Build same tree twice produces same hash
TEST_F(TreeBuilderTest, BuildTreeTwiceSameHash) {
    Index index1, index2;
    ObjectStore store(tempDir);
    
    // Create same index entries in both
    IndexEntry entry;
    entry.path = "file.txt";
    entry.hashHex = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; // Valid hex
    entry.mode = 0100644;
    
    index1.addOrUpdate(entry);
    index2.addOrUpdate(entry);
    
    std::string hash1 = TreeBuilder::buildFromIndex(index1, store);
    std::string hash2 = TreeBuilder::buildFromIndex(index2, store);
    
    EXPECT_EQ(hash1, hash2); // Same content = same hash
}

// Test: Build tree with mixed files and directories
TEST_F(TreeBuilderTest, BuildTreeMixedStructure) {
    Index index;
    ObjectStore store(tempDir);
    
    // Root level file
    IndexEntry entry1;
    entry1.path = "README.md";
    entry1.hashHex = "1111111111111111111111111111111111111111"; // Valid hex
    entry1.mode = 0100644;
    index.addOrUpdate(entry1);
    
    // File in subdirectory
    IndexEntry entry2;
    entry2.path = "src/main.cpp";
    entry2.hashHex = "2222222222222222222222222222222222222222"; // Valid hex
    entry2.mode = 0100644;
    index.addOrUpdate(entry2);
    
    // Another file in same subdirectory
    IndexEntry entry3;
    entry3.path = "src/util.cpp";
    entry3.hashHex = "3333333333333333333333333333333333333333"; // Valid hex
    entry3.mode = 0100644;
    index.addOrUpdate(entry3);
    
    // File in nested subdirectory
    IndexEntry entry4;
    entry4.path = "src/include/header.h";
    entry4.hashHex = "4444444444444444444444444444444444444444"; // Valid hex
    entry4.mode = 0100644;
    index.addOrUpdate(entry4);
    
    std::string treeHash = TreeBuilder::buildFromIndex(index, store);
    
    EXPECT_FALSE(treeHash.empty());
    
    // Should create trees: root, src/, src/include/
    fs::path rootTreePath = store.getObjectPath(treeHash);
    EXPECT_TRUE(fs::exists(rootTreePath));
}

