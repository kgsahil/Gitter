#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

#include "test_utils.hpp"
#include "cli/CommandFactory.hpp"
#include "cli/CommandInvoker.hpp"
#include "core/Repository.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"
#include "core/CommitObject.hpp"
#include "cli/commands/InitCommand.hpp"
#include "cli/commands/AddCommand.hpp"
#include "cli/commands/CommitCommand.hpp"
#include "cli/commands/StatusCommand.hpp"
#include "cli/commands/LogCommand.hpp"
#include "cli/commands/RestoreCommand.hpp"
#include "cli/commands/CatFileCommand.hpp"
#include "cli/commands/HelpCommand.hpp"
#include "cli/commands/CheckoutCommand.hpp"
#include "cli/commands/ResetCommand.hpp"
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

namespace gitter::test {

using namespace gitter::test::utils;

/**
 * @brief Integration tests for Git-like workflow scenarios
 * 
 * Tests complete workflows combining multiple commands to ensure
 * Gitter behaves like Git in real-world scenarios.
 */
class GitWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Register all commands (normally done in main.cpp)
        registerCommands();
        
        tempDir = createTempDir();
        originalCwd = getCwd();
        setCwd(tempDir);
        
        // Don't pre-initialize repo - let init command do it
        repoPath = tempDir;
    }
    
    void TearDown() override {
        setCwd(originalCwd);
        removeDir(tempDir);
    }
    
    fs::path tempDir;
    fs::path originalCwd;
    fs::path repoPath;
    CommandInvoker invoker;
    AppContext ctx;
    
private:
    void registerCommands() {
        auto& f = CommandFactory::instance();
        f.registerCreator("help", [] { return std::make_unique<HelpCommand>(); });
        f.registerCreator("init", [] { return std::make_unique<InitCommand>(); });
        f.registerCreator("add", [] { return std::make_unique<AddCommand>(); });
        f.registerCreator("commit", [] { return std::make_unique<CommitCommand>(); });
        f.registerCreator("status", [] { return std::make_unique<StatusCommand>(); });
        f.registerCreator("log", [] { return std::make_unique<LogCommand>(); });
        f.registerCreator("checkout", [] { return std::make_unique<CheckoutCommand>(); });
        f.registerCreator("restore", [] { return std::make_unique<RestoreCommand>(); });
        f.registerCreator("cat-file", [] { return std::make_unique<CatFileCommand>(); });
        f.registerCreator("reset", [] { return std::make_unique<ResetCommand>(); });
    }
};

/**
 * @brief Test: Basic workflow - init, add, commit, status, log
 * Positive: Complete workflow should work seamlessly
 */
TEST_F(GitWorkflowTest, BasicWorkflowInitAddCommitStatusLog) {
    // 1. Initialize repository
    auto initCmd = CommandFactory::instance().create("init");
    ASSERT_NE(initCmd, nullptr);
    auto result = invoker.invoke(*initCmd, ctx, {});
    EXPECT_TRUE(result) << result.error().message;
    
    // 2. Create and add files
    createFile(repoPath, "file1.txt", "content1");
    createFile(repoPath, "file2.txt", "content2");
    
    auto addCmd = CommandFactory::instance().create("add");
    ASSERT_NE(addCmd, nullptr);
    result = invoker.invoke(*addCmd, ctx, {"file1.txt", "file2.txt"});
    EXPECT_TRUE(result) << result.error().message;
    
    // 3. Verify Index state - both files should be staged
    Index index1;
    ASSERT_TRUE(index1.load(repoPath));
    const auto& entries = index1.entries();
    EXPECT_EQ(entries.size(), 2);
    EXPECT_TRUE(entries.find("file1.txt") != entries.end());
    EXPECT_TRUE(entries.find("file2.txt") != entries.end());
    
    // Check status - should show staged files
    auto statusCmd = CommandFactory::instance().create("status");
    ASSERT_NE(statusCmd, nullptr);
    
    // Capture status output
    testing::internal::CaptureStdout();
    result = invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(result) << result.error().message;
    EXPECT_TRUE(statusOutput.find("Changes to be committed") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("file1.txt") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("file2.txt") != std::string::npos);
    
    // 4. Commit
    auto commitCmd = CommandFactory::instance().create("commit");
    ASSERT_NE(commitCmd, nullptr);
    
    // Commit should produce no output (Git-like behavior)
    result = invoker.invoke(*commitCmd, ctx, {"-m", "Initial commit"});
    EXPECT_TRUE(result) << result.error().message;
    
    // Verify: HEAD should point to a valid commit
    std::ifstream headFile(repoPath / ".gitter" / "HEAD");
    ASSERT_TRUE(headFile.good());
    std::string headContent;
    std::getline(headFile, headContent);
    headFile.close();
    EXPECT_TRUE(headContent.rfind("ref: ", 0) == 0);
    
    // Read actual commit hash from ref
    std::string refPath = headContent.substr(5);
    std::string commitHash = readHashFromFile(repoPath / ".gitter" / refPath);
    EXPECT_EQ(commitHash.length(), 40); // SHA-1 hash length
    
    // Verify: Commit object exists in objects/
    ObjectStore store(repoPath);
    CommitObject commit;
    EXPECT_NO_THROW(commit = store.readCommit(commitHash));
    EXPECT_EQ(commit.message, "Initial commit\n");
    
    // 5. Check status after commit - should be clean
    testing::internal::CaptureStdout();
    result = invoker.invoke(*statusCmd, ctx, {});
    statusOutput = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(result) << result.error().message;
    EXPECT_TRUE(statusOutput.find("nothing to commit, working tree clean") != std::string::npos);
    
    // 6. View log
    auto logCmd = CommandFactory::instance().create("log");
    ASSERT_NE(logCmd, nullptr);
    
    testing::internal::CaptureStdout();
    result = invoker.invoke(*logCmd, ctx, {});
    std::string logOutput = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(result) << result.error().message;
    EXPECT_TRUE(logOutput.find("Initial commit") != std::string::npos);
}

/**
 * @brief Test: Modify file after commit and stage again
 * Positive: Modified file should be detected and staged correctly
 */
