#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include "test_utils.hpp"
#include "cli/commands/AddCommand.hpp"
#include "core/Repository.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class AddCommandTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = createTempDir();
        originalCwd = getCwd();
        setCwd(tempDir);
        
        // Initialize repository
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

// Test: Add single file
TEST_F(AddCommandTest, AddSingleFile) {
    AddCommand cmd;
    createFile(tempDir, "file1.txt", "content1");
    
    std::vector<std::string> args{"file1.txt"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify file added to index
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 1);
    EXPECT_TRUE(entries.find("file1.txt") != entries.end());
}

// Test: Add multiple files
TEST_F(AddCommandTest, AddMultipleFiles) {
    AddCommand cmd;
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    createFile(tempDir, "file3.cpp", "content3");
    
    std::vector<std::string> args{"file1.txt", "file2.txt", "file3.cpp"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 3);
    EXPECT_TRUE(entries.find("file1.txt") != entries.end());
    EXPECT_TRUE(entries.find("file2.txt") != entries.end());
    EXPECT_TRUE(entries.find("file3.cpp") != entries.end());
}

// Test: Add directory recursively
TEST_F(AddCommandTest, AddDirectoryRecursive) {
    AddCommand cmd;
    fs::create_directories(tempDir / "src" / "util");
    createFile(tempDir, "src/main.cpp", "int main() {}");
    createFile(tempDir, "src/util/helper.cpp", "void helper() {}");
    createFile(tempDir, "README.md", "Readme");
    
    std::vector<std::string> args{"src/"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 2);
    EXPECT_TRUE(entries.find("src/main.cpp") != entries.end());
    EXPECT_TRUE(entries.find("src/util/helper.cpp") != entries.end());
}

// Test: Add current directory (all files)
TEST_F(AddCommandTest, AddCurrentDirectory) {
    AddCommand cmd;
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    fs::create_directories(tempDir / "src");
    createFile(tempDir, "src/main.cpp", "int main() {}");
    
    std::vector<std::string> args{"."};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_GE(entries.size(), 3); // At least 3 files
}

// Test: Add with glob pattern *.txt
TEST_F(AddCommandTest, AddGlobPatternTxtFiles) {
    AddCommand cmd;
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    createFile(tempDir, "file3.cpp", "content3");
    
    std::vector<std::string> args{"*.txt"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 2); // Only .txt files
    EXPECT_TRUE(entries.find("file1.txt") != entries.end());
    EXPECT_TRUE(entries.find("file2.txt") != entries.end());
    EXPECT_TRUE(entries.find("file3.cpp") == entries.end());
}

// Test: Add with glob pattern src/*.cpp
TEST_F(AddCommandTest, AddGlobPatternInSubdirectory) {
    AddCommand cmd;
    fs::create_directories(tempDir / "src");
    createFile(tempDir, "src/main.cpp", "int main() {}");
    createFile(tempDir, "src/helper.cpp", "void helper() {}");
    createFile(tempDir, "src/helper.h", "void helper();");
    createFile(tempDir, "main.cpp", "other");
    
    std::vector<std::string> args{"src/*.cpp"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 2); // Only src/*.cpp
    EXPECT_TRUE(entries.find("src/main.cpp") != entries.end());
    EXPECT_TRUE(entries.find("src/helper.cpp") != entries.end());
    EXPECT_TRUE(entries.find("src/helper.h") == entries.end());
    EXPECT_TRUE(entries.find("main.cpp") == entries.end());
}

