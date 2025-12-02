#pragma once

#include "util/Expected.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace gitter {

enum class SearchMode {
    WorkingTree,  // Search ALL files in working directory
    Index         // Search staged + HEAD committed files only
};

struct SearchHit {
    std::string path;
    uint64_t frequency;
    std::vector<uint32_t> lineNumbers;
};

struct SearchResult {
    std::vector<SearchHit> hits;
    uint64_t totalHits;
    uint64_t totalFiles;
};

class SearchService {
public:
    Expected<SearchResult> search(const std::vector<std::string>& terms, SearchMode mode);

private:
    Expected<SearchResult> searchWorkingTree(const std::vector<std::string>& normalizedTerms);
    Expected<SearchResult> searchIndexedFiles(const std::vector<std::string>& normalizedTerms);
    
    // Cache helpers
    void loadCache();
    void saveCache();
    void updateCacheEntry(const std::string& term, const std::string& path);
    
    std::unordered_map<std::string, std::vector<std::string>> cache_; // term -> list of paths
    std::filesystem::path cacheFile_;
    bool cacheLoaded_{false};
};

}
