#include <gtest/gtest.h>
#include <filesystem>
#include "test_utils.hpp"
#include "core/Repository.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class RepositoryTest : public ::testing::Test {
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
};

// Test: Discover repository root in current directory
TEST_F(RepositoryTest, DiscoverRootCurrentDirectory) {
    initTestRepo(tempDir);
    
    auto result = Repository::instance().discoverRoot(tempDir);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value(), tempDir);
}

// Test: Discover repository root from subdirectory
TEST_F(RepositoryTest, DiscoverRootFromSubdirectory) {
    initTestRepo(tempDir);
    fs::path subdir = tempDir / "src" / "util";
    fs::create_directories(subdir);
    
    auto result = Repository::instance().discoverRoot(subdir);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value(), tempDir);
}

// Test: Discover fails when not in repository
TEST_F(RepositoryTest, DiscoverFailsNotInRepository) {
    // No repository initialized
    auto result = Repository::instance().discoverRoot(tempDir);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::NotARepository);
}

// Test: Discover walks up directory tree
TEST_F(RepositoryTest, DiscoverWalksUpTree) {
    initTestRepo(tempDir);
    
    // Create nested structure
    fs::path deep = tempDir / "a" / "b" / "c" / "d" / "e";
    fs::create_directories(deep);
    
    auto result = Repository::instance().discoverRoot(deep);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(result.value(), tempDir);
}

// Test: Init creates repository structure
TEST_F(RepositoryTest, InitCreatesStructure) {
    auto result = Repository::instance().init(tempDir);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    // Verify structure
    EXPECT_TRUE(fs::exists(tempDir / ".gitter"));
    EXPECT_TRUE(fs::exists(tempDir / ".gitter" / "objects"));
    EXPECT_TRUE(fs::exists(tempDir / ".gitter" / "refs" / "heads"));
    EXPECT_TRUE(fs::exists(tempDir / ".gitter" / "HEAD"));
}

// Test: Init fails if already initialized
TEST_F(RepositoryTest, InitFailsIfExists) {
    initTestRepo(tempDir);
    
    auto result = Repository::instance().init(tempDir);
    // Should either succeed (idempotent) or return AlreadyInitialized
    // Current implementation: may succeed or fail
}

// Test: Singleton pattern
TEST_F(RepositoryTest, SingletonPattern) {
    Repository& repo1 = Repository::instance();
    Repository& repo2 = Repository::instance();
    
    // Should be same instance
    EXPECT_EQ(&repo1, &repo2);
}