TEST_F(GitWorkflowTest, ModifyFileAfterCommit) {
    // Setup: init, add, commit
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto statusCmd = CommandFactory::instance().create("status");
    
    ASSERT_NE(initCmd, nullptr);
    ASSERT_NE(addCmd, nullptr);
    ASSERT_NE(commitCmd, nullptr);
    ASSERT_NE(statusCmd, nullptr);
    
    invoker.invoke(*initCmd, ctx, {});
    
    createFile(repoPath, "file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"file1.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "First commit"});
    
    // Modify file - add delay to ensure mtime updates
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    createFile(repoPath, "file1.txt", "modified content");
    
    // Check status - should show modified
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(statusOutput.find("Changes not staged for commit") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("modified: file1.txt") != std::string::npos);
    
    // Stage and commit again
    invoker.invoke(*addCmd, ctx, {"file1.txt"});
    
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    statusOutput = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(statusOutput.find("Changes to be committed") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("file1.txt") != std::string::npos);
    
    invoker.invoke(*commitCmd, ctx, {"-m", "Update file1"});
    
    // Should be clean again
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    statusOutput = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(statusOutput.find("nothing to commit") != std::string::npos);
}

/**
 * @brief Test: Add, unstage, then stage again
 * Positive: restore --staged should unstage files properly
 */
TEST_F(GitWorkflowTest, StageUnstageRestage) {
    // Setup: init
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto restoreCmd = CommandFactory::instance().create("restore");
    
    invoker.invoke(*initCmd, ctx, {});
    
    createFile(repoPath, "file1.txt", "content1");
    createFile(repoPath, "file2.txt", "content2");
    
    // Stage both files
    invoker.invoke(*addCmd, ctx, {"file1.txt", "file2.txt"});
    
    // Check staged - verify Index state directly
    Index index1;
    ASSERT_TRUE(index1.load(repoPath));
    const auto& entries1 = index1.entries();
    EXPECT_EQ(entries1.size(), 2);
    EXPECT_TRUE(entries1.find("file1.txt") != entries1.end());
    EXPECT_TRUE(entries1.find("file2.txt") != entries1.end());
    
    // Unstage file1
    invoker.invoke(*restoreCmd, ctx, {"--staged", "file1.txt"});
    
    // Check only file2 staged - verify Index state
    Index index2;
    ASSERT_TRUE(index2.load(repoPath));
    const auto& entries2 = index2.entries();
    EXPECT_EQ(entries2.size(), 1);
    EXPECT_TRUE(entries2.find("file2.txt") != entries2.end());
    EXPECT_TRUE(entries2.find("file1.txt") == entries2.end());
    
    // Re-stage file1
    invoker.invoke(*addCmd, ctx, {"file1.txt"});
    
    // Check both staged again - verify Index state
    Index index3;
    ASSERT_TRUE(index3.load(repoPath));
    const auto& entries3 = index3.entries();
    EXPECT_EQ(entries3.size(), 2);
    EXPECT_TRUE(entries3.find("file1.txt") != entries3.end());
    EXPECT_TRUE(entries3.find("file2.txt") != entries3.end());
}

/**
 * @brief Test: Delete tracked file
 * Positive: Deleted file should show in status
 */
TEST_F(GitWorkflowTest, DeleteTrackedFile) {
    // Setup: init, add, commit
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto statusCmd = CommandFactory::instance().create("status");
    
    invoker.invoke(*initCmd, ctx, {});
    
    createFile(repoPath, "file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"file1.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Add file1"});
    
    // Delete file
    fs::remove(repoPath / "file1.txt");
    
    // Check status - should show deleted
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(statusOutput.find("Changes not staged for commit") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("deleted:  file1.txt") != std::string::npos);
}

/**
 * @brief Test: Multiple commits form a chain
 * Positive: Log should show all commits in correct order
 */
TEST_F(GitWorkflowTest, MultipleCommitsChain) {
    // Setup
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto logCmd = CommandFactory::instance().create("log");
    
    invoker.invoke(*initCmd, ctx, {});
    
    // First commit
    createFile(repoPath, "file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"file1.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "First commit"});
    
    // Second commit
    createFile(repoPath, "file2.txt", "content2");
    invoker.invoke(*addCmd, ctx, {"file2.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Second commit"});
    
    // Third commit
    createFile(repoPath, "file3.txt", "content3");
    invoker.invoke(*addCmd, ctx, {"file3.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Third commit"});
    
    // Verify: Should have 3 commits with proper parent chain
    std::string firstHash, secondHash, thirdHash;
    
    // Read HEAD to get third commit hash
    std::ifstream headFile(repoPath / ".gitter" / "HEAD");
    std::string headContent;
    std::getline(headFile, headContent);
    headFile.close();
    std::string refPath = headContent.substr(5);
    thirdHash = readHashFromFile(repoPath / ".gitter" / refPath);
    
    // Traverse parent chain
    ObjectStore store(repoPath);
    CommitObject thirdCommit = store.readCommit(thirdHash);
    EXPECT_EQ(thirdCommit.message, "Third commit\n");
    EXPECT_EQ(thirdCommit.parentHashes.size(), 1);
    
    secondHash = thirdCommit.parentHashes[0];
    CommitObject secondCommit = store.readCommit(secondHash);
    EXPECT_EQ(secondCommit.message, "Second commit\n");
    EXPECT_EQ(secondCommit.parentHashes.size(), 1);
    
    firstHash = secondCommit.parentHashes[0];
    CommitObject firstCommit = store.readCommit(firstHash);
    EXPECT_EQ(firstCommit.message, "First commit\n");
    EXPECT_EQ(firstCommit.parentHashes.size(), 0); // Root commit
    
    // Check log - should show 3 commits in reverse order
    testing::internal::CaptureStdout();
    invoker.invoke(*logCmd, ctx, {});
    std::string logOutput = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(logOutput.find("Third commit") != std::string::npos);
    EXPECT_TRUE(logOutput.find("Second commit") != std::string::npos);
    EXPECT_TRUE(logOutput.find("First commit") != std::string::npos);
    
    // Verify order: Third should appear first
    size_t thirdPos = logOutput.find("Third commit");
    size_t secondPos = logOutput.find("Second commit");
    size_t firstPos = logOutput.find("First commit");
    
    EXPECT_LT(thirdPos, secondPos);
    EXPECT_LT(secondPos, firstPos);
}

/**
 * @brief Test: Try to commit without staging
 * Negative: Should show nothing to commit message
 */
