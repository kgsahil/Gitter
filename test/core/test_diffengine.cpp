#include <gtest/gtest.h>

#include "core/DiffEngine.hpp"

using namespace gitter;

class DiffEngineTest : public ::testing::Test {};

// Basic addition
TEST_F(DiffEngineTest, SimpleAddition) {
    std::string oldText = "line1\nline2\n";
    std::string newText = "line1\nline2\nline3\n";

    auto result = DiffEngine::computeDiff(oldText, newText);
    ASSERT_EQ(result.hunks.size(), 1u);

    const auto& h = result.hunks[0];
    bool sawAdd = false;
    for (const auto& l : h.lines) {
        if (l.type == DiffEngine::DiffLine::Addition && l.content == "line3") {
            sawAdd = true;
        }
    }
    EXPECT_TRUE(sawAdd);
}

// Basic deletion
TEST_F(DiffEngineTest, SimpleDeletion) {
    std::string oldText = "line1\nline2\nline3\n";
    std::string newText = "line1\nline3\n";

    auto result = DiffEngine::computeDiff(oldText, newText);
    ASSERT_EQ(result.hunks.size(), 1u);

    const auto& h = result.hunks[0];
    bool sawDel = false;
    for (const auto& l : h.lines) {
        if (l.type == DiffEngine::DiffLine::Deletion && l.content == "line2") {
            sawDel = true;
        }
    }
    EXPECT_TRUE(sawDel);
}

// Modification (delete + add)
TEST_F(DiffEngineTest, SimpleModification) {
    std::string oldText = "line1\nline2\n";
    std::string newText = "line1\nline2 modified\n";

    auto result = DiffEngine::computeDiff(oldText, newText);
    ASSERT_FALSE(result.hunks.empty());

    bool sawChange = false;
    for (const auto& h : result.hunks) {
        for (const auto& l : h.lines) {
            if (l.type == DiffEngine::DiffLine::Deletion ||
                l.type == DiffEngine::DiffLine::Addition) {
                sawChange = true;
            }
        }
    }
    EXPECT_TRUE(sawChange);
}

// No differences
TEST_F(DiffEngineTest, NoDifferencesProducesNoHunks) {
    std::string oldText = "same\ntext\n";
    std::string newText = "same\ntext\n";

    auto result = DiffEngine::computeDiff(oldText, newText);
    EXPECT_TRUE(result.hunks.empty());
}

// Handles files without trailing newline
TEST_F(DiffEngineTest, HandlesMissingTrailingNewline) {
    std::string oldText = "line1\nline2";
    std::string newText = "line1\nline2\nline3";

    auto result = DiffEngine::computeDiff(oldText, newText);
    ASSERT_EQ(result.hunks.size(), 1u);

    const auto& h = result.hunks[0];
    bool sawAdd = false;
    for (const auto& l : h.lines) {
        if (l.type == DiffEngine::DiffLine::Addition && l.content == "line3") {
            sawAdd = true;
        }
    }
    EXPECT_TRUE(sawAdd);
}


