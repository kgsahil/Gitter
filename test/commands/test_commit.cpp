#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "test_utils.hpp"
#include "cli/commands/CommitCommand.hpp"
#include "cli/commands/AddCommand.hpp"
#include "core/Repository.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"
#include "core/CommitObject.hpp"
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class CommitCommandTest : public ::testing::Test {
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

// Test: Commit with -m flag
TEST_F(CommitCommandTest, CommitWithMessage) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    // Add file
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Commit
    std::vector<std::string> args{"-m", "Initial commit"};
    auto result = commitCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify commit object exists
    fs::path headPath = tempDir / ".gitter" / "HEAD";
    std::ifstream headFile(headPath);
    std::string headContent;
    std::getline(headFile, headContent);
    headFile.close();
    
    // Follow ref
    std::string refPath = headContent.substr(5); // Remove "ref: "
    fs::path refFile = tempDir / ".gitter" / refPath;
    ASSERT_TRUE(fs::exists(refFile));
    
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    rf.close();
    
    // Verify commit object exists
    ObjectStore store(tempDir);
    fs::path objPath = store.getObjectPath(commitHash);
    EXPECT_TRUE(fs::exists(objPath));
}

// Test: Commit empty index (should fail)
TEST_F(CommitCommandTest, CommitEmptyIndex) {
    CommitCommand cmd;
    std::vector<std::string> args{"-m", "Empty commit"};
    
    auto result = cmd.execute(ctx, args);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgs);
}

// Test: Commit without -m flag (should fail)
TEST_F(CommitCommandTest, CommitWithoutMessage) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    // Add file
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Commit without message
    std::vector<std::string> args;
    auto result = commitCmd.execute(ctx, args);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidArgs);
}

// Test: Create root commit (no parent)
TEST_F(CommitCommandTest, RootCommit) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    std::vector<std::string> args{"-m", "Root commit"};
    auto result = commitCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify commit object
    ObjectStore store(tempDir);
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    
    auto commit = store.readCommit(commitHash);
    EXPECT_TRUE(commit.parentHashes.empty()); // Root commit has no parent
}

// Test: Create commit with parent
TEST_F(CommitCommandTest, CommitWithParent) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    // First commit
    createFile(tempDir, "file1.txt", "content1");
    addCmd.execute(ctx, {"file1.txt"});
    commitCmd.execute(ctx, {"-m", "First commit"});
    
    // Second commit
    createFile(tempDir, "file2.txt", "content2");
    addCmd.execute(ctx, {"file2.txt"});
    
    std::vector<std::string> args{"-m", "Second commit"};
    auto result = commitCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify second commit has parent
    ObjectStore store(tempDir);
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    
    auto commit = store.readCommit(commitHash);
    EXPECT_EQ(commit.message.find("Second commit"), 0);
    EXPECT_EQ(commit.parentHashes.size(), 1); // Has parent
}

// Test: Commit with -am flag (shorthand)
TEST_F(CommitCommandTest, CommitWithAMFlag) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    // First commit
    createFile(tempDir, "file.txt", "content1");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "First"});
    
    // Modify file
    createFile(tempDir, "file.txt", "content2");
    
    // Commit with -am (should auto-stage and commit modified file)
    std::vector<std::string> args{"-am", "Update with -am"};
    auto result = commitCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify commit was created
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    rf.close();
    
    ObjectStore store(tempDir);
    auto commit = store.readCommit(commitHash);
    EXPECT_EQ(commit.message, "Update with -am\n");
    
    // Verify modified file is in commit
    fs::path objPath = store.getObjectPath(commit.treeHash);
    ASSERT_TRUE(fs::exists(objPath));
}

