#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include "test_utils.hpp"
#include "cli/commands/StatusCommand.hpp"
#include "cli/commands/AddCommand.hpp"
#include "cli/commands/CommitCommand.hpp"
#include "core/Repository.hpp"
#include "core/Index.hpp"
#include "core/ObjectStore.hpp"
#include "util/Sha1Hasher.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class StatusCommandTest : public ::testing::Test {
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

// Test: Status on empty repository
TEST_F(StatusCommandTest, StatusEmptyRepository) {
    StatusCommand cmd;
    std::vector<std::string> args;
    
    auto result = cmd.execute(ctx, args);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    
    std::string output = getOutput();
    // Should show nothing to commit or clean
    EXPECT_NE(output.find("nothing"), std::string::npos);
}

// Test: Status with staged files (no commits)
TEST_F(StatusCommandTest, StatusWithStagedFiles) {
    AddCommand addCmd;
    StatusCommand statusCmd;
    
    createFile(tempDir, "file1.txt", "content1");
    createFile(tempDir, "file2.txt", "content2");
    addCmd.execute(ctx, {"file1.txt", "file2.txt"});
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("Changes to be committed"), std::string::npos);
    EXPECT_NE(output.find("file1.txt"), std::string::npos);
    EXPECT_NE(output.find("file2.txt"), std::string::npos);
}

// Test: Status after commit (should be clean)
TEST_F(StatusCommandTest, StatusAfterCommit) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    StatusCommand statusCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial commit"});
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("nothing to commit, working tree clean"), std::string::npos);
    EXPECT_EQ(output.find("Changes to be committed"), std::string::npos);
}

// Test: Status with modified files
TEST_F(StatusCommandTest, StatusWithModifiedFiles) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    StatusCommand statusCmd;
    
    // Create, add, commit
    createFile(tempDir, "file.txt", "content1");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    // Modify file - add small delay to ensure mtime updates (filesystem granularity)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    createFile(tempDir, "file.txt", "content2");
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("Changes not staged for commit"), std::string::npos);
    EXPECT_NE(output.find("modified: file.txt"), std::string::npos);
}

// Test: Status with deleted files
TEST_F(StatusCommandTest, StatusWithDeletedFiles) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    StatusCommand statusCmd;
    
    // Create, add, commit
    fs::path filePath = createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    // Delete file
    fs::remove(filePath);
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("Changes not staged for commit"), std::string::npos);
    EXPECT_NE(output.find("deleted:  file.txt"), std::string::npos); // StatusCommand uses two spaces for alignment
}

// Test: Status with untracked files
TEST_F(StatusCommandTest, StatusWithUntrackedFiles) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    StatusCommand statusCmd;
    
    // Create, add, commit
    createFile(tempDir, "file1.txt", "content1");
    addCmd.execute(ctx, {"file1.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    // Create untracked file
    createFile(tempDir, "file2.txt", "content2");
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("Untracked files"), std::string::npos);
    EXPECT_NE(output.find("file2.txt"), std::string::npos);
}

// Test: Status with staged and modified
TEST_F(StatusCommandTest, StatusStagedAndModified) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    StatusCommand statusCmd;
    
    // Create, add, commit
    createFile(tempDir, "file.txt", "content1");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    // Modify and stage
    createFile(tempDir, "file.txt", "content2");
    addCmd.execute(ctx, {"file.txt"});
    
    // Modify again (after staging)
    createFile(tempDir, "file.txt", "content3");
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Should show both staged and modified
    EXPECT_NE(output.find("Changes to be committed"), std::string::npos);
    EXPECT_NE(output.find("Changes not staged for commit"), std::string::npos);
    EXPECT_NE(output.find("modified: file.txt"), std::string::npos);
}

