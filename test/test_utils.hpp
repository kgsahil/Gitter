#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

namespace gitter::test {

/**
 * @brief Test utilities for Gitter tests
 * 
 * Provides helper functions for creating temporary repositories,
 * test files, and cleaning up test environments.
 */
namespace utils {

/**
 * @brief Create a temporary directory for testing
 * @return Path to temporary directory
 */
std::filesystem::path createTempDir();

/**
 * @brief Remove a directory and all its contents
 * @param dir Directory to remove
 */
void removeDir(const std::filesystem::path& dir);

/**
 * @brief Create a file with content in the given directory
 * @param baseDir Base directory
 * @param filename File name
 * @param content File content
 * @return Full path to created file
 */
std::filesystem::path createFile(
    const std::filesystem::path& baseDir,
    const std::string& filename,
    const std::string& content = ""
);

/**
 * @brief Create multiple files in a directory
 * @param baseDir Base directory
 * @param files Vector of filename-content pairs
 */
void createFiles(
    const std::filesystem::path& baseDir,
    const std::vector<std::pair<std::string, std::string>>& files
);

/**
 * @brief Read file content
 * @param filePath Path to file
 * @return File content as string
 */
std::string readFile(const std::filesystem::path& filePath);

/**
 * @brief Check if a file exists and has given content
 * @param filePath Path to file
 * @param expectedContent Expected content
 * @return True if file exists and content matches
 */
bool fileHasContent(const std::filesystem::path& filePath, const std::string& expectedContent);

/**
 * @brief Initialize a test repository
 * @param repoPath Path where repository should be initialized
 * @return Path to repository root
 */
std::filesystem::path initTestRepo(const std::filesystem::path& repoPath);

/**
 * @brief Get current working directory
 * @return Current working directory path
 */
std::filesystem::path getCwd();

/**
 * @brief Set working directory
 * @param dir Directory to change to
 */
void setCwd(const std::filesystem::path& dir);

} // namespace utils

} // namespace gitter::test