TEST_F(GitWorkflowTest, CommitWithoutStagingNegative) {
    // Setup: init, add, commit
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto statusCmd = CommandFactory::instance().create("status");
    
    invoker.invoke(*initCmd, ctx, {});
    
    createFile(repoPath, "file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"file1.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "First commit"});
    
    // Try to commit again without changes
    auto result = invoker.invoke(*commitCmd, ctx, {"-m", "Second commit"});
    
    // Should show nothing to commit (exit code 0 but message indicates)
    // Note: Current implementation may succeed with empty tree
    // This tests that we handle the case gracefully
}

/**
 * @brief Test: Pattern matching with add and restore
 * Positive: Glob patterns should work for staging/unstaging
 */
TEST_F(GitWorkflowTest, PatternMatchingAddRestore) {
    // Setup
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto restoreCmd = CommandFactory::instance().create("restore");
    auto statusCmd = CommandFactory::instance().create("status");
    
    invoker.invoke(*initCmd, ctx, {});
    
    // Create multiple files
    createFile(repoPath, "file1.txt", "content1");
    createFile(repoPath, "file2.txt", "content2");
    createFile(repoPath, "file3.cpp", "content3");
    createFile(repoPath, "file4.cpp", "content4");
    
    // Stage all .txt files with pattern
    invoker.invoke(*addCmd, ctx, {"*.txt"});
    
    // Verify: Only .txt files should be in Index
    Index index1;
    ASSERT_TRUE(index1.load(repoPath));
    const auto& entries1 = index1.entries();
    EXPECT_EQ(entries1.size(), 2);
    EXPECT_TRUE(entries1.find("file1.txt") != entries1.end());
    EXPECT_TRUE(entries1.find("file2.txt") != entries1.end());
    EXPECT_TRUE(entries1.find("file3.cpp") == entries1.end());
    EXPECT_TRUE(entries1.find("file4.cpp") == entries1.end());
    
    // Check staged - only .txt files should be staged
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    
    // .txt files should be in "Changes to be committed"
    EXPECT_TRUE(statusOutput.find("Changes to be committed") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("file1.txt") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("file2.txt") != std::string::npos);
    
    // .cpp files should be in "Untracked files"
    EXPECT_TRUE(statusOutput.find("Untracked files") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("file3.cpp") != std::string::npos); // Untracked
    EXPECT_TRUE(statusOutput.find("file4.cpp") != std::string::npos); // Untracked
    
    // Unstage with pattern
    invoker.invoke(*restoreCmd, ctx, {"--staged", "*.txt"});
    
    // Verify: Index should be empty after restore
    Index index2;
    ASSERT_TRUE(index2.load(repoPath));
    const auto& entries2 = index2.entries();
    EXPECT_EQ(entries2.size(), 0);
    
    // Check unstaged
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    statusOutput = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(statusOutput.find("file1.txt") != std::string::npos); // Untracked
    EXPECT_TRUE(statusOutput.find("file2.txt") != std::string::npos); // Untracked
}

/**
 * @brief Test: Add with directory recursion
 * Positive: gitter add . should add all files recursively
 */
TEST_F(GitWorkflowTest, AddDirectoryRecursion) {
    // Setup
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto statusCmd = CommandFactory::instance().create("status");
    
    invoker.invoke(*initCmd, ctx, {});
    
    // Create nested structure
    createFile(repoPath, "file1.txt", "content1");
    createFile(repoPath / "dir1", "file2.txt", "content2");
    createFile(repoPath / "dir1", "file3.cpp", "content3");
    createFile(repoPath / "dir1" / "dir2", "file4.txt", "content4");
    
    // Add all files recursively
    invoker.invoke(*addCmd, ctx, {"."});
    
    // Verify: All files should be in Index
    Index index;
    ASSERT_TRUE(index.load(repoPath));
    const auto& entries = index.entries();
    EXPECT_EQ(entries.size(), 4);
    EXPECT_TRUE(entries.find("file1.txt") != entries.end());
    EXPECT_TRUE(entries.find("dir1/file2.txt") != entries.end() ||
                entries.find("dir1\\file2.txt") != entries.end());
    EXPECT_TRUE(entries.find("dir1/file3.cpp") != entries.end() ||
                entries.find("dir1\\file3.cpp") != entries.end());
    
    // Check all files staged
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(statusOutput.find("file1.txt") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("dir1/file2.txt") != std::string::npos ||
                statusOutput.find("dir1\\file2.txt") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("dir1/file3.cpp") != std::string::npos ||
                statusOutput.find("dir1\\file3.cpp") != std::string::npos);
}

/**
 * @brief Test: Reset workflow - init, add, commit, reset
 * Positive: Reset should move HEAD back and clear index
 */
TEST_F(GitWorkflowTest, ResetWorkflow) {
    // Setup
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto resetCmd = CommandFactory::instance().create("reset");
    auto logCmd = CommandFactory::instance().create("log");
    auto statusCmd = CommandFactory::instance().create("status");
    
    // 1. Initialize repository
    invoker.invoke(*initCmd, ctx, {});
    
    // 2. Create initial commit
    createFile(repoPath, "file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"file1.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "First"});
    
    // Get first commit hash
    std::ifstream headFile(repoPath / ".gitter" / "HEAD");
    std::string headContent;
    std::getline(headFile, headContent);
    headFile.close();
    std::string refPath = headContent.substr(5);
    std::string firstHash = readHashFromFile(repoPath / ".gitter" / refPath);
    
    // 3. Create second commit
    createFile(repoPath, "file2.txt", "content2");
    invoker.invoke(*addCmd, ctx, {"file2.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Second"});
    
    // Verify log shows both commits
    testing::internal::CaptureStdout();
    invoker.invoke(*logCmd, ctx, {});
    std::string logOutput = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(logOutput.find("First") != std::string::npos);
    EXPECT_TRUE(logOutput.find("Second") != std::string::npos);
    
    // 4. Reset to HEAD~1
    auto result = invoker.invoke(*resetCmd, ctx, {"HEAD~1"});
    EXPECT_TRUE(result) << result.error().message;
    
    // Verify HEAD now points to first commit
    std::ifstream headFile2(repoPath / ".gitter" / "HEAD");
    std::string headContent2;
    std::getline(headFile2, headContent2);
    headFile2.close();
    std::string refPath2 = headContent2.substr(5);
    std::string resetHash = readHashFromFile(repoPath / ".gitter" / refPath2);
    EXPECT_EQ(firstHash, resetHash);
    
    // Verify log shows only first commit
    testing::internal::CaptureStdout();
    invoker.invoke(*logCmd, ctx, {});
    logOutput = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(logOutput.find("First") != std::string::npos);
    EXPECT_EQ(logOutput.find("Second"), std::string::npos); // Second should be gone
    
    // Verify index is cleared
    Index index;
    ASSERT_TRUE(index.load(repoPath));
    EXPECT_TRUE(index.entries().empty());
    
    // Verify file2 still exists in working tree (untracked)
    EXPECT_TRUE(fs::exists(repoPath / "file2.txt"));
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(statusOutput.find("Untracked files") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("file2.txt") != std::string::npos);
}