// Test: Status with multiple categories
TEST_F(StatusCommandTest, StatusMultipleCategories) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    StatusCommand statusCmd;
    
    // Initial commit
    createFile(tempDir, "tracked.txt", "content");
    addCmd.execute(ctx, {"tracked.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    // Stage new file
    createFile(tempDir, "new.txt", "new");
    addCmd.execute(ctx, {"new.txt"});
    
    // Modify tracked file
    createFile(tempDir, "tracked.txt", "modified");
    
    // Create untracked file
    createFile(tempDir, "untracked.txt", "untracked");
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Should show all three categories
    EXPECT_NE(output.find("Changes to be committed"), std::string::npos);
    EXPECT_NE(output.find("Changes not staged for commit"), std::string::npos);
    EXPECT_NE(output.find("Untracked files"), std::string::npos);
}

// Test: Status with subdirectories
TEST_F(StatusCommandTest, StatusWithSubdirectories) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    StatusCommand statusCmd;
    
    // Create structure and commit
    fs::create_directories(tempDir / "src");
    createFile(tempDir, "src/main.cpp", "int main() {}");
    addCmd.execute(ctx, {"."});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    // Modify file
    createFile(tempDir, "src/main.cpp", "int main() { return 0; }");
    
    // Add new file
    createFile(tempDir, "src/util.cpp", "void util() {}");
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("modified: src/main.cpp"), std::string::npos);
    EXPECT_NE(output.find("src/util.cpp"), std::string::npos); // Untracked
}

// Test: Status after add (but before commit)
TEST_F(StatusCommandTest, StatusAfterAddBeforeCommit) {
    AddCommand addCmd;
    StatusCommand statusCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("Changes to be committed"), std::string::npos);
}

// Test: Status after commit clears staged area
TEST_F(StatusCommandTest, StatusAfterCommitClearsStaged) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    StatusCommand statusCmd;
    
    createFile(tempDir, "file.txt", "content");
    addCmd.execute(ctx, {"file.txt"});
    commitCmd.execute(ctx, {"-m", "Commit"});
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Should not show "Changes to be committed"
    EXPECT_EQ(output.find("Changes to be committed"), std::string::npos);
}

// Test: Status with empty file
TEST_F(StatusCommandTest, StatusWithEmptyFile) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    StatusCommand statusCmd;
    
    createFile(tempDir, "empty.txt", "");
    addCmd.execute(ctx, {"empty.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    // Modify empty file
    createFile(tempDir, "empty.txt", "now has content");
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("modified: empty.txt"), std::string::npos);
}

// Test: Status with binary file (test that it works)
TEST_F(StatusCommandTest, StatusWithBinaryFile) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    StatusCommand statusCmd;
    
    // Create binary file
    fs::path binFile = tempDir / "binary.bin";
    std::ofstream bin(binFile, std::ios::binary);
    bin.write("\x00\x01\x02\x03\xff\xfe\xfd", 7);
    bin.close();
    
    addCmd.execute(ctx, {"binary.bin"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    // Modify binary
    std::ofstream bin2(binFile, std::ios::binary);
    bin2.write("\x00\x01\x02\x03\xff\xfe\xfc", 7);
    bin2.close();
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    EXPECT_NE(output.find("modified: binary.bin"), std::string::npos);
}

// Test: Status with combination of all states
TEST_F(StatusCommandTest, StatusAllStatesCombined) {
    AddCommand addCmd;
    CommitCommand commitCmd;
    StatusCommand statusCmd;
    
    // Initial state
    createFile(tempDir, "tracked1.txt", "content1");
    addCmd.execute(ctx, {"tracked1.txt"});
    commitCmd.execute(ctx, {"-m", "Initial"});
    
    // Create various states
    createFile(tempDir, "tracked1.txt", "modified1"); // Modified
    createFile(tempDir, "tracked2.txt", "content2"); // New tracked (will add)
    addCmd.execute(ctx, {"tracked2.txt"}); // Staged
    createFile(tempDir, "tracked2.txt", "modified2"); // Modified after staging
    createFile(tempDir, "untracked.txt", "untracked"); // Untracked
    fs::remove(tempDir / "tracked1.txt"); // Deleted
    
    clearOutput();
    statusCmd.execute(ctx, {});
    
    std::string output = getOutput();
    // Verify all states shown
    EXPECT_NE(output.find("Changes to be committed"), std::string::npos);
    EXPECT_NE(output.find("Changes not staged for commit"), std::string::npos);
    EXPECT_NE(output.find("Untracked files"), std::string::npos);
    EXPECT_NE(output.find("deleted:  tracked1.txt"), std::string::npos); // StatusCommand uses two spaces
    EXPECT_NE(output.find("modified: tracked2.txt"), std::string::npos);
}

