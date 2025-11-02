#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <sstream>
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

// Test: Add non-existent file (should show warning)
TEST_F(AddCommandTest, AddNonExistentFile) {
    AddCommand cmd;
    std::vector<std::string> args{"nonexistent.txt"};
    
    // Capture stderr
    std::stringstream stderrCapture;
    std::streambuf* oldCerr = std::cerr.rdbuf();
    std::cerr.rdbuf(stderrCapture.rdbuf());
    
    auto result = cmd.execute(ctx, args);
    
    // Restore stderr
    std::cerr.rdbuf(oldCerr);
    
    // Should succeed but show warning
    EXPECT_TRUE(result.has_value());
    
    std::string output = stderrCapture.str();
    EXPECT_NE(output.find("warning: path does not exist"), std::string::npos);
}

// Test: Add pattern with no matches (should show warning)
TEST_F(AddCommandTest, AddPatternNoMatches) {
    AddCommand cmd;
    std::vector<std::string> args{"*.nonexistent"};
    
    // Capture stderr
    std::stringstream stderrCapture;
    std::streambuf* oldCerr = std::cerr.rdbuf();
    std::cerr.rdbuf(stderrCapture.rdbuf());
    
    auto result = cmd.execute(ctx, args);
    
    // Restore stderr
    std::cerr.rdbuf(oldCerr);
    
    // Should succeed but show warning
    EXPECT_TRUE(result.has_value());
    
    std::string output = stderrCapture.str();
    EXPECT_NE(output.find("warning: no files match pattern"), std::string::npos);
}

// Test: Try to add .gitter directory (should skip silently)
TEST_F(AddCommandTest, AddGitterDirectory) {
    AddCommand cmd;
    std::vector<std::string> args{".gitter"};
    
    // Should succeed and skip .gitter silently
    auto result = cmd.execute(ctx, args);
    EXPECT_TRUE(result.has_value());
    
    // Index should be empty (no files added)
    Index index;
    index.load(tempDir);
    EXPECT_TRUE(index.entries().empty());
}

// Test: Add file with TAB characters in name
TEST_F(AddCommandTest, AddFileWithTabsInName) {
    AddCommand cmd;
    
    // Create file with TAB characters (like Git supports)
    std::string tabFile = std::string("file") + '\t' + "with" + '\t' + "tabs.txt";
    fs::path filePath = tempDir / tabFile;
    
    // Write file content
    std::ofstream out(filePath);
    out << "content with tabs in name";
    out.close();
    
    std::vector<std::string> args{tabFile};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify file added to index
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 1);
    EXPECT_TRUE(entries.find(tabFile) != entries.end());
}

// Test: Add file with newline characters in name
TEST_F(AddCommandTest, AddFileWithNewlinesInName) {
    AddCommand cmd;
    
    // Create file with newline characters (like Git supports)
    std::string nlFile = std::string("file") + '\n' + "with" + '\n' + "newlines.txt";
    fs::path filePath = tempDir / nlFile;
    
    // Write file content
    std::ofstream out(filePath);
    out << "content with newlines in name";
    out.close();
    
    std::vector<std::string> args{nlFile};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify file added to index
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 1);
    EXPECT_TRUE(entries.find(nlFile) != entries.end());
}

// Test: Index round-trip with special characters
TEST_F(AddCommandTest, IndexRoundTripSpecialCharacters) {
    AddCommand cmd;
    
    // Create file with TAB characters
    std::string specialFile = std::string("special") + '\t' + "file" + '\t' + "name.txt";
    fs::path filePath = tempDir / specialFile;
    createFile(tempDir, specialFile, "special content");
    
    // Add file
    auto result = cmd.execute(ctx, {specialFile});
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Get hash from index
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    ASSERT_TRUE(entries.find(specialFile) != entries.end());
    std::string hash1 = entries.at(specialFile).hashHex;
    
    // Save index
    ASSERT_TRUE(index.save(tempDir));
    
    // Load index again
    Index index2;
    ASSERT_TRUE(index2.load(tempDir));
    
    // Verify entry preserved correctly
    const auto& entries2 = index2.entries();
    EXPECT_EQ(entries2.size(), 1);
    ASSERT_TRUE(entries2.find(specialFile) != entries2.end());
    std::string hash2 = entries2.at(specialFile).hashHex;
    EXPECT_EQ(hash1, hash2);
}

// Test: Add empty directory (should skip - Git doesn't track empty dirs)
TEST_F(AddCommandTest, AddEmptyDirectory) {
    AddCommand cmd;
    
    // Create empty directory
    fs::create_directories(tempDir / "empty_dir");
    
    std::vector<std::string> args{"empty_dir"};
    auto result = cmd.execute(ctx, args);
    
    // Should succeed but not add anything
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    
    // Should be empty (no files to track)
    EXPECT_EQ(entries.size(), 0);
}

