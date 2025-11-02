#include "util/PathUtils.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace gitter {

std::string PathUtils::normalize(const std::string& path) {
    fs::path p(path);
    std::string normalized = p.lexically_normal().generic_string();
    
    // Remove leading ./ if present
    if (normalized.length() >= 2 && normalized.substr(0, 2) == "./") {
        normalized = normalized.substr(2);
    }
    
    return normalized;
}

}

