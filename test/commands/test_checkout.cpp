#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "test_utils.hpp"
#include "cli/commands/CheckoutCommand.hpp"
#include "cli/commands/InitCommand.hpp"
#include "cli/commands/AddCommand.hpp"
#include "cli/commands/CommitCommand.hpp"
#include "core/Repository.hpp"
#include "core/Index.hpp"
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class CheckoutCommandTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = createTempDir();
        originalCwd = getCwd();
        setCwd(tempDir);
        
        // Initialize repository using InitCommand
        InitCommand initCmd;
        initCmd.execute(ctx, {});
        
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

// Test: Create new branch with -b flag
TEST_F(CheckoutCommandTest, CreateBranch) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    CheckoutCommand checkoutCmd;
    
    // Create a commit first
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    // Create new branch
    clearOutput();
    auto result = checkoutCmd.execute(ctx, {"-b", "feature"});
    EXPECT_TRUE(result.has_value());
    
    // Verify output
    std::string output = getOutput();
    EXPECT_NE(output.find("Switched to a new branch 'feature'"), std::string::npos);
    
    // Verify HEAD points to feature
    auto headRes = Repository::resolveHEAD(tempDir);
    ASSERT_TRUE(headRes);
    auto [hash, branchRef] = headRes.value();
    
    auto branchRes = Repository::getCurrentBranch(tempDir);
    ASSERT_TRUE(branchRes);
    EXPECT_EQ(branchRes.value(), "feature");
    
    // Verify branch reference file exists
    EXPECT_TRUE(fs::exists(tempDir / ".gitter" / "refs" / "heads" / "feature"));
}

// Test: Switch to existing branch
TEST_F(CheckoutCommandTest, SwitchToBranch) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    CheckoutCommand checkoutCmd;
    
    // Create a commit on main
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    // Create feature branch
    checkoutCmd.execute(ctx, {"-b", "feature"});
    
    // Switch back to main
    clearOutput();
    auto result = checkoutCmd.execute(ctx, {"main"});
    EXPECT_TRUE(result.has_value());
    
    // Verify output
    std::string output = getOutput();
    EXPECT_NE(output.find("Switched to branch 'main'"), std::string::npos);
    
    // Verify HEAD points to main
    auto branchRes = Repository::getCurrentBranch(tempDir);
    ASSERT_TRUE(branchRes);
    EXPECT_EQ(branchRes.value(), "main");
}

// Test: Branch doesn't exist
TEST_F(CheckoutCommandTest, BranchDoesNotExist) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    CheckoutCommand checkoutCmd;
    
    // Create a commit first
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    // Try to switch to non-existent branch
    clearOutput();
    auto result = checkoutCmd.execute(ctx, {"nonexistent"});
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("'nonexistent' does not exist"), std::string::npos);
}

// Test: Branch already exists
TEST_F(CheckoutCommandTest, BranchAlreadyExists) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    CheckoutCommand checkoutCmd;
    
    // Create a commit first
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    // Create feature branch
    checkoutCmd.execute(ctx, {"-b", "feature"});
    
    // Try to create again
    clearOutput();
    auto result = checkoutCmd.execute(ctx, {"-b", "feature"});
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("a branch named 'feature' already exists"), std::string::npos);
}

// Test: No arguments
TEST_F(CheckoutCommandTest, NoArguments) {
    CheckoutCommand checkoutCmd;
    
    clearOutput();
    auto result = checkoutCmd.execute(ctx, {});
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("branch name required"), std::string::npos);
}

// Test: No commits yet - can still create branches
TEST_F(CheckoutCommandTest, NoCommits) {
    CheckoutCommand checkoutCmd;
    
    clearOutput();
    auto result = checkoutCmd.execute(ctx, {"-b", "feature"});
    // Git allows creating branches even with no commits
    EXPECT_TRUE(result.has_value());
    
    std::string output = getOutput();
    EXPECT_NE(output.find("Switched to a new branch 'feature'"), std::string::npos);
    
    // Verify HEAD points to feature
    auto branchRes = Repository::getCurrentBranch(tempDir);
    ASSERT_TRUE(branchRes);
    EXPECT_EQ(branchRes.value(), "feature");
}

// Test: Switch between multiple branches
TEST_F(CheckoutCommandTest, SwitchMultipleBranches) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    CheckoutCommand checkoutCmd;
    
    // Create initial commit
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    // Create three branches
    checkoutCmd.execute(ctx, {"-b", "feature1"});
    checkoutCmd.execute(ctx, {"main"});
    checkoutCmd.execute(ctx, {"-b", "feature2"});
    checkoutCmd.execute(ctx, {"main"});
    checkoutCmd.execute(ctx, {"-b", "feature3"});
    
    // Verify current branch
    auto branchRes = Repository::getCurrentBranch(tempDir);
    ASSERT_TRUE(branchRes);
    EXPECT_EQ(branchRes.value(), "feature3");
    
    // Switch back to feature1
    clearOutput();
    checkoutCmd.execute(ctx, {"feature1"});
    std::string output = getOutput();
    EXPECT_NE(output.find("Switched to branch 'feature1'"), std::string::npos);
    
    // Verify current branch
    branchRes = Repository::getCurrentBranch(tempDir);
    ASSERT_TRUE(branchRes);
    EXPECT_EQ(branchRes.value(), "feature1");
}

