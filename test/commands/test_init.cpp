#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "test_utils.hpp"
#include "cli/commands/InitCommand.hpp"
#include "core/Repository.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class InitCommandTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = createTempDir();
        originalCwd = getCwd();
        setCwd(tempDir);
    }
    
    void TearDown() override {
        setCwd(originalCwd);
        removeDir(tempDir);
    }
    
    fs::path tempDir;
    fs::path originalCwd;
    AppContext ctx;
};

// Test: Initialize repository in current directory
TEST_F(InitCommandTest, InitCurrentDirectory) {
    InitCommand cmd;
    std::vector<std::string> args;
    
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify .gitter directory structure
    EXPECT_TRUE(fs::exists(tempDir / ".gitter"));
    EXPECT_TRUE(fs::is_directory(tempDir / ".gitter"));
    EXPECT_TRUE(fs::exists(tempDir / ".gitter" / "objects"));
    EXPECT_TRUE(fs::is_directory(tempDir / ".gitter" / "objects"));
    EXPECT_TRUE(fs::exists(tempDir / ".gitter" / "refs" / "heads"));
    EXPECT_TRUE(fs::is_directory(tempDir / ".gitter" / "refs" / "heads"));
    EXPECT_TRUE(fs::exists(tempDir / ".gitter" / "HEAD"));
    
    // Verify HEAD file content
    std::ifstream headFile(tempDir / ".gitter" / "HEAD");
    std::string headContent;
    std::getline(headFile, headContent);
    EXPECT_EQ(headContent, "ref: refs/heads/main");
}

// Test: Initialize repository in specified directory
TEST_F(InitCommandTest, InitSpecifiedDirectory) {
    InitCommand cmd;
    fs::path targetDir = tempDir / "myproject";
    std::vector<std::string> args{targetDir.string()};
    
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify .gitter directory in specified path
    EXPECT_TRUE(fs::exists(targetDir / ".gitter"));
    EXPECT_TRUE(fs::is_directory(targetDir / ".gitter"));
    EXPECT_TRUE(fs::exists(targetDir / ".gitter" / "objects"));
    EXPECT_TRUE(fs::exists(targetDir / ".gitter" / "HEAD"));
}

// Test: Initialize in non-existent directory (should create it)
TEST_F(InitCommandTest, InitNonExistentDirectory) {
    InitCommand cmd;
    fs::path newDir = tempDir / "new" / "nested" / "project";
    std::vector<std::string> args{newDir.string()};
    
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    EXPECT_TRUE(fs::exists(newDir / ".gitter"));
    EXPECT_TRUE(fs::is_directory(newDir / ".gitter"));
}

// Test: Initialize in existing repository (should fail or handle gracefully)
TEST_F(InitCommandTest, InitExistingRepository) {
    InitCommand cmd1, cmd2;
    std::vector<std::string> args;
    
    // First init
    auto result1 = cmd1.execute(ctx, args);
    ASSERT_TRUE(result1.has_value()) << result1.error().message;
    
    // Second init in same directory
    auto result2 = cmd2.execute(ctx, args);
    // Should either succeed (idempotent) or return AlreadyInitialized error
    // Current implementation: checks and may return error
    // This test documents the behavior
}

// Test: Initialize with relative path
TEST_F(InitCommandTest, InitRelativePath) {
    InitCommand cmd;
    fs::path relativePath = "relative_repo";
    std::vector<std::string> args{relativePath.string()};
    
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    fs::path expectedPath = tempDir / relativePath;
    EXPECT_TRUE(fs::exists(expectedPath / ".gitter"));
}

// Test: Initialize with trailing slash in path
TEST_F(InitCommandTest, InitPathWithTrailingSlash) {
    InitCommand cmd;
    std::string pathWithSlash = (tempDir / "repo_with_slash").string() + "/";
    std::vector<std::string> args{pathWithSlash};
    
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    fs::path expectedPath = tempDir / "repo_with_slash";
    EXPECT_TRUE(fs::exists(expectedPath / ".gitter"));
}

// Test: Verify all required directories created
TEST_F(InitCommandTest, VerifyDirectoryStructure) {
    InitCommand cmd;
    std::vector<std::string> args;
    
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    fs::path gitterDir = tempDir / ".gitter";
    
    // Check all required subdirectories
    EXPECT_TRUE(fs::exists(gitterDir / "objects"));
    EXPECT_TRUE(fs::is_directory(gitterDir / "objects"));
    
    EXPECT_TRUE(fs::exists(gitterDir / "refs"));
    EXPECT_TRUE(fs::is_directory(gitterDir / "refs"));
    
    EXPECT_TRUE(fs::exists(gitterDir / "refs" / "heads"));
    EXPECT_TRUE(fs::is_directory(gitterDir / "refs" / "heads"));
}

// Test: Verify HEAD file points to main branch
TEST_F(InitCommandTest, VerifyHEADPointsToMain) {
    InitCommand cmd;
    std::vector<std::string> args;
    
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    fs::path headPath = tempDir / ".gitter" / "HEAD";
    ASSERT_TRUE(fs::exists(headPath));
    
    std::ifstream headFile(headPath);
    std::string headContent;
    std::getline(headFile, headContent);
    EXPECT_EQ(headContent, "ref: refs/heads/main");
}

// Test: Initialize multiple repositories in sequence
TEST_F(InitCommandTest, InitMultipleRepositories) {
    InitCommand cmd;
    
    for (int i = 0; i < 3; ++i) {
        fs::path repoPath = tempDir / ("repo" + std::to_string(i));
        std::vector<std::string> args{repoPath.string()};
        
        auto result = cmd.execute(ctx, args);
        ASSERT_TRUE(result.has_value()) << "Failed to init repo " << i;
        
        EXPECT_TRUE(fs::exists(repoPath / ".gitter"));
    }
}

// Test: Repository discovery after init
TEST_F(InitCommandTest, DiscoverRepositoryAfterInit) {
    InitCommand cmd;
    std::vector<std::string> args;
    
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Change to subdirectory
    fs::path subdir = tempDir / "subdir";
    fs::create_directories(subdir);
    setCwd(subdir);
    
    // Should be able to discover repository
    auto repoRoot = Repository::instance().discoverRoot(subdir);
    ASSERT_TRUE(repoRoot.has_value()) << "Failed to discover repository";
    EXPECT_EQ(repoRoot.value(), tempDir);
}

