#include "test_utils.hpp"

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace gitter::test::utils {

fs::path createTempDir() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::string dirname = "gitter_test_";
    for (int i = 0; i < 8; ++i) {
        dirname += "0123456789abcdef"[dis(gen)];
    }
    
    fs::path tempDir = fs::temp_directory_path() / dirname;
    fs::create_directories(tempDir);
    return tempDir;
}

void removeDir(const fs::path& dir) {
    if (fs::exists(dir)) {
        fs::remove_all(dir);
    }
}

fs::path createFile(
    const fs::path& baseDir,
    const std::string& filename,
    const std::string& content
) {
    fs::path filePath = baseDir / filename;
    
    // Create parent directories if needed
    fs::create_directories(filePath.parent_path());
    
    std::ofstream file(filePath);
    file << content;
    file.close();
    
    return filePath;
}

void createFiles(
    const fs::path& baseDir,
    const std::vector<std::pair<std::string, std::string>>& files
) {
    for (const auto& [filename, content] : files) {
        createFile(baseDir, filename, content);
    }
}

std::string readFile(const fs::path& filePath) {
    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool fileHasContent(const fs::path& filePath, const std::string& expectedContent) {
    if (!fs::exists(filePath)) {
        return false;
    }
    std::string actualContent = readFile(filePath);
    return actualContent == expectedContent;
}

fs::path initTestRepo(const fs::path& repoPath) {
    fs::create_directories(repoPath);
    
    // Create .gitter directory structure
    fs::path gitterDir = repoPath / ".gitter";
    fs::create_directories(gitterDir);
    fs::create_directories(gitterDir / "objects");
    fs::create_directories(gitterDir / "refs" / "heads");
    
    // Create HEAD
    std::ofstream headFile(gitterDir / "HEAD");
    headFile << "ref: refs/heads/main\n";
    headFile.close();
    
    return repoPath;
}

fs::path getCwd() {
    return fs::current_path();
}

void setCwd(const fs::path& dir) {
    fs::current_path(dir);
}

} // namespace gitter::test::utils