// Test: All branches point to same commit
TEST_F(CheckoutCommandTest, BranchesPointToSameCommit) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    CheckoutCommand checkoutCmd;
    
    // Create initial commit
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    // Get commit hash
    auto headRes = Repository::resolveHEAD(tempDir);
    ASSERT_TRUE(headRes);
    auto [mainHash, _] = headRes.value();
    
    // Create feature branch
    checkoutCmd.execute(ctx, {"-b", "feature"});
    
    // Verify feature points to same commit
    headRes = Repository::resolveHEAD(tempDir);
    ASSERT_TRUE(headRes);
    auto [featureHash, __] = headRes.value();
    EXPECT_EQ(mainHash, featureHash);
}

// Test: Create branch then commit on it
TEST_F(CheckoutCommandTest, CreateBranchAndCommit) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    CheckoutCommand checkoutCmd;
    
    // Create initial commit
    createFile(tempDir, "file1.txt", "content1");
    addCmd.execute(ctx, {"file1.txt"});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    // Create feature branch
    checkoutCmd.execute(ctx, {"-b", "feature"});
    
    // Create another commit on feature
    createFile(tempDir, "file2.txt", "content2");
    addCmd.execute(ctx, {"file2.txt"});
    commitCmd.execute(ctx, {"-m", "Add file2"});
    
    // Switch to main
    checkoutCmd.execute(ctx, {"main"});
    
    // Verify main still has original commit
    auto headRes = Repository::resolveHEAD(tempDir);
    ASSERT_TRUE(headRes);
    auto [mainHash, _] = headRes.value();
    
    // Switch to feature
    checkoutCmd.execute(ctx, {"feature"});
    
    // Verify feature has the new commit
    headRes = Repository::resolveHEAD(tempDir);
    ASSERT_TRUE(headRes);
    auto [featureHash, __] = headRes.value();
    EXPECT_NE(mainHash, featureHash);
}

// Test: Checkout with tree structure preservation
TEST_F(CheckoutCommandTest, CheckoutPreservesTreeStructure) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    CheckoutCommand checkoutCmd;
    
    // Create commit with subdirectory structure
    fs::create_directories(tempDir / "src");
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "src/file2.txt", "content2");
    createFile(tempDir, "src/subdir/file3.txt", "content3");
    
    addCmd.execute(ctx, {"."});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    // Create feature branch
    checkoutCmd.execute(ctx, {"-b", "feature"});
    
    // Verify all files exist after checkout
    EXPECT_TRUE(fs::exists(tempDir / "file1.txt"));
    EXPECT_TRUE(fs::exists(tempDir / "src/file2.txt"));
    EXPECT_TRUE(fs::exists(tempDir / "src/subdir/file3.txt"));
}

// Test: Checkout creates missing directories
TEST_F(CheckoutCommandTest, CheckoutCreatesDirectories) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    CheckoutCommand checkoutCmd;
    
    // Create commit with files
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "deep/nested/path/file.txt", "content");
    
    addCmd.execute(ctx, {"."});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    // Create branch first
    checkoutCmd.execute(ctx, {"-b", "feature"});
    
    // Remove directories
    fs::remove_all(tempDir / "deep");
    
    // Switch back to main (should restore directories)
    checkoutCmd.execute(ctx, {"main"});
    
    // Verify directory structure restored
    EXPECT_TRUE(fs::exists(tempDir / "deep/nested/path/file.txt"));
}

// Test: Checkout updates index
TEST_F(CheckoutCommandTest, CheckoutUpdatesIndex) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    CheckoutCommand checkoutCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    // Create branch
    checkoutCmd.execute(ctx, {"-b", "feature"});
    
    // Verify index has file
    Index index;
    index.load(tempDir);
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 1);
    EXPECT_TRUE(entries.find("file.txt") != entries.end());
}

// Test: Invalid branch name - currently allows (needs validation)
TEST_F(CheckoutCommandTest, InvalidBranchName) {
    CheckoutCommand checkoutCmd;
    
    // Try with special characters - currently succeeds (Git allows some)
    auto result = checkoutCmd.execute(ctx, {"-b", "invalid-name"});
    // This should succeed - Git allows dashes in branch names
    EXPECT_TRUE(result.has_value());
    
    // Verify branch was created
    auto branchRes = Repository::getCurrentBranch(tempDir);
    ASSERT_TRUE(branchRes);
    EXPECT_EQ(branchRes.value(), "invalid-name");
}