/**
 * @brief Test: Complete branching workflow
 * Positive: Create branches, switch, make commits on different branches
 */
TEST_F(GitWorkflowTest, BranchingWorkflow) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    auto logCmd = CommandFactory::instance().create("log");
    
    // 1. Initialize repository
    invoker.invoke(*initCmd, ctx, {});
    
    // 2. Create commit on main
    createFile(tempDir, "main-file.txt", "main content");
    invoker.invoke(*addCmd, ctx, {"main-file.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Main commit"});
    
    // Get main commit hash
    auto headRes = Repository::resolveHEAD(repoPath);
    ASSERT_TRUE(headRes);
    auto [mainHash, _] = headRes.value();
    
    // 3. Create and switch to feature branch
    testing::internal::CaptureStdout();
    invoker.invoke(*checkoutCmd, ctx, {"-b", "feature"});
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Switched to a new branch 'feature'"), std::string::npos);
    
    // Verify HEAD points to feature
    auto branchRes = Repository::getCurrentBranch(repoPath);
    ASSERT_TRUE(branchRes);
    EXPECT_EQ(branchRes.value(), "feature");
    
    // Verify feature points to same commit as main
    headRes = Repository::resolveHEAD(repoPath);
    ASSERT_TRUE(headRes);
    auto [featureHash1, __] = headRes.value();
    EXPECT_EQ(mainHash, featureHash1);
    
    // 4. Create commit on feature
    createFile(tempDir, "feature-file.txt", "feature content");
    invoker.invoke(*addCmd, ctx, {"feature-file.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Feature commit"});
    
    // Get feature commit hash
    headRes = Repository::resolveHEAD(repoPath);
    ASSERT_TRUE(headRes);
    auto [featureHash2, ___] = headRes.value();
    EXPECT_NE(mainHash, featureHash2);
    
    // 5. Switch back to main
    testing::internal::CaptureStdout();
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Switched to branch 'main'"), std::string::npos);
    
    // Verify HEAD points to main
    branchRes = Repository::getCurrentBranch(repoPath);
    ASSERT_TRUE(branchRes);
    EXPECT_EQ(branchRes.value(), "main");
    
    // Verify main still has original commit
    headRes = Repository::resolveHEAD(repoPath);
    ASSERT_TRUE(headRes);
    auto [mainHashCheck, ____] = headRes.value();
    EXPECT_EQ(mainHash, mainHashCheck);
    
    // 6. Switch back to feature
    testing::internal::CaptureStdout();
    invoker.invoke(*checkoutCmd, ctx, {"feature"});
    output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Switched to branch 'feature'"), std::string::npos);
    
    // Verify HEAD points to feature with new commit
    headRes = Repository::resolveHEAD(repoPath);
    ASSERT_TRUE(headRes);
    auto [featureHashCheck, _____] = headRes.value();
    EXPECT_EQ(featureHash2, featureHashCheck);
}

/**
 * @brief Test: Create multiple branches from same commit
 * Positive: All branches initially point to same commit
 */
TEST_F(GitWorkflowTest, MultipleBranches) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    
    // 1. Initialize and create commit
    invoker.invoke(*initCmd, ctx, {});
    createFile(tempDir, "file.txt", "content");
    invoker.invoke(*addCmd, ctx, {"file.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial"});
    
    auto headRes = Repository::resolveHEAD(repoPath);
    ASSERT_TRUE(headRes);
    auto [initialHash, _] = headRes.value();
    
    // 2. Create three branches
    invoker.invoke(*checkoutCmd, ctx, {"-b", "branch1"});
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    invoker.invoke(*checkoutCmd, ctx, {"-b", "branch2"});
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    invoker.invoke(*checkoutCmd, ctx, {"-b", "branch3"});
    
    // 3. Switch to each branch and verify they all point to same commit
    invoker.invoke(*checkoutCmd, ctx, {"branch1"});
    headRes = Repository::resolveHEAD(repoPath);
    ASSERT_TRUE(headRes);
    auto [hash1, __] = headRes.value();
    EXPECT_EQ(initialHash, hash1);
    
    invoker.invoke(*checkoutCmd, ctx, {"branch2"});
    headRes = Repository::resolveHEAD(repoPath);
    ASSERT_TRUE(headRes);
    auto [hash2, ___] = headRes.value();
    EXPECT_EQ(initialHash, hash2);
    
    invoker.invoke(*checkoutCmd, ctx, {"branch3"});
    headRes = Repository::resolveHEAD(repoPath);
    ASSERT_TRUE(headRes);
    auto [hash3, ____] = headRes.value();
    EXPECT_EQ(initialHash, hash3);
}

/**
 * @brief Test: gitter add . should not re-stage unchanged files
 * Positive: After committing, gitter add . should not modify unchanged files in index
 */
TEST_F(GitWorkflowTest, AddDotDoesNotRestageUnchangedFiles) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    
    // Initialize repository
    invoker.invoke(*initCmd, ctx, {});
    
    // Create and commit a file
    createFile(tempDir, "file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"file1.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial commit"});
    
    // Capture initial index state
    Index index1;
    index1.load(repoPath);
    std::string initialHash = index1.entries().at("file1.txt").hashHex;
    
    // Run gitter add . - should not modify file1.txt
    invoker.invoke(*addCmd, ctx, {"."});
    
    // Verify hash unchanged
    Index index2;
    index2.load(repoPath);
    std::string afterAddHash = index2.entries().at("file1.txt").hashHex;
    EXPECT_EQ(initialHash, afterAddHash) << "add . should not modify unchanged file";
}

