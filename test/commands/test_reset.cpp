#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "test_utils.hpp"
#include "cli/commands/ResetCommand.hpp"
#include "cli/commands/AddCommand.hpp"
#include "cli/commands/CommitCommand.hpp"
#include "cli/commands/LogCommand.hpp"
#include "cli/commands/StatusCommand.hpp"
#include "core/Repository.hpp"
#include "core/ObjectStore.hpp"
#include "core/CommitObject.hpp"
#include "core/Index.hpp"
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class ResetCommandTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = createTempDir();
        originalCwd = getCwd();
        setCwd(tempDir);
        
        initTestRepo(tempDir);
        
        // Capture output
        oldCout = std::cout.rdbuf();
        outputStream = std::stringstream();
        std::cout.rdbuf(outputStream.rdbuf());
    }
    
    void TearDown() override {
        std::cout.rdbuf(oldCout);
        setCwd(originalCwd);
        removeDir(tempDir);
    }
    
    std::string getOutput() {
        return outputStream.str();
    }
    
    void clearOutput() {
        outputStream.str("");
        outputStream.clear();
    }
    
    fs::path tempDir;
    fs::path originalCwd;
    AppContext ctx;
    std::stringstream outputStream;
    std::streambuf* oldCout;
};

// Helper: Get current HEAD commit hash
std::string getHEAD(const fs::path& root) {
    std::filesystem::path headPath = root / ".gitter" / "HEAD";
    std::ifstream headFile(headPath);
    std::string headContent;
    std::getline(headFile, headContent);
    headFile.close();
    
    if (headContent.rfind("ref: ", 0) == 0) {
        std::string refPath = headContent.substr(5);
        std::filesystem::path refFile = root / ".gitter" / refPath;
        std::ifstream rf(refFile);
        std::string hash;
        std::getline(rf, hash);
        rf.close();
        return hash;
    }
    return headContent;
}

// Test: Reset with no arguments (should fail)
TEST_F(ResetCommandTest, ResetNoArgs) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    ResetCommand resetCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    clearOutput();
    auto result = resetCmd.execute(ctx, {});
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("target commit required") != std::string::npos);
}

// Test: Reset to HEAD (no change)
TEST_F(ResetCommandTest, ResetToHEAD) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    ResetCommand resetCmd;
    
    createFile(tempDir, "file.txt", "content1");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "First"});
    
    std::string initialHash = getHEAD(tempDir);
    
    clearOutput();
    auto result = resetCmd.execute(ctx, {"HEAD"});
    
    EXPECT_TRUE(result.has_value());
    std::string finalHash = getHEAD(tempDir);
    EXPECT_EQ(initialHash, finalHash);
}

// Test: Reset to HEAD~1 (one commit back)
TEST_F(ResetCommandTest, ResetOneCommitBack) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    ResetCommand resetCmd;
    
    // Create first commit
    createFile(tempDir, "file.txt", "content1");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "First"});
    std::string firstHash = getHEAD(tempDir);
    
    // Create second commit
    createFile(tempDir, "file.txt", "content2");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Second"});
    
    // Reset to first commit
    clearOutput();
    auto result = resetCmd.execute(ctx, {"HEAD~1"});
    
    EXPECT_TRUE(result.has_value());
    std::string resetHash = getHEAD(tempDir);
    EXPECT_EQ(firstHash, resetHash);
}

// Test: Reset to HEAD~2 (two commits back)
TEST_F(ResetCommandTest, ResetTwoCommitsBack) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    ResetCommand resetCmd;
    
    // Create first commit
    createFile(tempDir, "file.txt", "content1");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "First"});
    std::string firstHash = getHEAD(tempDir);
    
    // Create second commit
    createFile(tempDir, "file.txt", "content2");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Second"});
    
    // Create third commit
    createFile(tempDir, "file.txt", "content3");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Third"});
    
    // Reset to first commit
    clearOutput();
    auto result = resetCmd.execute(ctx, {"HEAD~2"});
    
    EXPECT_TRUE(result.has_value());
    std::string resetHash = getHEAD(tempDir);
    EXPECT_EQ(firstHash, resetHash);
}

