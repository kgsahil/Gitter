#include <gtest/gtest.h>
#include <filesystem>
#include <vector>
#include <regex>
#include "test_utils.hpp"
#include "util/PatternMatcher.hpp"
#include "core/Index.hpp"

namespace fs = std::filesystem;

using namespace gitter;
using namespace gitter::test::utils;

class PatternMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = createTempDir();
        
        // Create test file structure
        fs::create_directories(tempDir / "src" / "util");
        createFile(tempDir, "file1.txt", "content1");
        createFile(tempDir, "file2.txt", "content2");
        createFile(tempDir, "file3.cpp", "content3");
        createFile(tempDir, "src/main.cpp", "int main() {}");
        createFile(tempDir, "src/util/helper.cpp", "void helper() {}");
        createFile(tempDir, "src/util/helper.h", "void helper();");
        
        // Normalized .gitter path to skip
        gitterDir = (tempDir / ".gitter").string();
    }
    
    void TearDown() override {
        removeDir(tempDir);
    }
    
    fs::path tempDir;
    std::string gitterDir;
};

// Test: Convert glob to regex
TEST_F(PatternMatcherTest, GlobToRegex) {
    // Test that regex works by matching strings
    std::regex regex1 = PatternMatcher::globToRegex("*.txt");
    EXPECT_TRUE(std::regex_match("file.txt", regex1));
    EXPECT_TRUE(std::regex_match("test.txt", regex1));
    EXPECT_FALSE(std::regex_match("file.cpp", regex1));
    
    // Question mark pattern
    std::regex regex2 = PatternMatcher::globToRegex("file?.txt");
    EXPECT_TRUE(std::regex_match("file1.txt", regex2));
    EXPECT_TRUE(std::regex_match("file2.txt", regex2));
    EXPECT_FALSE(std::regex_match("file10.txt", regex2)); // Too long
    
    // Directory pattern
    std::regex regex3 = PatternMatcher::globToRegex("src/*.cpp");
    EXPECT_TRUE(std::regex_match("src/main.cpp", regex3));
    EXPECT_FALSE(std::regex_match("main.cpp", regex3));
}

// Test: Check if string is a pattern
TEST_F(PatternMatcherTest, IsPattern) {
    EXPECT_TRUE(PatternMatcher::isPattern("*.txt"));
    EXPECT_TRUE(PatternMatcher::isPattern("file?"));
    EXPECT_TRUE(PatternMatcher::isPattern("src/*.cpp"));
    
    EXPECT_FALSE(PatternMatcher::isPattern("file.txt"));
    EXPECT_FALSE(PatternMatcher::isPattern("src/main.cpp"));
    EXPECT_FALSE(PatternMatcher::isPattern(""));
}

