#include "util/PatternMatcher.hpp"

#include <algorithm>
#include <filesystem>
#include <regex>
#include "core/Index.hpp"

namespace fs = std::filesystem;

namespace gitter {
namespace PatternMatcher {

std::regex globToRegex(const std::string& pattern) {
    std::string regexStr;
    for (char c : pattern) {
        if (c == '*') {
            regexStr += ".*";
        } else if (c == '?') {
            regexStr += ".";
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
    std::regex re = globToRegex(pattern);
    std::error_code ec;
    
    // Search from current directory
    fs::path searchRoot = fs::current_path();
    
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
            // Get relative path from current dir for pattern matching
            fs::path rel = fs::relative(entry, searchRoot, ec);
            std::string relStr = rel.generic_string();
            
            // Match against pattern
            if (std::regex_match(relStr, re) || std::regex_search(relStr, re)) {
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
    std::regex re = globToRegex(pattern);
    
    for (const auto& kv : indexPaths) {
        const std::string& path = kv.first;
        if (std::regex_match(path, re) || std::regex_search(path, re)) {
            matches.push_back(path);
        }
    }
    
    return matches;
}

}  // namespace PatternMatcher
}  // namespace gitter

