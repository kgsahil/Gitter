#include <gtest/gtest.h>
#include <filesystem>
#include "test_utils.hpp"
#include "core/ObjectStore.hpp"
#include "core/CommitObject.hpp"
#include "util/Sha1Hasher.hpp"
#include "util/Sha256Hasher.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class ObjectStoreTest : public ::testing::Test {
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

// Test: Write blob object
TEST_F(ObjectStoreTest, WriteBlob) {
    ObjectStore store(tempDir);
    
    std::string content = "hello world";
    std::string hash = store.writeBlob(content);
    
    // Verify hash is 40 chars (SHA-1)
    EXPECT_EQ(hash.length(), 40);
    
    // Verify object exists
    fs::path objPath = store.getObjectPath(hash);
    EXPECT_TRUE(fs::exists(objPath));
    EXPECT_EQ(objPath.parent_path().filename().string().length(), 2); // 2-char dir
}

// Test: Write blob from file
TEST_F(ObjectStoreTest, WriteBlobFromFile) {
    ObjectStore store(tempDir);
    
    fs::path filePath = createFile(tempDir, "test.txt", "file content");
    std::string hash = store.writeBlobFromFile(filePath);
    
    EXPECT_EQ(hash.length(), 40);
    fs::path objPath = store.getObjectPath(hash);
    EXPECT_TRUE(fs::exists(objPath));
}

// Test: Write tree object
TEST_F(ObjectStoreTest, WriteTree) {
    ObjectStore store(tempDir);
    
    // Create tree content (simplified: mode name\0hash)
    std::string treeContent = "100644 file.txt\0";
    // Add 20-byte binary hash (simplified for test)
    std::vector<uint8_t> hashBytes(20, 0xAB);
    treeContent.append(reinterpret_cast<const char*>(hashBytes.data()), 20);
    
    std::string hash = store.writeTree(treeContent);
    
    EXPECT_EQ(hash.length(), 40);
    fs::path objPath = store.getObjectPath(hash);
    EXPECT_TRUE(fs::exists(objPath));
}

// Test: Write commit object
TEST_F(ObjectStoreTest, WriteCommit) {
    ObjectStore store(tempDir);
    
    // Use valid 40-char hex tree hash
    std::string treeHash = "0000000000000000000000000000000000000001"; // 40-char valid hex
    std::string commitContent = "tree " + treeHash + "\n"
                               "author Gitter User <user@example.com> 1698765432 +0000\n"
                               "committer Gitter User <user@example.com> 1698765432 +0000\n"
                               "\n"
                               "Test commit message";
    
    std::string hash = store.writeCommit(commitContent);
    
    EXPECT_EQ(hash.length(), 40);
    fs::path objPath = store.getObjectPath(hash);
    EXPECT_TRUE(fs::exists(objPath));
}

// Test: Read commit object
TEST_F(ObjectStoreTest, ReadCommit) {
    ObjectStore store(tempDir);
    
    // Write commit (Git format requires newlines after each header line)
    // Use valid 40-char hex hash
    std::string fullTreeHash = "abc123def456ghi789jkl012mno345pqr678st12"; // 40 chars
    std::string commitContent = "tree " + fullTreeHash + "\n"
                               "author Test User <test@example.com> 1698765432 +0000\n"
                               "committer Test User <test@example.com> 1698765432 +0000\n"
                               "\n"
                               "Test message\n";
    
    std::string hash = store.writeCommit(commitContent);
    
    // Read commit
    CommitObject commit = store.readCommit(hash);
    
    EXPECT_EQ(commit.hash, hash);
    EXPECT_EQ(commit.treeHash, fullTreeHash);
    EXPECT_EQ(commit.authorName, "Test User");
    EXPECT_EQ(commit.authorEmail, "test@example.com");
    EXPECT_EQ(commit.message, "Test message\n");
}

// Test: Read non-existent object
TEST_F(ObjectStoreTest, ReadNonExistentObject) {
    ObjectStore store(tempDir);
    
    EXPECT_THROW(store.readCommit("nonexistent"), std::runtime_error);
}