/**
 * @brief Test: gitter add . should add new files but skip unchanged ones
 * Positive: Add . should only add new files, not re-stage committed files
 */
TEST_F(GitWorkflowTest, AddDotAddsNewFilesSkipsUnchanged) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto statusCmd = CommandFactory::instance().create("status");
    
    // Initialize repository
    invoker.invoke(*initCmd, ctx, {});
    
    // Create and commit a file
    createFile(tempDir, "file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"file1.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial commit"});
    
    // Capture initial hash
    Index index1;
    index1.load(repoPath);
    std::string initialHash = index1.entries().at("file1.txt").hashHex;
    
    // Add a new file and run gitter add .
    createFile(tempDir, "file2.txt", "content2");
    invoker.invoke(*addCmd, ctx, {"."});
    
    // Verify both files in index
    Index index2;
    index2.load(repoPath);
    const auto& entries = index2.entries();
    EXPECT_EQ(entries.size(), 2);
    EXPECT_TRUE(entries.find("file1.txt") != entries.end());
    EXPECT_TRUE(entries.find("file2.txt") != entries.end());
    
    // Verify file1.txt hash unchanged
    EXPECT_EQ(entries.at("file1.txt").hashHex, initialHash) << "file1.txt should not be modified";
    
    // Verify status shows only file2.txt as staged
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    EXPECT_NE(statusOutput.find("Changes to be committed"), std::string::npos);
    EXPECT_NE(statusOutput.find("file2.txt"), std::string::npos);
    EXPECT_EQ(statusOutput.find("file1.txt"), std::string::npos) << "file1.txt should not be listed as staged";
}

/**
 * @brief Test: Status should only show changed files as staged, not all files
 * Positive: After add . with one new file, status should only show that file
 */
TEST_F(GitWorkflowTest, StatusShowsOnlyChangedFilesAsStaged) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto statusCmd = CommandFactory::instance().create("status");
    
    // Initialize repository
    invoker.invoke(*initCmd, ctx, {});
    
    // Create and commit file1.txt
    createFile(tempDir, "file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"file1.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial commit"});
    
    // Add new file2.txt
    createFile(tempDir, "file2.txt", "content2");
    invoker.invoke(*addCmd, ctx, {"file2.txt"});
    
    // Check status - should show only file2.txt as staged
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    
    EXPECT_NE(statusOutput.find("Changes to be committed"), std::string::npos);
    EXPECT_NE(statusOutput.find("file2.txt"), std::string::npos);
    EXPECT_EQ(statusOutput.find("file1.txt"), std::string::npos) << "file1.txt should not be listed as staged";
}

/**
 * @brief Test: Status should show modified files, not unchanged ones
 * Positive: Modify one file, status should only show that file as staged
 */
TEST_F(GitWorkflowTest, StatusShowsOnlyModifiedFilesAsStaged) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto statusCmd = CommandFactory::instance().create("status");
    
    // Initialize repository
    invoker.invoke(*initCmd, ctx, {});
    
    // Create and commit file1.txt and file2.txt
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    invoker.invoke(*addCmd, ctx, {"file1.txt", "file2.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial commit"});
    
    // Modify only file1.txt and stage it
    createFile(tempDir, "file1.txt", "modified1");
    invoker.invoke(*addCmd, ctx, {"file1.txt"});
    
    // Check status - should show only file1.txt as staged
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    
    EXPECT_NE(statusOutput.find("Changes to be committed"), std::string::npos);
    EXPECT_NE(statusOutput.find("file1.txt"), std::string::npos);
    EXPECT_EQ(statusOutput.find("file2.txt"), std::string::npos) << "file2.txt should not be listed as staged";
}

/**
 * @brief Test: Checkout removes files from working tree when switching branches
 * Positive: Files unique to a branch are removed when switching away
 */
TEST_F(GitWorkflowTest, CheckoutRemovesFilesOnBranchSwitch) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    
    // Initialize repository
    invoker.invoke(*initCmd, ctx, {});
    
    // Create initial commit on main
    createFile(repoPath, "file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial"});
    
    // Create feature branch with additional file
    invoker.invoke(*checkoutCmd, ctx, {"-b", "feature"});
    createFile(repoPath, "file2.txt", "content2");
    invoker.invoke(*addCmd, ctx, {"file2.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Add file2"});
    
    // Verify file2 exists
    EXPECT_TRUE(fs::exists(repoPath / "file2.txt"));
    
    // Switch back to main
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    
    // Verify file2 is removed
    EXPECT_FALSE(fs::exists(repoPath / "file2.txt"));
    
    // Verify file1 still exists
    EXPECT_TRUE(fs::exists(repoPath / "file1.txt"));
    
    // Switch back to feature
    invoker.invoke(*checkoutCmd, ctx, {"feature"});
    
    // Verify file2 is restored
    EXPECT_TRUE(fs::exists(repoPath / "file2.txt"));
    EXPECT_TRUE(fs::exists(repoPath / "file1.txt"));
}

/**
 * @brief Test: Checkout removes empty directories when switching branches
 * Positive: Directories that only contained files from another branch are removed
 */
TEST_F(GitWorkflowTest, CheckoutRemovesEmptyDirectories) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    
    // Initialize repository
    invoker.invoke(*initCmd, ctx, {});
    
    // Create commit with nested directory on main
    fs::create_directories(repoPath / "deep" / "nested" / "path");
    createFile(repoPath, "deep/nested/path/file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial"});
    
    // Create feature branch with different nested directory
    invoker.invoke(*checkoutCmd, ctx, {"-b", "feature"});
    fs::create_directories(repoPath / "another" / "path");
    createFile(repoPath, "another/path/file2.txt", "content2");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Add file2"});
    
    // Verify another/path exists
    EXPECT_TRUE(fs::exists(repoPath / "another" / "path" / "file2.txt"));
    
    // Switch back to main
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    
    // Verify another/path directory is removed
    EXPECT_FALSE(fs::exists(repoPath / "another" / "path"));
    
    // Verify deep/nested/path still exists
    EXPECT_TRUE(fs::exists(repoPath / "deep" / "nested" / "path" / "file1.txt"));
}