// Test: Match files with pattern *.txt
TEST_F(PatternMatcherTest, MatchTxtFiles) {
    auto matches = PatternMatcher::matchFilesInWorkingTree("*.txt", tempDir, gitterDir);
    
    EXPECT_GE(matches.size(), 2);
    // Verify file1.txt and file2.txt are in results (as absolute paths)
    bool found1 = false, found2 = false;
    for (const auto& match : matches) {
        std::string name = match.filename().string();
        if (name == "file1.txt") found1 = true;
        if (name == "file2.txt") found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

// Test: Match files with pattern *.cpp
TEST_F(PatternMatcherTest, MatchCppFiles) {
    auto matches = PatternMatcher::matchFilesInWorkingTree("*.cpp", tempDir, gitterDir);
    
    EXPECT_GE(matches.size(), 1);
    // Should include file3.cpp at root
    bool found = false;
    for (const auto& match : matches) {
        if (match.filename().string() == "file3.cpp") found = true;
    }
    EXPECT_TRUE(found);
}

// Test: Match files in subdirectory src/*.cpp
TEST_F(PatternMatcherTest, MatchSubdirectoryCpp) {
    auto matches = PatternMatcher::matchFilesInWorkingTree("src/*.cpp", tempDir, gitterDir);
    
    EXPECT_GE(matches.size(), 1);
    // Should include src/main.cpp
    bool found = false;
    for (const auto& match : matches) {
        std::string path = match.string();
        if (path.find("src/main.cpp") != std::string::npos) found = true;
    }
    EXPECT_TRUE(found);
}

// Test: Match files recursively
TEST_F(PatternMatcherTest, MatchRecursive) {
    // Match all .cpp files
    auto matches = PatternMatcher::matchFilesInWorkingTree("*.cpp", tempDir, gitterDir);
    
    // Should include at least file3.cpp
    EXPECT_GE(matches.size(), 1);
}

// Test: Match with question mark pattern
TEST_F(PatternMatcherTest, MatchQuestionMark) {
    createFile(tempDir, "test1.py", "test1");
    createFile(tempDir, "test2.py", "test2");
    createFile(tempDir, "test10.py", "test10");
    
    auto matches = PatternMatcher::matchFilesInWorkingTree("test?.py", tempDir, gitterDir);
    
    // Should match test1.py and test2.py
    EXPECT_GE(matches.size(), 2);
    bool found1 = false, found2 = false;
    for (const auto& match : matches) {
        std::string name = match.filename().string();
        if (name == "test1.py") found1 = true;
        if (name == "test2.py") found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

// Test: Match literal filename
TEST_F(PatternMatcherTest, MatchLiteralFilename) {
    // Not a pattern, should match exact file
    auto matches = PatternMatcher::matchFilesInWorkingTree("file1.txt", tempDir, gitterDir);
    
    EXPECT_GE(matches.size(), 1);
    bool found = false;
    for (const auto& match : matches) {
        if (match.filename().string() == "file1.txt") found = true;
    }
    EXPECT_TRUE(found);
}

// Test: Match paths in index
TEST_F(PatternMatcherTest, MatchPathsInIndex) {
    // Create index entries
    std::unordered_map<std::string, IndexEntry> entries;
    
    IndexEntry entry1;
    entry1.path = "file1.txt";
    entries["file1.txt"] = entry1;
    
    IndexEntry entry2;
    entry2.path = "file2.txt";
    entries["file2.txt"] = entry2;
    
    IndexEntry entry3;
    entry3.path = "file3.cpp";
    entries["file3.cpp"] = entry3;
    
    // Match with pattern (pattern first)
    auto matches = PatternMatcher::matchPathsInIndex("*.txt", entries);
    
    EXPECT_EQ(matches.size(), 2);
    EXPECT_TRUE(std::find(matches.begin(), matches.end(), 
                          "file1.txt") != matches.end());
    EXPECT_TRUE(std::find(matches.begin(), matches.end(), 
                          "file2.txt") != matches.end());
    EXPECT_FALSE(std::find(matches.begin(), matches.end(), 
                           "file3.cpp") != matches.end());
}

// Test: Match paths in index with subdirectory pattern
TEST_F(PatternMatcherTest, MatchIndexSubdirectoryPattern) {
    std::unordered_map<std::string, IndexEntry> entries;
    
    IndexEntry entry1;
    entry1.path = "src/main.cpp";
    entries["src/main.cpp"] = entry1;
    
    IndexEntry entry2;
    entry2.path = "src/util/helper.cpp";
    entries["src/util/helper.cpp"] = entry2;
    
    IndexEntry entry3;
    entry3.path = "src/util/helper.h";
    entries["src/util/helper.h"] = entry3;
    
    IndexEntry entry4;
    entry4.path = "main.cpp";
    entries["main.cpp"] = entry4;
    
    // Match src/*.cpp (pattern first)
    auto matches = PatternMatcher::matchPathsInIndex("src/*.cpp", entries);
    
    EXPECT_EQ(matches.size(), 1);
    EXPECT_TRUE(std::find(matches.begin(), matches.end(), 
                          "src/main.cpp") != matches.end());
    EXPECT_FALSE(std::find(matches.begin(), matches.end(), 
                          "src/util/helper.cpp") != matches.end()); // Nested, not matched
    EXPECT_FALSE(std::find(matches.begin(), matches.end(), 
                           "main.cpp") != matches.end());
}

// Test: Empty pattern matches nothing
TEST_F(PatternMatcherTest, EmptyPattern) {
    auto matches = PatternMatcher::matchFilesInWorkingTree("", tempDir, gitterDir);
    EXPECT_TRUE(matches.empty());
}

// Test: Pattern with special characters
TEST_F(PatternMatcherTest, PatternWithSpecialChars) {
    createFile(tempDir, "file[1].txt", "content");
    createFile(tempDir, "file[2].txt", "content");
    
    // Glob patterns may need escaping for special chars
    // This depends on implementation
}

// Test: Multiple patterns (not directly supported, but test individual)
TEST_F(PatternMatcherTest, MultiplePatterns) {
    auto txtMatches = PatternMatcher::matchFilesInWorkingTree("*.txt", tempDir, gitterDir);
    auto cppMatches = PatternMatcher::matchFilesInWorkingTree("*.cpp", tempDir, gitterDir);
    
    EXPECT_GE(txtMatches.size(), 2);
    EXPECT_GE(cppMatches.size(), 1);
}