// Test: Hash file content
TEST_F(ObjectStoreTest, HashFileContent) {
    ObjectStore store(tempDir);
    
    fs::path filePath = createFile(tempDir, "test.txt", "content");
    std::string hash = store.hashFileContent(filePath);
    
    EXPECT_EQ(hash.length(), 40);
    
    // Hash should match if we write blob
    std::string hash2 = store.writeBlob("content");
    // Note: hashFileContent hashes "blob <size>\0<content>"
    // writeBlob also hashes the same format
    // So hashes should match
    EXPECT_EQ(hash, hash2);
}

// Test: Same content produces same hash
TEST_F(ObjectStoreTest, SameContentSameHash) {
    ObjectStore store(tempDir);
    
    std::string content = "same content";
    std::string hash1 = store.writeBlob(content);
    std::string hash2 = store.writeBlob(content);
    
    EXPECT_EQ(hash1, hash2);
}

// Test: Different content produces different hash
TEST_F(ObjectStoreTest, DifferentContentDifferentHash) {
    ObjectStore store(tempDir);
    
    std::string hash1 = store.writeBlob("content1");
    std::string hash2 = store.writeBlob("content2");
    
    EXPECT_NE(hash1, hash2);
}

// Test: Object stored in 2-char directory
TEST_F(ObjectStoreTest, ObjectStoredInTwoCharDirectory) {
    ObjectStore store(tempDir);
    
    std::string hash = store.writeBlob("test");
    
    fs::path objPath = store.getObjectPath(hash);
    EXPECT_TRUE(fs::exists(objPath));
    
    // Verify directory structure
    fs::path parent = objPath.parent_path();
    EXPECT_EQ(parent.filename().string().length(), 2); // First 2 chars
    EXPECT_EQ(objPath.filename().string().length(), 38); // Remaining 38 chars (SHA-1)
}

// Test: Write object twice (should not duplicate)
TEST_F(ObjectStoreTest, WriteObjectTwice) {
    ObjectStore store(tempDir);
    
    std::string hash1 = store.writeBlob("content");
    std::string hash2 = store.writeBlob("content");
    
    EXPECT_EQ(hash1, hash2);
    
    // Should only have one object file
    fs::path objPath = store.getObjectPath(hash1);
    ASSERT_TRUE(fs::exists(objPath));
    
    // Get file size
    auto size1 = fs::file_size(objPath);
    
    // Write again (should not change)
    store.writeBlob("content");
    
    auto size2 = fs::file_size(objPath);
    EXPECT_EQ(size1, size2); // Size unchanged
}

// Test: Commit with parent
TEST_F(ObjectStoreTest, CommitWithParent) {
    ObjectStore store(tempDir);
    
    // First commit - use valid 40-char tree hash
    std::string treeHash1 = "0000000000000000000000000000000000000001"; // 40-char valid hex
    std::string commit1 = "tree " + treeHash1 + "\n"
                         "author User <user@example.com> 1698765432 +0000\n"
                         "committer User <user@example.com> 1698765432 +0000\n"
                         "\n"
                         "First";
    
    std::string hash1 = store.writeCommit(commit1);
    
    // Second commit with parent - use valid 40-char tree hash
    std::string treeHash2 = "0000000000000000000000000000000000000002"; // 40-char valid hex
    std::string commit2 = "tree " + treeHash2 + "\n"
                         "parent " + hash1 + "\n"
                         "author User <user@example.com> 1698765433 +0000\n"
                         "committer User <user@example.com> 1698765433 +0000\n"
                         "\n"
                         "Second";
    
    std::string hash2 = store.writeCommit(commit2);
    
    CommitObject commit = store.readCommit(hash2);
    EXPECT_EQ(commit.parentHashes.size(), 1);
    EXPECT_EQ(commit.parentHashes[0], hash1);
}

// Test: Empty blob
TEST_F(ObjectStoreTest, EmptyBlob) {
    ObjectStore store(tempDir);
    
    std::string hash = store.writeBlob("");
    
    EXPECT_EQ(hash.length(), 40);
    fs::path objPath = store.getObjectPath(hash);
    EXPECT_TRUE(fs::exists(objPath));
}