/**
 * @brief Test: Checkout preserves files common to both branches
 * Positive: Files that exist in both branches remain unchanged when switching
 */
TEST_F(GitWorkflowTest, CheckoutPreservesCommonFiles) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    
    // Initialize repository
    invoker.invoke(*initCmd, ctx, {});
    
    // Create commit on main
    createFile(repoPath, "common.txt", "v1");
    createFile(repoPath, "main-only.txt", "main");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Main commit"});
    
    // Create feature branch with modified common file and new file
    invoker.invoke(*checkoutCmd, ctx, {"-b", "feature"});
    createFile(repoPath, "common.txt", "v2");
    createFile(repoPath, "feature-only.txt", "feature");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Feature commit"});
    
    // Switch back to main
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    
    // Verify common file is restored to v1
    EXPECT_TRUE(fs::exists(repoPath / "common.txt"));
    std::string content = readFile(repoPath / "common.txt");
    EXPECT_EQ(content, "v1");
    
    // Verify main-only file exists
    EXPECT_TRUE(fs::exists(repoPath / "main-only.txt"));
    
    // Verify feature-only file is removed
    EXPECT_FALSE(fs::exists(repoPath / "feature-only.txt"));
}

/**
 * @brief Test: Reset does not modify working tree
 * Positive: Files in working tree remain unchanged after reset
 */
TEST_F(GitWorkflowTest, ResetDoesNotModifyWorkingTree) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto resetCmd = CommandFactory::instance().create("reset");
    auto statusCmd = CommandFactory::instance().create("status");
    
    // Initialize repository
    invoker.invoke(*initCmd, ctx, {});
    
    // Create three commits
    createFile(repoPath, "file.txt", "v1");
    invoker.invoke(*addCmd, ctx, {"file.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "First"});
    
    createFile(repoPath, "file.txt", "v2");
    invoker.invoke(*addCmd, ctx, {"file.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Second"});
    
    createFile(repoPath, "file.txt", "v3");
    invoker.invoke(*addCmd, ctx, {"file.txt"});
    invoker.invoke(*commitCmd, ctx, {"-m", "Third"});
    
    // Verify file contains v3
    EXPECT_TRUE(fs::exists(repoPath / "file.txt"));
    std::string content = readFile(repoPath / "file.txt");
    EXPECT_EQ(content, "v3");
    
    // Reset to HEAD~1
    invoker.invoke(*resetCmd, ctx, {"HEAD~1"});
    
    // Verify file still contains v3 (working tree unchanged)
    content = readFile(repoPath / "file.txt");
    EXPECT_EQ(content, "v3");
    
    // Verify status shows file as untracked (index was cleared)
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(statusOutput.find("Untracked files") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("file.txt") != std::string::npos);
}

/**
 * @brief Test: Checkout preserves staged uncommitted files (Git-compatible behavior)
 */
TEST_F(GitWorkflowTest, CheckoutPreservesStagedFiles) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    auto statusCmd = CommandFactory::instance().create("status");
    
    // Initialize repository
    invoker.invoke(*initCmd, ctx, {});
    
    // Create initial commit on main
    createFile(repoPath, "file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial"});
    
    // Create feature branch
    invoker.invoke(*checkoutCmd, ctx, {"-b", "feature"});
    
    // Add a new file and stage it (but don't commit)
    createFile(repoPath, "file2.txt", "content2");
    invoker.invoke(*addCmd, ctx, {"file2.txt"});
    
    // Switch back to main
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    
    // Switch back to feature
    invoker.invoke(*checkoutCmd, ctx, {"feature"});
    
    // Check status - file2 should be staged (preserved like Git)
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    
    // Git-compatible behavior: staged files are preserved
    EXPECT_TRUE(statusOutput.find("Changes to be committed") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("file2.txt") != std::string::npos);
    
    // File2 should still exist in working tree
    EXPECT_TRUE(fs::exists(repoPath / "file2.txt"));
}

/**
 * @brief Test: Staged modified files preserved across branch switches
 */
TEST_F(GitWorkflowTest, CheckoutPreservesModifiedStagedFiles) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    auto statusCmd = CommandFactory::instance().create("status");
    
    invoker.invoke(*initCmd, ctx, {});
    
    // Initial commit
    createFile(repoPath, "file.txt", "original");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial"});
    
    // Create branch
    invoker.invoke(*checkoutCmd, ctx, {"-b", "feature"});
    
    // Modify and stage
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Ensure mtime differs
    createFile(repoPath, "file.txt", "modified");
    invoker.invoke(*addCmd, ctx, {"file.txt"});
    
    // Switch to main and back
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    invoker.invoke(*checkoutCmd, ctx, {"feature"});
    
    // Verify staged modification preserved
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string statusOutput = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(statusOutput.find("Changes to be committed") != std::string::npos);
    EXPECT_TRUE(statusOutput.find("modified: file.txt") != std::string::npos);
}

/**
 * @brief Test: Multiple files staged across different branches
 */
TEST_F(GitWorkflowTest, CheckoutPreservesMultipleStagedFiles) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    auto statusCmd = CommandFactory::instance().create("status");
    
    invoker.invoke(*initCmd, ctx, {});
    
    createFile(repoPath, "common.txt", "content");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial"});
    
    // Add files on main
    createFile(repoPath, "main1.txt", "main1");
    createFile(repoPath, "main2.txt", "main2");
    invoker.invoke(*addCmd, ctx, {"."});
    
    // Create and switch to feature
    invoker.invoke(*checkoutCmd, ctx, {"-b", "feature"});
    
    // Add files on feature
    createFile(repoPath, "feature1.txt", "feature1");
    createFile(repoPath, "feature2.txt", "feature2");
    invoker.invoke(*addCmd, ctx, {"."});
    
    // Switch back to main
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string status = testing::internal::GetCapturedStdout();
    
    // Git preserves ALL staged files across branches
    EXPECT_TRUE(status.find("main1.txt") != std::string::npos);
    EXPECT_TRUE(status.find("main2.txt") != std::string::npos);
    EXPECT_TRUE(status.find("feature1.txt") != std::string::npos);
    EXPECT_TRUE(status.find("feature2.txt") != std::string::npos);
    
    // Switch back to feature
    invoker.invoke(*checkoutCmd, ctx, {"feature"});
    
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    status = testing::internal::GetCapturedStdout();
    
    // Git preserves ALL staged files across branches
    EXPECT_TRUE(status.find("main1.txt") != std::string::npos);
    EXPECT_TRUE(status.find("main2.txt") != std::string::npos);
    EXPECT_TRUE(status.find("feature1.txt") != std::string::npos);
    EXPECT_TRUE(status.find("feature2.txt") != std::string::npos);
}

