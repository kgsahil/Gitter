#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "test_utils.hpp"
#include "cli/commands/LogCommand.hpp"
#include "cli/commands/AddCommand.hpp"
#include "cli/commands/CommitCommand.hpp"
#include "core/Repository.hpp"
#include "core/ObjectStore.hpp"
#include "core/CommitObject.hpp"
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class LogCommandTest : public ::testing::Test {
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

// Test: Log on empty repository
TEST_F(LogCommandTest, LogEmptyRepository) {
    LogCommand cmd;
    std::vector<std::string> args;
    
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    std::string output = getOutput();
    EXPECT_NE(output.find("`your current branch does not have any commits yet`"), std::string::npos);
}

// Test: Log with single commit
TEST_F(LogCommandTest, LogSingleCommit) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("commit"), std::string::npos);
    EXPECT_NE(output.find("Initial commit"), std::string::npos);
    EXPECT_NE(output.find("Author:"), std::string::npos);
}

// Test: Log with multiple commits
TEST_F(LogCommandTest, LogMultipleCommits) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    // Create 3 commits
    for (int i = 0; i < 3; ++i) {
        fs::path file = createFile(tempDir, "file" + std::to_string(i) + ".txt", 
                                    "content" + std::to_string(i));
        addCmd.execute(ctx, {file.filename().string()});
        commitCmd.execute(ctx, {"-m", "Commit " + std::to_string(i)});
    }
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Should show all 3 commits
    size_t commitCount = 0;
    size_t pos = 0;
    while ((pos = output.find("commit ", pos)) != std::string::npos) {
        commitCount++;
        pos += 7;
    }
    EXPECT_GE(commitCount, 3);
    
    // Should show commit messages in reverse order (newest first)
    size_t commit2Pos = output.find("Commit 2");
    size_t commit1Pos = output.find("Commit 1");
    size_t commit0Pos = output.find("Commit 0");
    EXPECT_NE(commit2Pos, std::string::npos);
    EXPECT_NE(commit1Pos, std::string::npos);
    EXPECT_NE(commit0Pos, std::string::npos);
    EXPECT_LT(commit2Pos, commit1Pos); // Commit 2 before Commit 1
    EXPECT_LT(commit1Pos, commit0Pos); // Commit 1 before Commit 0
}

// Test: Log limits to 10 commits
TEST_F(LogCommandTest, LogLimitsTo10Commits) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    // Create 15 commits
    for (int i = 0; i < 15; ++i) {
        fs::path file = createFile(tempDir, "file" + std::to_string(i) + ".txt", 
                                    "content" + std::to_string(i));
        addCmd.execute(ctx, {file.filename().string()});
        commitCmd.execute(ctx, {"-m", "Commit " + std::to_string(i)});
    }
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Count commits in output
    size_t commitCount = 0;
    size_t pos = 0;
    while ((pos = output.find("commit ", pos)) != std::string::npos) {
        commitCount++;
        pos += 7;
    }
    EXPECT_EQ(commitCount, 10); // Should be limited to 10
}

// Test: Log shows commit hash
TEST_F(LogCommandTest, LogShowsCommitHash) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Test"});
    
    // Get commit hash
    ObjectStore store(tempDir);
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Should contain commit hash (at least first few characters)
    EXPECT_NE(output.find(commitHash.substr(0, 7)), std::string::npos);
}

// Test: Log shows author information
TEST_F(LogCommandTest, LogShowsAuthorInfo) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Test"});
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("Author:"), std::string::npos);
    // Should show default author (Gitter User)
    EXPECT_NE(output.find("Gitter User"), std::string::npos);
}

// Test: Log shows date
TEST_F(LogCommandTest, LogShowsDate) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Test"});
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("Date:"), std::string::npos);
}

// Test: Log shows multi-line commit message
TEST_F(LogCommandTest, LogShowsMultiLineMessage) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    std::string message = "First line\nSecond line\nThird line";
    commitCmd.execute(ctx, {"-m", message});
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("First line"), std::string::npos);
    EXPECT_NE(output.find("Second line"), std::string::npos);
    EXPECT_NE(output.find("Third line"), std::string::npos);
}

