#include "core/Repository.hpp"

#include <fstream>

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

}

 
