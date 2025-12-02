#include <gtest/gtest.h>
#include <filesystem>
#include <sstream>

#include "test_utils.hpp"
#include "cli/commands/DiffCommand.hpp"
#include "cli/commands/AddCommand.hpp"
#include "cli/commands/CommitCommand.hpp"
#include "core/Repository.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class DiffCommandTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = createTempDir();
        originalCwd = getCwd();
        setCwd(tempDir);

        initTestRepo(tempDir);

        oldCout = std::cout.rdbuf();
        outputStream = std::stringstream();
        std::cout.rdbuf(outputStream.rdbuf());
    }

    void TearDown() override {
        std::cout.rdbuf(oldCout);
        setCwd(originalCwd);
        removeDir(tempDir);
    }

    std::string getOutput() const {
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
    std::streambuf* oldCout{};
};

// No differences: clean working tree and index
TEST_F(DiffCommandTest, NoDifferencesProducesNoOutput) {
    DiffCommand cmd;
    std::vector<std::string> args;

    auto res = cmd.execute(ctx, args);
    ASSERT_TRUE(res) << res.error().message;

    EXPECT_TRUE(getOutput().empty());
}

// Working tree vs index: modified file
TEST_F(DiffCommandTest, ShowsUnstagedChangesWorkingTreeVsIndex) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    DiffCommand diffCmd;

    // Create, add, commit
    createFile(tempDir, "file.txt", "v1\n");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});

    // Modify working tree without staging
    createFile(tempDir, "file.txt", "v1\nv2\n");

    clearOutput();
    auto res = diffCmd.execute(ctx, {});
    ASSERT_TRUE(res) << res.error().message;

    auto out = getOutput();
    // Basic unified diff markers (header + addition)
    EXPECT_NE(out.find("diff --git a/file.txt b/file.txt"), std::string::npos);
    EXPECT_NE(out.find("+v2"), std::string::npos);
}

// Working tree vs index: deleted file
TEST_F(DiffCommandTest, ShowsDeletedFilesWorkingTreeVsIndex) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    DiffCommand diffCmd;

    // Create, add, commit
    fs::path p = createFile(tempDir, "dead.txt", "gone\n");
    addCmd.execute(ctx, {"dead.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});

    // Delete from working tree
    fs::remove(p);

    clearOutput();
    auto res = diffCmd.execute(ctx, {});
    ASSERT_TRUE(res) << res.error().message;

    auto out = getOutput();
    EXPECT_NE(out.find("diff --git a/dead.txt b/dead.txt"), std::string::npos);
    EXPECT_NE(out.find("-gone"), std::string::npos);
}

// Index vs HEAD: staged changes
TEST_F(DiffCommandTest, ShowsStagedChangesIndexVsHeadWithCached) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    DiffCommand diffCmd;

    // Initial commit
    createFile(tempDir, "file.txt", "one\n");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});

    // Modify and stage
    createFile(tempDir, "file.txt", "one\n two\n");
    addCmd.execute(ctx, {"file.txt"});

    clearOutput();
    auto res = diffCmd.execute(ctx, {"--cached"});
    ASSERT_TRUE(res) << res.error().message;

    auto out = getOutput();
    EXPECT_NE(out.find("diff --git a/file.txt b/file.txt"), std::string::npos);
    EXPECT_NE(out.find("+ two"), std::string::npos);
}

// Unknown args (commit-like) are rejected for now
TEST_F(DiffCommandTest, RejectsCommitArgumentsForNow) {
    DiffCommand diffCmd;

    clearOutput();
    auto res = diffCmd.execute(ctx, {"HEAD"});
    EXPECT_FALSE(res);
    EXPECT_EQ(res.error().code, ErrorCode::InvalidArgs);
}