// Test: Add directory with only empty subdirectories
TEST_F(AddCommandTest, AddDirectoryWithOnlyEmptySubdirs) {
    AddCommand cmd;
    
    // Create nested empty directories
    fs::create_directories(tempDir / "dir1" / "dir2" / "dir3");
    
    std::vector<std::string> args{"dir1"};
    auto result = cmd.execute(ctx, args);
    
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    
    // No files, so index should be empty
    EXPECT_EQ(entries.size(), 0);
}

// Test: Add file with Unicode characters in filename (Chinese)
TEST_F(AddCommandTest, AddFileWithUnicodeChinese) {
    AddCommand cmd;
    
    // Create file with Chinese characters
    std::string unicodeFile = "文件.txt";
    createFile(tempDir, unicodeFile, "Chinese content");
    
    std::vector<std::string> args{unicodeFile};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 1);
    EXPECT_TRUE(entries.find(unicodeFile) != entries.end());
}

// Test: Add file with Unicode characters in filename (Arabic)
TEST_F(AddCommandTest, AddFileWithUnicodeArabic) {
    AddCommand cmd;
    
    std::string unicodeFile = "مرحبا.py";
    createFile(tempDir, unicodeFile, "Arabic content");
    
    std::vector<std::string> args{unicodeFile};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 1);
    EXPECT_TRUE(entries.find(unicodeFile) != entries.end());
}

// Test: Add file with Unicode characters in filename (Russian)
TEST_F(AddCommandTest, AddFileWithUnicodeRussian) {
    AddCommand cmd;
    
    std::string unicodeFile = "тест.cpp";
    createFile(tempDir, unicodeFile, "int main() {}");
    
    std::vector<std::string> args{unicodeFile};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 1);
    EXPECT_TRUE(entries.find(unicodeFile) != entries.end());
}

// Test: Add file with emoji in filename
TEST_F(AddCommandTest, AddFileWithEmoji) {
    AddCommand cmd;
    
    std::string unicodeFile = "文件📄.txt";
    createFile(tempDir, unicodeFile, "emoji content");
    
    std::vector<std::string> args{unicodeFile};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 1);
    EXPECT_TRUE(entries.find(unicodeFile) != entries.end());
}

// Test: Add multiple files with mixed Unicode characters
TEST_F(AddCommandTest, AddMultipleUnicodeFiles) {
    AddCommand cmd;
    
    createFile(tempDir, "文件.txt", "Chinese");
    createFile(tempDir, "مرحبا.py", "Arabic");
    createFile(tempDir, "тест.cpp", "Russian");
    createFile(tempDir, "file📄.txt", "emoji");
    
    std::vector<std::string> args{"."};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 4);
    EXPECT_TRUE(entries.find("文件.txt") != entries.end());
    EXPECT_TRUE(entries.find("مرحبا.py") != entries.end());
    EXPECT_TRUE(entries.find("тест.cpp") != entries.end());
    EXPECT_TRUE(entries.find("file📄.txt") != entries.end());
}

// Test: Add large binary file (10MB)
TEST_F(AddCommandTest, AddLargeBinaryFile) {
    AddCommand cmd;
    
    // Create 10MB file
    fs::path largeFile = tempDir / "large.bin";
    std::ofstream out(largeFile, std::ios::binary);
    
    const size_t size = 10 * 1024 * 1024; // 10MB
    std::vector<char> buffer(1024 * 1024, 0x42); // 1MB of 0x42
    
    for (size_t i = 0; i < 10; ++i) {
        out.write(buffer.data(), buffer.size());
    }
    out.close();
    
    std::vector<std::string> args{"large.bin"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify file added to index
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 1);
    EXPECT_TRUE(entries.find("large.bin") != entries.end());
    
    // Verify size is correct
    auto entry = entries.at("large.bin");
    EXPECT_EQ(entry.sizeBytes, size);
    EXPECT_EQ(entry.hashHex.length(), 40); // SHA-1 hash
}

// Test: Add file with null bytes in content
TEST_F(AddCommandTest, AddFileWithNullBytes) {
    AddCommand cmd;
    
    fs::path file = tempDir / "nulls.bin";
    std::ofstream out(file, std::ios::binary);
    
    // Write content with null bytes
    out.write("header\0", 7);
    out.write("body\0", 5);
    out.write("footer", 6);
    out.close();
    
    std::vector<std::string> args{"nulls.bin"};
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 1);
    
    // Verify we can read it back correctly
    ObjectStore store(tempDir);
    auto entry = entries.at("nulls.bin");
    std::string content = store.readBlob(entry.hashHex);
    EXPECT_EQ(content.length(), 18); // 7 + 5 + 6
}


