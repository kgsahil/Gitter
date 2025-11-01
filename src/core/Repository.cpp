#include "core/Repository.hpp"

#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace gitter {

Repository& Repository::instance() {
    static Repository repo;
    return repo;
}

Expected<void> Repository::init(const fs::path& path) {
    std::scoped_lock lock(mtx);
    fs::path root = fs::absolute(path);
    fs::path gd = root / ".gitter";
    if (fs::exists(gd)) {
        return Error{ErrorCode::AlreadyInitialized, ".gitter already exists"};
    }
    std::error_code ec;
    fs::create_directories(gd / "objects", ec);
    if (ec) return Error{ErrorCode::IoError, std::string("Failed to create directories: ") + ec.message()};
    fs::create_directories(gd / "refs" / "heads", ec);
    if (ec) return Error{ErrorCode::IoError, std::string("Failed to create refs: ") + ec.message()};

    {
        std::ofstream head(gd / "HEAD", std::ios::binary);
        if (!head) return Error{ErrorCode::IoError, "Failed to write HEAD"};
        head << "ref: refs/heads/main\n";
    }
    {
        std::ofstream mainRef(gd / "refs" / "heads" / "main", std::ios::binary);
        if (!mainRef) return Error{ErrorCode::IoError, "Failed to write main ref"};
    }
    rootPath = root;
    return {};
}

Expected<fs::path> Repository::discoverRoot(const fs::path& start) const {
    fs::path cur = fs::absolute(start);
    std::error_code ec;
    while (true) {
        fs::path gd = cur / ".gitter";
        if (fs::exists(gd, ec) && fs::is_directory(gd, ec)) {
            return cur;
        }
        if (!cur.has_parent_path() || cur == cur.parent_path()) {
            return Error{ErrorCode::NotARepository, "Not inside a Gitter repository"};
        }
        cur = cur.parent_path();
    }
}

Expected<std::pair<std::string, std::string>> Repository::resolveHEAD(const fs::path& root) {
    fs::path headPath = root / ".gitter" / "HEAD";
    if (!fs::exists(headPath)) {
        return Error{ErrorCode::InvalidArgs, "No HEAD file"};
    }
    
    std::ifstream headFile(headPath);
    if (!headFile) {
        return Error{ErrorCode::IoError, "Failed to read HEAD file"};
    }
    std::string headContent;
    std::getline(headFile, headContent);
    headFile.close();
    
    std::string currentHash;
    std::string branchRef;
    if (headContent.rfind("ref: ", 0) == 0) {
        // HEAD points to a branch
        branchRef = headContent.substr(5);
        fs::path refFile = root / ".gitter" / branchRef;
        
        if (!fs::exists(refFile)) {
            // No commits yet (ref file doesn't exist)
            return std::make_pair(std::string(), branchRef);
        }
        
        std::ifstream rf(refFile);
        if (!rf) {
            return Error{ErrorCode::IoError, "Failed to read branch reference"};
        }
        std::getline(rf, currentHash);
        rf.close();
    } else {
        // Detached HEAD (direct commit hash)
        currentHash = headContent;
    }
    
    return std::make_pair(currentHash, branchRef);
}

Expected<void> Repository::updateHEAD(const fs::path& root, const std::string& commitHash) {
    fs::path headPath = root / ".gitter" / "HEAD";
    if (!fs::exists(headPath)) {
        return Error{ErrorCode::InvalidArgs, "No HEAD file"};
    }
    
    std::ifstream headFileRead(headPath);
    if (!headFileRead) {
        return Error{ErrorCode::IoError, "Failed to read HEAD file"};
    }
    std::string headContent;
    std::getline(headFileRead, headContent);
    headFileRead.close();
    
    if (headContent.rfind("ref: ", 0) == 0) {
        std::string refPath = headContent.substr(5);
        fs::path refFile = root / ".gitter" / refPath;
        
        // Create parent directories if needed
        std::error_code ec;
        fs::create_directories(refFile.parent_path(), ec);
        if (ec) {
            return Error{ErrorCode::IoError, "Failed to create ref directory: " + ec.message()};
        }
        
        // Write commit hash to branch ref
        std::ofstream rf(refFile, std::ios::binary);
        if (!rf) {
            return Error{ErrorCode::IoError, "Failed to write ref file"};
        }
        rf << commitHash << "\n";
        rf.flush();
        if (!rf || !rf.good()) {
            return Error{ErrorCode::IoError, "Failed to write commit hash to ref"};
        }
    } else {
        // Detached HEAD - update HEAD directly
        std::ofstream headFileWrite(headPath, std::ios::binary);
        if (!headFileWrite) {
            return Error{ErrorCode::IoError, "Failed to write HEAD file"};
        }
        headFileWrite << commitHash << "\n";
        headFileWrite.flush();
        if (!headFileWrite || !headFileWrite.good()) {
            return Error{ErrorCode::IoError, "Failed to write HEAD"};
        }
    }
    
    return {};
}

