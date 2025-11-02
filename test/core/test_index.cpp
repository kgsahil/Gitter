#include <gtest/gtest.h>
#include <filesystem>
#include "test_utils.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

// Helper function for base64 encoding (simplified version matching Index.cpp)
static std::string base64Encode(const std::string& input) {
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int val = 0, valb = -6;
    
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        encoded.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (encoded.size() % 4) {
        encoded.push_back('=');
    }
    
    return encoded;
}

class IndexTest : public ::testing::Test {
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

// Test: Load empty index
TEST_F(IndexTest, LoadEmptyIndex) {
    Index index;
    bool result = index.load(tempDir);
    EXPECT_TRUE(result);
    EXPECT_TRUE(index.entries().empty());
}

// Test: Add entry to index
TEST_F(IndexTest, AddEntry) {
    Index index;
    
    IndexEntry entry;
    entry.path = "file.txt";
    entry.hashHex = "abc123def4567890123456789012345678901234"; // 40-char valid hex
    entry.sizeBytes = 100;
    entry.mtimeNs = 1234567890;
    entry.mode = 0100644;
    entry.ctimeNs = 1234567890;
    
    EXPECT_NO_THROW(index.addOrUpdate(entry));
    EXPECT_EQ(index.entries().size(), 1);
    // Path is normalized, so look up with normalized path (should be same for "file.txt")
    EXPECT_TRUE(index.entries().find("file.txt") != index.entries().end());
}

// Test: Save and load index
TEST_F(IndexTest, SaveAndLoad) {
    Index index1;
    
    IndexEntry entry;
    entry.path = "file.txt";
    entry.hashHex = "abc123def4567890123456789012345678901234"; // 40-char valid hex
    entry.sizeBytes = 100;
    entry.mtimeNs = 1234567890000000000ULL;
    entry.mode = 0100644;
    entry.ctimeNs = 1234567890000000000ULL;
    
    index1.addOrUpdate(entry);
    index1.save(tempDir);
    
    // Load in new index
    Index index2;
    index2.load(tempDir);
    
    EXPECT_EQ(index2.entries().size(), 1);
    ASSERT_TRUE(index2.entries().find("file.txt") != index2.entries().end());
    
    const IndexEntry& loaded = index2.entries().at("file.txt");
    EXPECT_EQ(loaded.path, "file.txt");
    EXPECT_EQ(loaded.hashHex, "abc123def4567890123456789012345678901234"); // 40-char valid hex
    EXPECT_EQ(loaded.sizeBytes, 100);
    EXPECT_EQ(loaded.mode, 0100644);
}

// Test: Update existing entry
TEST_F(IndexTest, UpdateExistingEntry) {
    Index index;
    
    IndexEntry entry1;
    entry1.path = "file.txt";
    entry1.hashHex = "0000000000000000000000000000000000000001"; // 40-char valid hex
    entry1.sizeBytes = 100;
    index.addOrUpdate(entry1);
    
    IndexEntry entry2;
    entry2.path = "file.txt";
    entry2.hashHex = "0000000000000000000000000000000000000002"; // 40-char valid hex
    entry2.sizeBytes = 200;
    index.addOrUpdate(entry2);
    
    EXPECT_EQ(index.entries().size(), 1);
    EXPECT_EQ(index.entries().at("file.txt").hashHex, "0000000000000000000000000000000000000002");
    EXPECT_EQ(index.entries().at("file.txt").sizeBytes, 200);
}

// Test: Remove entry
TEST_F(IndexTest, RemoveEntry) {
    Index index;
    
    IndexEntry entry;
    entry.path = "file.txt";
    entry.hashHex = "0000000000000000000000000000000000000000"; // 40-char valid hex
    index.addOrUpdate(entry);
    
    EXPECT_EQ(index.entries().size(), 1);
    
    index.remove("file.txt");
    EXPECT_EQ(index.entries().size(), 0);
}

// Test: Multiple entries
TEST_F(IndexTest, MultipleEntries) {
    Index index;
    
    for (int i = 0; i < 5; ++i) {
        IndexEntry entry;
        entry.path = "file" + std::to_string(i) + ".txt";
        // Generate 40-char hex hash: pad with zeros
        std::string hash = std::string(40 - std::to_string(i).length(), '0') + std::to_string(i);
        entry.hashHex = hash; // Valid hex hash
        entry.sizeBytes = i * 100;
        index.addOrUpdate(entry);
    }
    
    EXPECT_EQ(index.entries().size(), 5);
    
    // Save and reload
    index.save(tempDir);
    Index index2;
    index2.load(tempDir);
    
    EXPECT_EQ(index2.entries().size(), 5);
    for (int i = 0; i < 5; ++i) {
        std::string path = "file" + std::to_string(i) + ".txt";
        EXPECT_TRUE(index2.entries().find(path) != index2.entries().end());
    }
}

// Test: Remove non-existent entry
TEST_F(IndexTest, RemoveNonExistent) {
    Index index;
    
    // Remove from empty index
    index.remove("nonexistent.txt");
    EXPECT_EQ(index.entries().size(), 0);
    
    // Add entry
    IndexEntry entry;
    entry.path = "file.txt";
    entry.hashHex = "0000000000000000000000000000000000000000"; // 40-char valid hex
    index.addOrUpdate(entry);
    
    // Remove non-existent
    index.remove("other.txt");
    EXPECT_EQ(index.entries().size(), 1);
}

// Test: Index with file permissions
TEST_F(IndexTest, IndexWithFilePermissions) {
    Index index;
    
    IndexEntry entry1;
    entry1.path = "regular.txt";
    entry1.hashHex = "0000000000000000000000000000000000000001"; // 40-char valid hex
    entry1.mode = 0100644;
    index.addOrUpdate(entry1);
    
    IndexEntry entry2;
    entry2.path = "executable.sh";
    entry2.hashHex = "0000000000000000000000000000000000000002"; // 40-char valid hex
    entry2.mode = 0100755;
    index.addOrUpdate(entry2);
    
    index.save(tempDir);
    
    Index index2;
    index2.load(tempDir);
    
    EXPECT_EQ(index2.entries().at("regular.txt").mode, 0100644);
    EXPECT_EQ(index2.entries().at("executable.sh").mode, 0100755);
}

// Test: Index with timestamps
TEST_F(IndexTest, IndexWithTimestamps) {
    Index index;
    
    IndexEntry entry;
    entry.path = "file.txt";
    entry.hashHex = "0000000000000000000000000000000000000000"; // 40-char valid hex
    entry.mtimeNs = 1234567890123456789ULL;
    entry.ctimeNs = 9876543210987654321ULL;
    index.addOrUpdate(entry);
    
    index.save(tempDir);
    
    Index index2;
    index2.load(tempDir);
    
    EXPECT_EQ(index2.entries().at("file.txt").mtimeNs, 1234567890123456789ULL);
    EXPECT_EQ(index2.entries().at("file.txt").ctimeNs, 9876543210987654321ULL);
}

// Test: Load corrupt index with empty file
TEST_F(IndexTest, LoadCorruptIndexEmptyFile) {
    Index index;
    
    // Create empty index file
    fs::path indexPath = tempDir / ".gitter" / "index";
    std::ofstream out(indexPath);
    out.close();
    
    // Should load successfully with no entries
    bool result = index.load(tempDir);
    EXPECT_TRUE(result);
    EXPECT_TRUE(index.entries().empty());
}

// Test: Load corrupt index with truncated line
TEST_F(IndexTest, LoadCorruptIndexTruncatedLine) {
    Index index;
    
    // Write index with incomplete entry (missing fields)
    fs::path indexPath = tempDir / ".gitter" / "index";
    std::ofstream out(indexPath, std::ios::binary);
    out << "file.txt\tabc123";  // Missing fields
    out.close();
    
    // Should skip invalid entry
    bool result = index.load(tempDir);
    EXPECT_TRUE(result);
    EXPECT_TRUE(index.entries().empty());
}

// Test: Load corrupt index with invalid hash format
TEST_F(IndexTest, LoadCorruptIndexInvalidHash) {
    Index index;
    
    // Write index with invalid hash
    fs::path indexPath = tempDir / ".gitter" / "index";
    std::ofstream out(indexPath, std::ios::binary);
    out << "file.txt\tinvalid_hash_123\t1024\t1234567890\t33188\t1234567890\n";
    out.close();
    
    // Should skip entry with invalid hash format
    bool result = index.load(tempDir);
    EXPECT_TRUE(result);
    EXPECT_TRUE(index.entries().empty());
}

// Test: Load corrupt index with invalid numeric field
TEST_F(IndexTest, LoadCorruptIndexInvalidNumeric) {
    Index index;
    
    // Write index with non-numeric size
    fs::path indexPath = tempDir / ".gitter" / "index";
    std::ofstream out(indexPath, std::ios::binary);
    out << "file.txt\tabc123def4567890123456789012345678901234\tnot_a_number\t1234567890\t33188\t1234567890\n";
    out.close();
    
    // Should skip entry with invalid numeric fields
    bool result = index.load(tempDir);
    EXPECT_TRUE(result);
    EXPECT_TRUE(index.entries().empty());
}

// Test: Load corrupt index with valid entries mixed with corrupt entries
TEST_F(IndexTest, LoadCorruptIndexMixedValidAndCorrupt) {
    Index index;
    
    // Write index with both valid and corrupt entries
    fs::path indexPath = tempDir / ".gitter" / "index";
    std::ofstream out(indexPath, std::ios::binary);
    
    // Valid entry
    std::string validPath = base64Encode("good.txt");
    out << validPath << "\tabc123def4567890123456789012345678901234\t1024\t1234567890\t33188\t1234567890\n";
    
    // Corrupt entry (invalid hash)
    std::string corruptPath = base64Encode("bad.txt");
    out << corruptPath << "\tbad_hash\t1024\t1234567890\t33188\t1234567890\n";
    
    // Another valid entry
    std::string validPath2 = base64Encode("good2.txt");
    out << validPath2 << "\tdef456abc1237890123456789012345678901234\t2048\t1234567890\t33188\t1234567890\n";
    
    out.close();
    
    // Should load only valid entries
    bool result = index.load(tempDir);
    EXPECT_TRUE(result);
    EXPECT_EQ(index.entries().size(), 2);
    EXPECT_TRUE(index.entries().find("good.txt") != index.entries().end());
    EXPECT_TRUE(index.entries().find("good2.txt") != index.entries().end());
    EXPECT_TRUE(index.entries().find("bad.txt") == index.entries().end());
}

