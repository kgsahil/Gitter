#pragma once

#include <filesystem>
#include <regex>
#include <string>
#include <vector>

namespace gitter {

/**
 * @brief Utility functions for glob pattern matching
 * 
 * Provides glob-to-regex conversion and pattern matching for pathspecs.
 * Used by add, restore, and other commands that support wildcards.
 * 
 * Supported patterns:
 *   * -> matches any characters (.*  in regex)
 *   ? -> matches single character (. in regex)
 *   [abc] -> character class (passed through)
 * 
 * Examples:
 *   *.txt          -> matches all .txt files
 *   src slash*.cpp -> matches .cpp files in src/
 *   test?.py       -> matches test1.py, test2.py, etc.
 */
namespace PatternMatcher {

/**
 * @brief Convert glob pattern to std::regex
 * 
 * Converts simple glob patterns to regex:
 *   * -> .*  (matches any characters)
 *   ? -> .   (matches single character)
 *   . -> \.  (literal dot)
 *   Special regex chars are escaped
 * 
 * @param pattern Glob pattern (e.g., "*.txt", "src slash *.cpp")
 * @return std::regex compiled from pattern
 * 
 * Example: "*.txt" -> ".*\.txt"
 */
std::regex globToRegex(const std::string& pattern);

/**
 * @brief Check if string contains glob pattern characters
 * 
 * @param path String to check
 * @return true if contains *, ?, or [
 */
bool isPattern(const std::string& path);

/**
 * @brief Match files in working tree against glob pattern
 * 
 * Recursively searches from current directory and returns all files
 * matching the glob pattern. Automatically skips .gitter directory.
 * 
 * @param pattern Glob pattern (e.g., "*.txt", "src slash *.cpp", "test?.py")
 * @param root Repository root path
 * @param gitterDirStr Normalized .gitter directory path to skip
 * @return Vector of absolute paths matching pattern
 * 
 * Example:
 *   auto matches = matchFilesInWorkingTree("*.cpp", repoRoot, gitterPath);
 */
std::vector<std::filesystem::path> matchFilesInWorkingTree(
    const std::string& pattern,
    const std::filesystem::path& root,
    const std::string& gitterDirStr
);

/**
 * @brief Match paths in index against glob pattern
 * 
 * Filters index entries by glob pattern and returns matching paths.
 * Used by restore command to unstage multiple files at once.
 * 
 * @param pattern Glob pattern (e.g., "*.txt", "src slash *.cpp")
 * @param indexPaths Set or map of paths currently in index
 * @return Vector of matching paths (relative to repo root)
 * 
 * Example:
 *   auto matches = matchPathsInIndex("*.cpp", index.entries());
 */
template<typename Container>
std::vector<std::string> matchPathsInIndex(const std::string& pattern, const Container& indexPaths);

}  // namespace PatternMatcher

}  // namespace gitter