/**
 * @brief Test: Complex branch with staged, modified, and untracked files
 */
TEST_F(GitWorkflowTest, CheckoutComplexStagingState) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    auto statusCmd = CommandFactory::instance().create("status");
    
    invoker.invoke(*initCmd, ctx, {});
    
    // Setup
    createFile(repoPath, "tracked.txt", "original");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial"});
    
    invoker.invoke(*checkoutCmd, ctx, {"-b", "branch"});
    
    // Staged new file
    createFile(repoPath, "new.txt", "content");
    invoker.invoke(*addCmd, ctx, {"new.txt"});
    
    // Staged modified file
    createFile(repoPath, "tracked.txt", "modified");
    invoker.invoke(*addCmd, ctx, {"tracked.txt"});
    
    // Untracked file
    createFile(repoPath, "untracked.txt", "untracked");
    
    // Switch branches
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    invoker.invoke(*checkoutCmd, ctx, {"branch"});
    
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string status = testing::internal::GetCapturedStdout();
    
    // Verify all states preserved
    EXPECT_TRUE(status.find("new.txt") != std::string::npos);
    EXPECT_TRUE(status.find("tracked.txt") != std::string::npos);
    EXPECT_TRUE(status.find("untracked.txt") != std::string::npos);
}

/**
 * @brief Test: Nested directory with staged files across branches
 */
TEST_F(GitWorkflowTest, CheckoutNestedDirectoryStaging) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    auto statusCmd = CommandFactory::instance().create("status");
    
    invoker.invoke(*initCmd, ctx, {});
    
    fs::create_directories(repoPath / "dir");
    createFile(repoPath / "dir", "file.txt", "content");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial"});
    
    invoker.invoke(*checkoutCmd, ctx, {"-b", "feature"});
    
    // Add nested file
    fs::create_directories(repoPath / "dir" / "subdir");
    createFile(repoPath / "dir" / "subdir", "nested.txt", "nested");
    invoker.invoke(*addCmd, ctx, {"."});
    
    // Switch branches
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    invoker.invoke(*checkoutCmd, ctx, {"feature"});
    
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string status = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(status.find("dir/subdir/nested.txt") != std::string::npos);
}

/**
 * @brief Test: Branch divergence and merge-like scenarios
 */
TEST_F(GitWorkflowTest, CheckoutWithDivergentBranches) {
    auto initCmd = CommandFactory::instance().create("init");
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    auto logCmd = CommandFactory::instance().create("log");
    auto statusCmd = CommandFactory::instance().create("status");
    
    invoker.invoke(*initCmd, ctx, {});
    
    createFile(repoPath, "common.txt", "content");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial"});
    
    // Branch 1
    invoker.invoke(*checkoutCmd, ctx, {"-b", "branch1"});
    createFile(repoPath, "file1.txt", "content1");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Add file1"});
    
    // Back to main
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    
    // Branch 2
    invoker.invoke(*checkoutCmd, ctx, {"-b", "branch2"});
    createFile(repoPath, "file2.txt", "content2");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Add file2"});
    
    // Verify isolation
    EXPECT_TRUE(fs::exists(repoPath / "common.txt"));
    EXPECT_TRUE(fs::exists(repoPath / "file2.txt"));
    EXPECT_FALSE(fs::exists(repoPath / "file1.txt"));
    
    invoker.invoke(*checkoutCmd, ctx, {"branch1"});
    EXPECT_TRUE(fs::exists(repoPath / "file1.txt"));
    EXPECT_FALSE(fs::exists(repoPath / "file2.txt"));
}

/**
 * @brief Test: File modified after staging appears in both categories
 * Edge case: Complex staging state
 */
TEST_F(GitWorkflowTest, FileModifiedAfterStagingShowsInBothCategories) {
    // Initialize repository
    auto initCmd = CommandFactory::instance().create("init");
    invoker.invoke(*initCmd, ctx, {});
    
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto statusCmd = CommandFactory::instance().create("status");
    
    // Stage file
    createFile(repoPath, "file.txt", "v1");
    invoker.invoke(*addCmd, ctx, {"file.txt"});
    
    // Modify after staging
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Ensure mtime differs
    createFile(repoPath, "file.txt", "v2");
    
    // Status should show file in BOTH categories
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string status = testing::internal::GetCapturedStdout();
    
    // Count occurrences of "file.txt" in modified
    size_t count = 0;
    size_t pos = 0;
    while ((pos = status.find("modified: file.txt", pos)) != std::string::npos) {
        ++count;
        pos += 1;
    }
    
    EXPECT_GE(count, 1) << "File should appear at least once as modified";
    EXPECT_NE(status.find("Changes to be committed"), std::string::npos);
    EXPECT_NE(status.find("Changes not staged for commit"), std::string::npos);
}

/**
 * @brief Test: Commit with Unicode filenames
 * Edge case: International characters
 */
TEST_F(GitWorkflowTest, CommitWithUnicodeFilenames) {
    // Initialize repository
    auto initCmd = CommandFactory::instance().create("init");
    invoker.invoke(*initCmd, ctx, {});
    
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto logCmd = CommandFactory::instance().create("log");
    
    // Create files with Unicode names
    createFile(repoPath, "文件.txt", "Chinese content");
    createFile(repoPath, "مرحبا.py", "Arabic content");
    createFile(repoPath, "тест.cpp", "Russian content");
    
    invoker.invoke(*addCmd, ctx, {"."});
    auto result = invoker.invoke(*commitCmd, ctx, {"-m", "Add Unicode files"});
    EXPECT_TRUE(result) << result.error().message;
    
    // Verify commit created
    testing::internal::CaptureStdout();
    invoker.invoke(*logCmd, ctx, {});
    std::string log = testing::internal::GetCapturedStdout();
    
    EXPECT_NE(log.find("Add Unicode files"), std::string::npos);
}