// Test: Multiple commits in sequence
TEST_F(CommitCommandTest, MultipleCommitsInSequence) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    for (int i = 0; i < 3; ++i) {
        fs::path filePath = createFile(tempDir, "file" + std::to_string(i) + ".txt", 
                                        "content" + std::to_string(i));
        addCmd.execute(ctx, {filePath.filename().string()});
        
        std::vector<std::string> args{"-m", "Commit " + std::to_string(i)};
        auto result = commitCmd.execute(ctx, args);
        ASSERT_TRUE(result.has_value()) << "Failed at commit " << i;
    }
    
    // Verify all commits exist
    ObjectStore store(tempDir);
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    
    // Traverse commit chain
    int count = 0;
    std::string current = commitHash;
    while (!current.empty() && count < 10) {
        auto commit = store.readCommit(current);
        count++;
        if (!commit.parentHashes.empty()) {
            current = commit.parentHashes[0];
        } else {
            break;
        }
    }
    
    EXPECT_EQ(count, 3); // Should have 3 commits
}

// Test: Commit with subdirectories
TEST_F(CommitCommandTest, CommitWithSubdirectories) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    fs::create_directories(tempDir / "src" / "util");
    createFile(tempDir, "src/main.cpp", "int main() {}");
    createFile(tempDir, "src/util/helper.cpp", "void helper() {}");
    createFile(tempDir, "README.md", "Readme");
    
    addCmd.execute(ctx, {"."});
    
    std::vector<std::string> args{"-m", "Add project structure"};
    auto result = commitCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify tree objects created
    ObjectStore store(tempDir);
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    
    auto commit = store.readCommit(commitHash);
    // Tree hash should exist
    EXPECT_FALSE(commit.treeHash.empty());
    
    // Verify tree object exists
    fs::path treePath = store.getObjectPath(commit.treeHash);
    EXPECT_TRUE(fs::exists(treePath));
}

// Test: Commit updates branch reference
TEST_F(CommitCommandTest, CommitUpdatesBranchReference) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    std::vector<std::string> args{"-m", "Test commit"};
    auto result = commitCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify branch reference updated
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    ASSERT_TRUE(fs::exists(refFile));
    
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    
    // Should be a valid commit hash (40 chars for SHA-1)
    EXPECT_EQ(commitHash.length(), 40);
}

// Test: Commit with environment variables
TEST_F(CommitCommandTest, CommitWithEnvironmentVariables) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Set environment variables (would need to actually set them in test)
    // For now, test that it uses defaults
    std::vector<std::string> args{"-m", "Test"};
    auto result = commitCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify commit object has author info
    ObjectStore store(tempDir);
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    
    auto commit = store.readCommit(commitHash);
    EXPECT_FALSE(commit.authorName.empty());
    EXPECT_FALSE(commit.authorEmail.empty());
}

// Test: Commit with long message
TEST_F(CommitCommandTest, CommitWithLongMessage) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    std::string longMessage = "This is a very long commit message that spans multiple lines.\n"
                               "It contains detailed information about what was changed.\n"
                               "And why the change was made.";
    
    std::vector<std::string> args{"-m", longMessage};
    auto result = commitCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify message stored correctly
    ObjectStore store(tempDir);
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    
    auto commit = store.readCommit(commitHash);
    EXPECT_NE(commit.message.find("This is a very long"), std::string::npos);
    EXPECT_NE(commit.message.find("multiple lines"), std::string::npos);
}

// Test: Commit creates tree objects
TEST_F(CommitCommandTest, CommitCreatesTreeObjects) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    fs::create_directories(tempDir / "src");
    createFile(tempDir, "src/main.cpp", "int main() {}");
    createFile(tempDir, "README.md", "Readme");
    
    addCmd.execute(ctx, {"."});
    commitCmd.execute(ctx, {"-m", "Add files"});
    
    // Get commit
    ObjectStore store(tempDir);
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    
    auto commit = store.readCommit(commitHash);
    
    // Verify tree object exists
    fs::path treePath = store.getObjectPath(commit.treeHash);
    EXPECT_TRUE(fs::exists(treePath));
}