Expected<bool> Repository::branchExists(const fs::path& root, const std::string& branchName) {
    fs::path refPath = root / ".gitter" / "refs" / "heads" / branchName;
    std::error_code ec;
    bool exists = fs::exists(refPath, ec) && fs::is_regular_file(refPath, ec);
    return exists;
}

Expected<std::vector<std::string>> Repository::listBranches(const fs::path& root) {
    fs::path headsDir = root / ".gitter" / "refs" / "heads";
    std::vector<std::string> branches;
    
    if (!fs::exists(headsDir)) {
        return branches;  // Empty list if directory doesn't exist
    }
    
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(headsDir, ec)) {
        if (entry.is_regular_file()) {
            branches.push_back(entry.path().filename().string());
        }
    }
    
    if (ec) {
        return Error{ErrorCode::IoError, "Failed to read branches directory: " + ec.message()};
    }
    
    return branches;
}

Expected<std::string> Repository::getCurrentBranch(const fs::path& root) {
    fs::path headPath = root / ".gitter" / "HEAD";
    if (!fs::exists(headPath)) {
        return Error{ErrorCode::InvalidArgs, "No HEAD file"};
    }
    
    std::ifstream headFile(headPath);
    if (!headFile) {
        return Error{ErrorCode::IoError, "Failed to read HEAD file"};
    }
    std::string headContent;
    std::getline(headFile, headContent);
    headFile.close();
    
    if (headContent.rfind("ref: ", 0) == 0) {
        // Extract branch name from ref path (e.g., "ref: refs/heads/main" -> "main")
        std::string refPath = headContent.substr(5);
        fs::path ref(refPath);
        return ref.filename().string();
    }
    
    // Detached HEAD - no branch name
    return std::string();
}

Expected<void> Repository::createBranch(const fs::path& root, const std::string& branchName, const std::string& commitHash) {
    fs::path refFile = root / ".gitter" / "refs" / "heads" / branchName;
    
    // Create parent directories if needed
    std::error_code ec;
    fs::create_directories(refFile.parent_path(), ec);
    if (ec) {
        return Error{ErrorCode::IoError, "Failed to create branch directory: " + ec.message()};
    }
    
    // Write commit hash to branch ref file
    std::ofstream rf(refFile, std::ios::binary);
    if (!rf) {
        return Error{ErrorCode::IoError, "Failed to create branch reference"};
    }
    rf << commitHash << "\n";
    rf.flush();
    if (!rf || !rf.good()) {
        return Error{ErrorCode::IoError, "Failed to write branch reference"};
    }
    
    return {};
}

Expected<void> Repository::switchToBranch(const fs::path& root, const std::string& branchName) {
    fs::path headPath = root / ".gitter" / "HEAD";
    
    // Update HEAD to point to the branch
    std::ofstream headFile(headPath, std::ios::binary);
    if (!headFile) {
        return Error{ErrorCode::IoError, "Failed to update HEAD"};
    }
    headFile << "ref: refs/heads/" << branchName << "\n";
    headFile.flush();
    if (!headFile || !headFile.good()) {
        return Error{ErrorCode::IoError, "Failed to write HEAD"};
    }
    
    return {};
}

Expected<std::string> Repository::getBranchCommit(const fs::path& root, const std::string& branchName) {
    fs::path refPath = root / ".gitter" / "refs" / "heads" / branchName;
    
    if (!fs::exists(refPath)) {
        return std::string();
    }
    
    std::ifstream refFile(refPath);
    if (!refFile) {
        return Error{ErrorCode::IoError, "Failed to read branch reference"};
    }
    
    std::string hash;
    std::getline(refFile, hash);
    
    // Remove any trailing whitespace
    while (!hash.empty() && std::isspace(static_cast<unsigned char>(hash.back()))) {
        hash.pop_back();
    }
    
    return hash;
}

}

 
