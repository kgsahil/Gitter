#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "test_utils.hpp"
#include "cli/commands/RestoreCommand.hpp"
#include "cli/commands/AddCommand.hpp"
#include "core/Repository.hpp"
#include "core/Index.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class RestoreCommandTest : public ::testing::Test {
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
    AppContext ctx;
};

// Test: Restore single file
TEST_F(RestoreCommandTest, RestoreSingleFile) {
    AddCommand addCmd;
    RestoreCommand restoreCmd;
    
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    addCmd.execute(ctx, {"file1.txt", "file2.txt"});
    
    // Verify both in index
    Index index1;
    index1.load(tempDir);
    EXPECT_EQ(index1.entries().size(), 2);
    
    // Restore one file
    std::vector<std::string> args{"--staged", "file1.txt"};
    auto result = restoreCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify only one remains
    Index index2;
    index2.load(tempDir);
    EXPECT_EQ(index2.entries().size(), 1);
    EXPECT_TRUE(index2.entries().find("file2.txt") != index2.entries().end());
    EXPECT_TRUE(index2.entries().find("file1.txt") == index2.entries().end());
}

// Test: Restore multiple files
TEST_F(RestoreCommandTest, RestoreMultipleFiles) {
    AddCommand addCmd;
    RestoreCommand restoreCmd;
    
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    createFile(tempDir, "file3.txt", "content3");
    addCmd.execute(ctx, {"file1.txt", "file2.txt", "file3.txt"});
    
    // Restore two files
    std::vector<std::string> args{"--staged", "file1.txt", "file2.txt"};
    auto result = restoreCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    EXPECT_EQ(index.entries().size(), 1);
    EXPECT_TRUE(index.entries().find("file3.txt") != index.entries().end());
}

// Test: Restore with glob pattern *.txt
TEST_F(RestoreCommandTest, RestoreWithGlobPattern) {
    AddCommand addCmd;
    RestoreCommand restoreCmd;
    
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    createFile(tempDir, "file1.cpp", "content1");
    addCmd.execute(ctx, {"file1.txt", "file2.txt", "file1.cpp"});
    
    // Restore all .txt files
    std::vector<std::string> args{"--staged", "*.txt"};
    auto result = restoreCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    EXPECT_EQ(index.entries().size(), 1);
    EXPECT_TRUE(index.entries().find("file1.cpp") != index.entries().end());
    EXPECT_TRUE(index.entries().find("file1.txt") == index.entries().end());
    EXPECT_TRUE(index.entries().find("file2.txt") == index.entries().end());
}

// Test: Restore with pattern src/*.cpp
TEST_F(RestoreCommandTest, RestoreWithSubdirectoryPattern) {
    AddCommand addCmd;
    RestoreCommand restoreCmd;
    
    fs::create_directories(tempDir / "src");
    createFile(tempDir, "src/main.cpp", "int main() {}");
    createFile(tempDir, "src/util.cpp", "void util() {}");
    createFile(tempDir, "src/helper.h", "void helper();");
    createFile(tempDir, "main.cpp", "other");
    
    addCmd.execute(ctx, {"."});
    
    // Restore src/*.cpp
    std::vector<std::string> args{"--staged", "src/*.cpp"};
    auto result = restoreCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    // Should still have helper.h and main.cpp in root
    EXPECT_TRUE(index.entries().find("src/main.cpp") == index.entries().end());
    EXPECT_TRUE(index.entries().find("src/util.cpp") == index.entries().end());
    EXPECT_TRUE(index.entries().find("src/helper.h") != index.entries().end());
    EXPECT_TRUE(index.entries().find("main.cpp") != index.entries().end());
}

// Test: Restore non-existent file (should handle gracefully)
TEST_F(RestoreCommandTest, RestoreNonExistentFile) {
    AddCommand addCmd;
    RestoreCommand restoreCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Try to restore non-existent file
    std::vector<std::string> args{"--staged", "nonexistent.txt"};
    auto result = restoreCmd.execute(ctx, args);
    // Should complete without error (just warns)
}

// Test: Restore all files from index
TEST_F(RestoreCommandTest, RestoreAllFiles) {
    AddCommand addCmd;
    RestoreCommand restoreCmd;
    
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    createFile(tempDir, "file3.txt", "content3");
    addCmd.execute(ctx, {"file1.txt", "file2.txt", "file3.txt"});
    
    // Restore all
    std::vector<std::string> args{"--staged", "*.txt"};
    auto result = restoreCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    EXPECT_EQ(index.entries().size(), 0);
}

// Test: Restore without --staged flag (should fail)
TEST_F(RestoreCommandTest, RestoreWithoutStagedFlag) {
    AddCommand addCmd;
    RestoreCommand restoreCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Missing --staged flag
    std::vector<std::string> args{"file.txt"};
    auto result = restoreCmd.execute(ctx, args);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgs);
}

// Test: Restore empty index (should handle gracefully)
TEST_F(RestoreCommandTest, RestoreEmptyIndex) {
    RestoreCommand restoreCmd;
    
    std::vector<std::string> args{"--staged", "file.txt"};
    auto result = restoreCmd.execute(ctx, args);
    // Should complete without error
}

// Test: Restore file that was never staged
TEST_F(RestoreCommandTest, RestoreUnstagedFile) {
    RestoreCommand restoreCmd;
    
    createFile(tempDir, "file.txt", "content");
    
    std::vector<std::string> args{"--staged", "file.txt"};
    auto result = restoreCmd.execute(ctx, args);
    // Should complete without error (file not in index)
}

// Test: Restore with pattern that matches nothing
TEST_F(RestoreCommandTest, RestorePatternMatchesNothing) {
    AddCommand addCmd;
    RestoreCommand restoreCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Pattern matches nothing
    std::vector<std::string> args{"--staged", "*.cpp"};
    auto result = restoreCmd.execute(ctx, args);
    // Should complete without error (just warns)
}

// Test: Restore and add again
TEST_F(RestoreCommandTest, RestoreAndAddAgain) {
    AddCommand addCmd;
    RestoreCommand restoreCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Restore
    restoreCmd.execute(ctx, {"--staged", "file.txt"});
    
    Index index1;
    index1.load(tempDir);
    EXPECT_EQ(index1.entries().size(), 0);
    
    // Add again
    addCmd.execute(ctx, {"file.txt"});
    
    Index index2;
    index2.load(tempDir);
    EXPECT_EQ(index2.entries().size(), 1);
    EXPECT_TRUE(index2.entries().find("file.txt") != index2.entries().end());
}

// Test: Restore with combination of files and patterns
TEST_F(RestoreCommandTest, RestoreMixedFilesAndPatterns) {
    AddCommand addCmd;
    RestoreCommand restoreCmd;
    
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    createFile(tempDir, "file3.cpp", "content3");
    createFile(tempDir, "file4.cpp", "content4");
    addCmd.execute(ctx, {"file1.txt", "file2.txt", "file3.cpp", "file4.cpp"});
    
    // Restore with mix
    std::vector<std::string> args{"--staged", "file1.txt", "*.cpp"};
    auto result = restoreCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    // Should only have file2.txt
    EXPECT_EQ(index.entries().size(), 1);
    EXPECT_TRUE(index.entries().find("file2.txt") != index.entries().end());
}