// Test: Commit message with special characters
TEST_F(CommitCommandTest, CommitMessageWithSpecialChars) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    std::string message = "Fix: Bug #123 & issue \"quoted\" with 'apostrophes'";
    std::vector<std::string> args{"-m", message};
    auto result = commitCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify message stored correctly
    ObjectStore store(tempDir);
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    
    auto commit = store.readCommit(commitHash);
    EXPECT_EQ(commit.message, message + "\n");
}

// Test: Commit with -a flag (auto-stage all modified files)
TEST_F(CommitCommandTest, CommitWithAutoStageFlag) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    // First commit
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    addCmd.execute(ctx, {"file1.txt", "file2.txt"});
    commitCmd.execute(ctx, {"-m", "First"});
    
    // Modify both files
    createFile(tempDir, "file1.txt", "modified content1");
    createFile(tempDir, "file2.txt", "modified content2");
    
    // Commit with -a should auto-stage both modified files
    std::vector<std::string> args{"-a", "-m", "Update with -a"};
    auto result = commitCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify commit was created
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    rf.close();
    
    ObjectStore store(tempDir);
    auto commit = store.readCommit(commitHash);
    EXPECT_EQ(commit.message, "Update with -a\n");
}

// Test: Commit with -am flag on multiple files
TEST_F(CommitCommandTest, CommitWithAMFlagMultipleFiles) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    // First commit with 3 files
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    createFile(tempDir, "file3.txt", "content3");
    addCmd.execute(ctx, {"file1.txt", "file2.txt", "file3.txt"});
    commitCmd.execute(ctx, {"-m", "First"});
    
    // Modify 2 files only
    createFile(tempDir, "file1.txt", "modified1");
    createFile(tempDir, "file3.txt", "modified3");
    
    // Commit with -am should auto-stage only modified files
    std::vector<std::string> args{"-am", "Update modified files"};
    auto result = commitCmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify commit was created
    fs::path refFile = tempDir / ".gitter" / "refs" / "heads" / "main";
    std::ifstream rf(refFile);
    std::string commitHash;
    std::getline(rf, commitHash);
    rf.close();
    
    ObjectStore store(tempDir);
    auto commit = store.readCommit(commitHash);
    EXPECT_EQ(commit.message, "Update modified files\n");
}

// Test: Commit with -a on empty working tree (should fail)
TEST_F(CommitCommandTest, CommitWithANoChanges) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    // First commit
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "First"});
    
    // Don't modify anything
    // Commit with -a should fail (nothing to commit)
    std::vector<std::string> args{"-a", "-m", "No changes"};
    auto result = commitCmd.execute(ctx, args);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.find("nothing to commit") != std::string::npos);
}

// Test: Commit with multiple -m flags (multi-paragraph message)
TEST_F(CommitCommandTest, CommitWithMultipleMessages) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    // Add file
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Commit with multiple messages
    auto result = commitCmd.execute(ctx, {"-m", "First line", "-m", "Second paragraph"});
    EXPECT_TRUE(result.has_value());
    
    // Verify the commit message combines with blank lines
    ObjectStore store(tempDir);
    auto headRes = Repository::resolveHEAD(tempDir);
    ASSERT_TRUE(headRes);
    std::string hash = headRes.value().first;
    
    CommitObject commit = store.readCommit(hash);
    EXPECT_EQ(commit.message, "First line\n\nSecond paragraph\n");
}

// Test: Commit with three -m flags
TEST_F(CommitCommandTest, CommitWithThreeMessages) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    
    // Add file
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    // Commit with three messages
    auto result = commitCmd.execute(ctx, {"-m", "Summary", "-m", "Details", "-m", "Notes"});
    EXPECT_TRUE(result.has_value());
    
    // Verify the commit message combines with blank lines
    ObjectStore store(tempDir);
    auto headRes = Repository::resolveHEAD(tempDir);
    ASSERT_TRUE(headRes);
    std::string hash = headRes.value().first;
    
    CommitObject commit = store.readCommit(hash);
    EXPECT_EQ(commit.message, "Summary\n\nDetails\n\nNotes\n");
}