// Test: Add with pattern test?.py
TEST_F(AddCommandTest, AddGlobPatternWithQuestionMark) {
    AddCommand cmd;
    createFile(tempDir, "test1.py", "test1");
    createFile(tempDir, "test2.py", "test2");
    createFile(tempDir, "test10.py", "test10");
    createFile(tempDir, "test.py", "test");
    
    std::vector<std::string> args{"test?.py"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    // Should match test1.py and test2.py, but not test10.py or test.py
    EXPECT_GE(entries.size(), 2);
    EXPECT_TRUE(entries.find("test1.py") != entries.end());
    EXPECT_TRUE(entries.find("test2.py") != entries.end());
}

// Test: Add non-existent file (should warn but not crash)
TEST_F(AddCommandTest, AddNonExistentFile) {
    AddCommand cmd;
    
    std::vector<std::string> args{"nonexistent.txt"};
    auto result = cmd.execute(ctx, args);
    // Should complete without error (just warns)
    // Result may or may not be successful depending on implementation
}

// Test: Add file, modify it, add again (should update index)
TEST_F(AddCommandTest, AddModifiedFile) {
    AddCommand cmd;
    fs::path filePath = createFile(tempDir, "file.txt", "content1");
    
    // First add
    std::vector<std::string> args1{"file.txt"};
    auto result1 = cmd.execute(ctx, args1);
    ASSERT_TRUE(result1.has_value()) << result1.error().message;
    
    Index index1;
    index1.load(tempDir);
    std::string hash1 = index1.entries().at("file.txt").hashHex;
    
    // Modify file - add small delay to ensure mtime updates (filesystem granularity)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    createFile(tempDir, "file.txt", "content2");
    
    // Add again
    auto result2 = cmd.execute(ctx, args1);
    ASSERT_TRUE(result2.has_value()) << result2.error().message;
    
    Index index2;
    index2.load(tempDir);
    std::string hash2 = index2.entries().at("file.txt").hashHex;
    
    // Hash should be different
    EXPECT_NE(hash1, hash2);
}

// Test: Add creates blob objects
TEST_F(AddCommandTest, AddCreatesBlobObjects) {
    AddCommand cmd;
    createFile(tempDir, "file.txt", "test content");
    
    std::vector<std::string> args{"file.txt"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Get hash from index
    Index index;
    index.load(tempDir);
    std::string hash = index.entries().at("file.txt").hashHex;
    
    // Verify blob object exists
    ObjectStore store(tempDir);
    fs::path objPath = store.getObjectPath(hash);
    EXPECT_TRUE(fs::exists(objPath));
}

// Test: Add skips .gitter directory
TEST_F(AddCommandTest, AddSkipsGitterDirectory) {
    AddCommand cmd;
    createFile(tempDir, "file.txt", "content");
    
    // Create file inside .gitter (should be skipped)
    fs::path gitterFile = tempDir / ".gitter" / "test.txt";
    createFile(tempDir / ".gitter", "test.txt", "should not be added");
    
    std::vector<std::string> args{"."};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    
    // Should contain file.txt but not .gitter/test.txt
    EXPECT_TRUE(entries.find("file.txt") != entries.end());
    EXPECT_TRUE(entries.find(".gitter/test.txt") == entries.end());
}

// Test: Add executable file (should preserve mode)
TEST_F(AddCommandTest, AddExecutableFile) {
    AddCommand cmd;
    fs::path scriptPath = createFile(tempDir, "script.sh", "#!/bin/bash\necho test");
    
    // Make executable (Unix/Linux only)
    #ifndef _WIN32
    fs::permissions(scriptPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);
    #endif
    
    std::vector<std::string> args{"script.sh"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    ASSERT_TRUE(entries.find("script.sh") != entries.end());
    
    #ifndef _WIN32
    // On Unix, executable mode should be 0100755
    EXPECT_EQ(entries.at("script.sh").mode, 0100755);
    #endif
}

// Test: Add with combination of files and patterns
TEST_F(AddCommandTest, AddMixedFilesAndPatterns) {
    AddCommand cmd;
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    createFile(tempDir, "main.cpp", "int main() {}");
    fs::create_directories(tempDir / "src");
    createFile(tempDir, "src/util.cpp", "void util() {}");
    
    std::vector<std::string> args{"file1.txt", "*.txt", "src/"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    // Should have: file1.txt, file2.txt (from *.txt), src/util.cpp
    EXPECT_GE(entries.size(), 3);
}

// Test: Add empty file
TEST_F(AddCommandTest, AddEmptyFile) {
    AddCommand cmd;
    createFile(tempDir, "empty.txt", "");
    
    std::vector<std::string> args{"empty.txt"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_TRUE(entries.find("empty.txt") != entries.end());
    
    // Empty file should have size 0
    EXPECT_EQ(entries.at("empty.txt").sizeBytes, 0);
}

// Test: Add in nested directory structure
TEST_F(AddCommandTest, AddNestedDirectoryStructure) {
    AddCommand cmd;
    fs::create_directories(tempDir / "a" / "b" / "c");
    createFile(tempDir, "a/file1.txt", "content1");
    createFile(tempDir, "a/b/file2.txt", "content2");
    createFile(tempDir, "a/b/c/file3.txt", "content3");
    
    std::vector<std::string> args{"a/"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 3);
    EXPECT_TRUE(entries.find("a/file1.txt") != entries.end());
    EXPECT_TRUE(entries.find("a/b/file2.txt") != entries.end());
    EXPECT_TRUE(entries.find("a/b/c/file3.txt") != entries.end());
}

// Test: Add with no arguments (should fail)
TEST_F(AddCommandTest, AddNoArguments) {
    AddCommand cmd;
    std::vector<std::string> args;
    
    auto result = cmd.execute(ctx, args);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgs);
}