// Test: Reset beyond root commit (should fail)
TEST_F(ResetCommandTest, ResetBeyondRootCommit) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    ResetCommand resetCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Only"});
    
    // Reset to HEAD~1 (root commit has no parent, should fail)
    clearOutput();
    auto result = resetCmd.execute(ctx, {"HEAD~1"});
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("reached root commit") != std::string::npos);
}

// Test: Reset clears index (unindexes changes)
TEST_F(ResetCommandTest, ResetClearsIndex) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    ResetCommand resetCmd;
    StatusCommand statusCmd;
    
    // Create initial commit
    createFile(tempDir, "file1.txt", "content1");
    addCmd.execute(ctx, {"file1.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    // Create second commit
    createFile(tempDir, "file2.txt", "content2");
    addCmd.execute(ctx, {"file2.txt"});
    commitCmd.execute(ctx, {"-m", "Second"});
    
    // Add new file to index (but don't commit)
    createFile(tempDir, "file3.txt", "content3");
    addCmd.execute(ctx, {"file3.txt"});
    
    // Verify index has file3
    Index index;
    index.load(tempDir);
    EXPECT_TRUE(index.entries().find("file3.txt") != index.entries().end());
    
    // Reset to previous commit
    clearOutput();
    auto result = resetCmd.execute(ctx, {"HEAD~1"});
    
    EXPECT_TRUE(result.has_value());
    
    // Verify index is cleared
    index.load(tempDir);
    EXPECT_TRUE(index.entries().empty());
    
    // Verify file3 still exists in working tree
    EXPECT_TRUE(fs::exists(tempDir / "file3.txt"));
}

// Test: Reset with invalid format
TEST_F(ResetCommandTest, ResetInvalidFormat) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    ResetCommand resetCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    clearOutput();
    auto result = resetCmd.execute(ctx, {"invalid"});
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("only HEAD and HEAD~n are supported") != std::string::npos);
}

// Test: Reset with negative number
TEST_F(ResetCommandTest, ResetNegativeNumber) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    ResetCommand resetCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    clearOutput();
    auto result = resetCmd.execute(ctx, {"HEAD~-1"});
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("negative steps not allowed") != std::string::npos);
}

// Test: Reset with invalid HEAD~n format
TEST_F(ResetCommandTest, ResetInvalidHEADFormat) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    ResetCommand resetCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    clearOutput();
    auto result = resetCmd.execute(ctx, {"HEAD~abc"});
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("invalid HEAD~n format") != std::string::npos);
}

// Test: Reset on empty repository
TEST_F(ResetCommandTest, ResetEmptyRepository) {
    ResetCommand resetCmd;
    
    clearOutput();
    auto result = resetCmd.execute(ctx, {"HEAD~1"});
    
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("no commits yet") != std::string::npos);
}

// Test: Reset then check log
TEST_F(ResetCommandTest, ResetCheckLog) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    ResetCommand resetCmd;
    LogCommand logCmd;
    
    // Create 3 commits
    createFile(tempDir, "file.txt", "content1");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "First"});
    
    createFile(tempDir, "file.txt", "content2");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Second"});
    
    createFile(tempDir, "file.txt", "content3");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Third"});
    
    // Verify log shows 3 commits
    clearOutput();
    logCmd.execute(ctx, {});
    std::string outputBefore = getOutput();
    EXPECT_NE(outputBefore.find("First"), std::string::npos);
    EXPECT_NE(outputBefore.find("Second"), std::string::npos);
    EXPECT_NE(outputBefore.find("Third"), std::string::npos);
    
    // Reset to HEAD~1
    resetCmd.execute(ctx, {"HEAD~1"});
    
    // Verify log now shows only 2 commits
    clearOutput();
    logCmd.execute(ctx, {});
    std::string outputAfter = getOutput();
    EXPECT_NE(outputAfter.find("First"), std::string::npos);
    EXPECT_NE(outputAfter.find("Second"), std::string::npos);
    EXPECT_EQ(outputAfter.find("Third"), std::string::npos); // Third should be gone
}