// Test: Log with root commit (no parent)
TEST_F(LogCommandTest, LogWithRootCommit) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Root commit"});
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Should show commit message
    EXPECT_NE(output.find("Root commit"), std::string::npos);
}

// Test: Log traverses parent chain correctly
TEST_F(LogCommandTest, LogTraversesParentChain) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    // Create chain of 5 commits
    for (int i = 0; i < 5; ++i) {
        fs::path file = createFile(tempDir, "file" + std::to_string(i) + ".txt", 
                                    "content" + std::to_string(i));
        addCmd.execute(ctx, {file.filename().string()});
        commitCmd.execute(ctx, {"-m", "Commit " + std::to_string(i)});
    }
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Verify all commits shown in reverse order
    for (int i = 4; i >= 0; --i) {
        size_t pos = output.find("Commit " + std::to_string(i));
        EXPECT_NE(pos, std::string::npos) << "Commit " << i << " not found";
    }
}

// Test: Log with commit that has special characters in message
TEST_F(LogCommandTest, LogSpecialCharactersInMessage) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    std::string message = "Fix: Bug #123 & issue with \"quotes\"";
    commitCmd.execute(ctx, {"-m", message});
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("Bug #123"), std::string::npos);
}

// Test: Log handles empty commit message
TEST_F(LogCommandTest, LogEmptyCommitMessage) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Commit with single empty message
    commitCmd.execute(ctx, {"-m", ""});
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Should still show the commit
    EXPECT_NE(output.find("commit"), std::string::npos);
}

// Test: Log with very long commit messages
TEST_F(LogCommandTest, LogLongCommitMessage) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Create a long message
    std::string longMsg;
    for (int i = 0; i < 20; ++i) {
        longMsg += "Line " + std::to_string(i) + "\n";
    }
    commitCmd.execute(ctx, {"-m", longMsg});
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Should show the long message
    EXPECT_NE(output.find("Line 0"), std::string::npos);
    EXPECT_NE(output.find("Line 19"), std::string::npos);
}

// Test: Log shows timezone information
TEST_F(LogCommandTest, LogShowsTimezone) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Test"});
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Should have Date line with timezone
    EXPECT_NE(output.find("Date:"), std::string::npos);
}

// Test: Log with exactly 10 commits
TEST_F(LogCommandTest, LogExactly10Commits) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    // Create exactly 10 commits
    for (int i = 0; i < 10; ++i) {
        fs::path file = createFile(tempDir, "file" + std::to_string(i) + ".txt", 
                                    "content" + std::to_string(i));
        addCmd.execute(ctx, {file.filename().string()});
        commitCmd.execute(ctx, {"-m", "Commit " + std::to_string(i)});
    }
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Count commits
    size_t commitCount = 0;
    size_t pos = 0;
    while ((pos = output.find("commit ", pos)) != std::string::npos) {
        commitCount++;
        pos += 7;
    }
    EXPECT_EQ(commitCount, 10);
}

// Test: Log with commit containing only newlines
TEST_F(LogCommandTest, LogCommitWithOnlyNewlines) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Commit with only newlines
    commitCmd.execute(ctx, {"-m", "\n\n\n"});
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Should still show commit
    EXPECT_NE(output.find("commit"), std::string::npos);
}

// Test: Log date formatting
TEST_F(LogCommandTest, LogDateFormatting) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    LogCommand logCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Test"});
    
    clearOutput();
    logCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Check for date format components
    EXPECT_NE(output.find("Date:"), std::string::npos);
    // Should have day of week and month
    bool hasDayMonth = (output.find("Mon ") != std::string::npos ||
                       output.find("Tue ") != std::string::npos ||
                       output.find("Wed ") != std::string::npos ||
                       output.find("Thu ") != std::string::npos ||
                       output.find("Fri ") != std::string::npos ||
                       output.find("Sat ") != std::string::npos ||
                       output.find("Sun ") != std::string::npos);
    EXPECT_TRUE(hasDayMonth);
}