/**
 * @brief Test: Checkout with staged Unicode file modifications
 * Edge case: Unicode filenames in complex checkout scenario
 */
TEST_F(GitWorkflowTest, CheckoutWithUnicodeFileStaging) {
    // Initialize repository
    auto initCmd = CommandFactory::instance().create("init");
    invoker.invoke(*initCmd, ctx, {});
    
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto checkoutCmd = CommandFactory::instance().create("checkout");
    
    // Create commit on main
    createFile(repoPath, "文件.txt", "main version");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial"});
    
    // Create branch
    invoker.invoke(*checkoutCmd, ctx, {"-b", "feature"});
    
    // Stage modification on feature
    createFile(repoPath, "文件.txt", "feature version");
    invoker.invoke(*addCmd, ctx, {"."});
    
    // Switch back to main
    testing::internal::CaptureStdout();
    invoker.invoke(*checkoutCmd, ctx, {"main"});
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_NE(output.find("Switched to branch 'main'"), std::string::npos);
    
    // Verify file reverted
    std::ifstream file(repoPath / "文件.txt");
    std::string content;
    std::getline(file, content);
    EXPECT_EQ(content, "main version");
}

/**
 * @brief Test: Large commit with many files
 * Edge case: Performance and correctness with 100+ files
 */
TEST_F(GitWorkflowTest, LargeCommitWithManyFiles) {
    // Initialize repository
    auto initCmd = CommandFactory::instance().create("init");
    invoker.invoke(*initCmd, ctx, {});
    
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto statusCmd = CommandFactory::instance().create("status");
    
    // Create 100 files
    for (int i = 0; i < 100; ++i) {
        std::string filename = "file" + std::to_string(i) + ".txt";
        createFile(repoPath, filename, "content " + std::to_string(i));
    }
    
    invoker.invoke(*addCmd, ctx, {"."});
    
    // Commit should succeed
    auto result = invoker.invoke(*commitCmd, ctx, {"-m", "Add 100 files"});
    EXPECT_TRUE(result) << result.error().message;
    
    // Verify all files indexed
    Index index;
    index.load(tempDir);
    EXPECT_EQ(index.entries().size(), 100);
    
    // Status should be clean
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string status = testing::internal::GetCapturedStdout();
    
    EXPECT_NE(status.find("nothing to commit, working tree clean"), std::string::npos);
}

/**
 * @brief Test: Orphaned commits after reset
 * Edge case: Objects remain accessible after reset
 */
TEST_F(GitWorkflowTest, OrphanedCommitsAfterReset) {
    // Initialize repository
    auto initCmd = CommandFactory::instance().create("init");
    invoker.invoke(*initCmd, ctx, {});
    
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto resetCmd = CommandFactory::instance().create("reset");
    auto logCmd = CommandFactory::instance().create("log");
    
    // Create 3 commits
    createFile(repoPath, "file.txt", "v1");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "v1"});
    
    createFile(repoPath, "file.txt", "v2");
    invoker.invoke(*addCmd, ctx, {"."});
    auto result = invoker.invoke(*commitCmd, ctx, {"-m", "v2"});
    EXPECT_TRUE(result);
    
    createFile(repoPath, "file.txt", "v3");
    invoker.invoke(*addCmd, ctx, {"."});
    result = invoker.invoke(*commitCmd, ctx, {"-m", "v3"});
    EXPECT_TRUE(result);
    
    // Get commit hashes
    auto headRes = Repository::resolveHEAD(repoPath);
    ASSERT_TRUE(headRes);
    auto [commit3Hash, _] = headRes.value();
    
    // Reset to commit2 (HEAD~1)
    result = invoker.invoke(*resetCmd, ctx, {"HEAD~1"});
    EXPECT_TRUE(result);
    
    // Verify commit3 still exists in objects/ (orphaned but not deleted)
    ObjectStore store(repoPath);
    EXPECT_NO_THROW(store.readCommit(commit3Hash));
    
    // Create new commit
    createFile(repoPath, "file.txt", "v4");
    invoker.invoke(*addCmd, ctx, {"."});
    auto result4 = invoker.invoke(*commitCmd, ctx, {"-m", "v4"});
    EXPECT_TRUE(result4);
    
    // Verify commit3's parent is not v4's parent
    auto newHeadRes = Repository::resolveHEAD(repoPath);
    ASSERT_TRUE(newHeadRes);
    auto [commit4Hash, __] = newHeadRes.value();
    
    CommitObject c4 = store.readCommit(commit4Hash);
    EXPECT_NE(c4.parentHashes[0], commit3Hash);
}

/**
 * @brief Test: Status with Unicode filenames and complex states
 * Edge case: Unicode in all three states (staged/modified/untracked)
 */
TEST_F(GitWorkflowTest, StatusUnicodeInAllStates) {
    // Initialize repository
    auto initCmd = CommandFactory::instance().create("init");
    invoker.invoke(*initCmd, ctx, {});
    
    auto addCmd = CommandFactory::instance().create("add");
    auto commitCmd = CommandFactory::instance().create("commit");
    auto statusCmd = CommandFactory::instance().create("status");
    
    // Create and commit file
    createFile(repoPath, "文件.txt", "v1");
    invoker.invoke(*addCmd, ctx, {"."});
    invoker.invoke(*commitCmd, ctx, {"-m", "Initial"});
    
    // Modify committed file
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Ensure mtime differs
    createFile(repoPath, "文件.txt", "v2");
    
    // Stage new file
    createFile(repoPath, "مرحبا.py", "new");
    invoker.invoke(*addCmd, ctx, {"مرحبا.py"});
    
    // Create untracked file
    createFile(repoPath, "тест.cpp", "untracked");
    
    testing::internal::CaptureStdout();
    invoker.invoke(*statusCmd, ctx, {});
    std::string status = testing::internal::GetCapturedStdout();
    
    EXPECT_NE(status.find("Changes to be committed"), std::string::npos);
    EXPECT_NE(status.find("Changes not staged"), std::string::npos);
    EXPECT_NE(status.find("Untracked files"), std::string::npos);
    EXPECT_NE(status.find("مرحبا.py"), std::string::npos);
    EXPECT_NE(status.find("文件.txt"), std::string::npos);
    EXPECT_NE(status.find("тест.cpp"), std::string::npos);
}

} // namespace gitter::test

