#include "util/PatternMatcher.hpp"

#include <algorithm>
#include <filesystem>
#include <regex>
#include "core/Index.hpp"

namespace fs = std::filesystem;

namespace gitter {
namespace PatternMatcher {

std::regex globToRegex(const std::string& pattern) {
    std::string regexStr = "^";  // Anchor at start
    for (char c : pattern) {
        if (c == '*') {
            // In Git, * doesn't match / unless it's ** (which we don't support yet)
            // So * matches [^/]* (any character except /)
            regexStr += "[^/]*";
        } else if (c == '?') {
            // ? matches a single character except /
            regexStr += "[^/]";
        } else if (c == '.') {
            regexStr += "\\.";
        } else if (c == '+' || c == '[' || c == ']' || c == '(' || c == ')' || 
                   c == '{' || c == '}' || c == '^' || c == '$' || c == '|' || c == '\\') {
            regexStr += '\\';
            regexStr += c;
        } else {
            regexStr += c;
        }
    }
    regexStr += "$";  // Anchor at end
    return std::regex(regexStr);
}

bool isPattern(const std::string& path) {
    return path.find('*') != std::string::npos || 
           path.find('?') != std::string::npos ||
           path.find('[') != std::string::npos;
}

std::vector<fs::path> matchFilesInWorkingTree(
    const std::string& pattern,
    const fs::path& root,
    const std::string& gitterDirStr
) {
    std::vector<fs::path> matches;
    
    // Empty pattern matches nothing (Git behavior)
    if (pattern.empty()) {
        return matches;
    }
    
    std::regex re = globToRegex(pattern);
    std::error_code ec;
    
    // Search from root directory (not current_path!)
    fs::path searchRoot = root;
    
    for (auto it = fs::recursive_directory_iterator(searchRoot, ec); 
         it != fs::recursive_directory_iterator(); 
         it.increment(ec)) {
        if (ec) break;
        const auto& entry = it->path();
        
        // Skip .gitter directory
        auto entryStr = entry.lexically_normal().string();
        if (entryStr.rfind(gitterDirStr, 0) == 0) {
            it.disable_recursion_pending();
            continue;
        }
        
        if (fs::is_regular_file(entry, ec)) {
            // Get relative path from root for pattern matching
            fs::path rel = fs::relative(entry, searchRoot, ec);
            std::string relStr = rel.generic_string();
            
            // Use regex_match for exact Git-style glob matching (not regex_search)
            if (std::regex_match(relStr, re)) {
                matches.push_back(entry);
            }
        }
    }
    
    return matches;
}

// Template specializations for common container types
template<>
std::vector<std::string> matchPathsInIndex<std::unordered_map<std::string, IndexEntry>>(
    const std::string& pattern,
    const std::unordered_map<std::string, IndexEntry>& indexPaths
) {
    std::vector<std::string> matches;
    
    // Empty pattern matches nothing (Git behavior)
    if (pattern.empty()) {
        return matches;
    }
    
    std::regex re = globToRegex(pattern);
    
    for (const auto& kv : indexPaths) {
        const std::string& path = kv.first;
        // Use regex_match for exact Git-style glob matching
        // This ensures "src slash *.cpp" only matches direct children like "src/main.cpp"
        // and not nested paths like "src/util/helper.cpp"
        if (std::regex_match(path, re)) {
            matches.push_back(path);
        }
    }
    
    return matches;
}

}  // namespace PatternMatcher
}  // namespace gitter

