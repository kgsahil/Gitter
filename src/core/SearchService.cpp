#include "core/SearchService.hpp"

#include "core/ObjectStore.hpp"
#include "core/Repository.hpp"
#include "core/CommitObject.hpp"
#include "core/Index.hpp"
#include "core/TreeBuilder.hpp"
#include "util/IHasher.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

size_t countOccurrences(const std::string& text, const std::string& term) {
    if (term.empty()) return 0;
    size_t count = 0;
    size_t pos = 0;
    while ((pos = text.find(term, pos)) != std::string::npos) {
        ++count;
        pos += term.size();
    }
    return count;
}

bool isBinaryContent(const std::string& content) {
    return content.find('\0') != std::string::npos;
}

void collectFiles(const fs::path& dir, const fs::path& skipDir, std::vector<fs::path>& files) {
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (ec) continue;
        if (entry.is_regular_file(ec) && !ec) {
            // Skip .gitter directory
            auto relPath = fs::relative(entry.path(), dir, ec);
            if (ec) continue;
            bool skip = false;
            for (const auto& part : relPath) {
                if (part == skipDir.filename()) {
                    skip = true;
                    break;
                }
            }
            if (!skip) {
                files.push_back(entry.path());
            }
        }
    }
}

struct FileSearchResult {
    uint64_t frequency{0};
    std::vector<uint32_t> lineNumbers;
    bool hasMatches() const { return frequency > 0; }
};

FileSearchResult searchInContent(const std::string& content, const std::vector<std::string>& normalizedTerms) {
    FileSearchResult result;
    
    std::istringstream lineStream(content);
    std::string line;
    uint32_t lineNum = 0;
    
    while (std::getline(lineStream, line)) {
        ++lineNum;
        std::string lowerLine = toLower(line);
        
        size_t lineMatches = 0;
        for (const auto& term : normalizedTerms) {
            lineMatches += countOccurrences(lowerLine, term);
        }
        
        if (lineMatches > 0) {
            result.frequency += lineMatches;
            result.lineNumbers.push_back(lineNum);
        }
    }
    
    return result;
}

}  // namespace

namespace gitter {

Expected<SearchResult> SearchService::search(const std::vector<std::string>& terms, SearchMode mode) {
    if (terms.empty()) {
        return Error{ErrorCode::InvalidArgs, "At least one search term required"};
    }

    // Normalize terms
    std::vector<std::string> normalized;
    normalized.reserve(terms.size());
    for (const auto& term : terms) {
        auto t = term;
        // Trim whitespace
        t.erase(0, t.find_first_not_of(" \t\n\r"));
        t.erase(t.find_last_not_of(" \t\n\r") + 1);
        if (!t.empty()) {
            normalized.push_back(toLower(t));
        }
    }

    if (normalized.empty()) {
        return Error{ErrorCode::InvalidArgs, "No valid search terms"};
    }

    // Load cache if available
    if (!cacheLoaded_) {
        loadCache();
    }

    Expected<SearchResult> result =
        (mode == SearchMode::WorkingTree) ? searchWorkingTree(normalized)
                                          : searchIndexedFiles(normalized);

    if (result) {
        saveCache();
    }

    return result;
}

Expected<SearchResult> SearchService::searchWorkingTree(const std::vector<std::string>& normalizedTerms) {
    // Find working directory (current directory)
    fs::path workingDir = fs::current_path();
    
    // Collect all files, skip .gitter
    std::vector<fs::path> files;
    fs::path gitterDir = workingDir / ".gitter";
    collectFiles(workingDir, gitterDir, files);

    std::vector<SearchHit> hits;
    uint64_t totalHits = 0;

    for (const auto& filePath : files) {
        std::ifstream in(filePath);
        if (!in) continue;

        std::stringstream buffer;
        buffer << in.rdbuf();
        std::string content = buffer.str();

        if (isBinaryContent(content)) continue;

        // Search content
        auto fileResult = searchInContent(content, normalizedTerms);
        
        if (fileResult.hasMatches()) {
            std::string relPath = fs::relative(filePath, workingDir).string();
            
            // Update cache for each term that matched
            std::string lowerContent = toLower(content);
            for (const auto& term : normalizedTerms) {
                if (lowerContent.find(term) != std::string::npos) {
                    updateCacheEntry(term, relPath);
                }
            }
            
            hits.push_back(SearchHit{
                relPath,
                fileResult.frequency,
                std::move(fileResult.lineNumbers)
            });
            totalHits += fileResult.frequency;
        }
    }

    // Sort by frequency desc
    std::sort(hits.begin(), hits.end(), [](const SearchHit& a, const SearchHit& b) {
        return a.frequency != b.frequency ? a.frequency > b.frequency : a.path < b.path;
    });

    SearchResult result;
    result.hits = std::move(hits);
    result.totalHits = totalHits;
    result.totalFiles = static_cast<uint64_t>(result.hits.size());
    return result;
}

Expected<SearchResult> SearchService::searchIndexedFiles(const std::vector<std::string>& normalizedTerms) {
    // Discover repository
    auto rootRes = Repository::instance().discoverRoot(fs::current_path());
    if (!rootRes) {
        return rootRes.error();
    }
    fs::path repoRoot = rootRes.value();

    // Load index (staged files)
    Index index;
    if (!index.load(repoRoot)) {
        return Error{ErrorCode::IoError, "Failed to load index"};
    }

    // Get HEAD tree (committed files)
    auto headRes = Repository::resolveHEAD(repoRoot);
    if (!headRes) {
        return headRes.error();
    }
    std::string headHash = headRes.value().first;

    // Collect all paths from index
    std::vector<std::string> paths;
    for (const auto& [path, entry] : index.entries()) {
        paths.push_back(path);
    }

    // If HEAD exists, also collect committed files
    ObjectStore store(repoRoot);
    if (!headHash.empty()) {
        try {
            auto commit = store.readCommit(headHash);
            auto treeEntries = store.readTree(commit.treeHash);
            for (const auto& entry : treeEntries) {
                // Add if not already in index
                if (index.entries().find(entry.name) == index.entries().end()) {
                    paths.push_back(entry.name);
                }
            }
        } catch (...) {
            // Ignore tree read errors
        }
    }

    std::vector<SearchHit> hits;
    uint64_t totalHits = 0;

    // Search each path
    for (const auto& path : paths) {
        std::string content;
        
        // Try to get from index first
        auto it = index.entries().find(path);
        if (it != index.entries().end()) {
            try {
                content = store.readBlob(it->second.hashHex);
            } catch (...) {
                continue;
            }
        } else if (!headHash.empty()) {
            // Try from HEAD
            try {
                auto commit = store.readCommit(headHash);
                auto treeEntries = store.readTree(commit.treeHash);
                for (const auto& entry : treeEntries) {
                    if (entry.name == path) {
                        content = store.readBlob(entry.hashHex);
                        break;
                    }
                }
            } catch (...) {
                continue;
            }
        }

        if (content.empty() || isBinaryContent(content)) continue;

        // Search content
        auto fileResult = searchInContent(content, normalizedTerms);
        
        if (fileResult.hasMatches()) {
            // Update cache for each term that matched
            std::string lowerContent = toLower(content);
            for (const auto& term : normalizedTerms) {
                if (lowerContent.find(term) != std::string::npos) {
                    updateCacheEntry(term, path);
                }
            }
            
            hits.push_back(SearchHit{
                path,
                fileResult.frequency,
                std::move(fileResult.lineNumbers)
            });
            totalHits += fileResult.frequency;
        }
    }

    // Sort by frequency desc
    std::sort(hits.begin(), hits.end(), [](const SearchHit& a, const SearchHit& b) {
        return a.frequency != b.frequency ? a.frequency > b.frequency : a.path < b.path;
    });

    SearchResult result;
    result.hits = std::move(hits);
    result.totalHits = totalHits;
    result.totalFiles = static_cast<uint64_t>(result.hits.size());
    return result;
}

void SearchService::loadCache() {
    if (cacheLoaded_) return;

    // Try to find cache file in .gitter/search/cache.txt
    auto rootRes = Repository::instance().discoverRoot(fs::current_path());
    if (!rootRes) return;

    cacheFile_ = rootRes.value() / ".gitter" / "search" / "cache.txt";
    
    std::ifstream in(cacheFile_);
    if (!in) {
        cacheLoaded_ = true;
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        
        // Format: term|path1,path2,path3
        size_t pos = line.find('|');
        if (pos == std::string::npos) continue;
        
        std::string term = line.substr(0, pos);
        std::string pathsStr = line.substr(pos + 1);
        
        std::vector<std::string> paths;
        std::istringstream pathStream(pathsStr);
        std::string path;
        while (std::getline(pathStream, path, ',')) {
            if (!path.empty()) {
                paths.push_back(path);
            }
        }
        
        cache_[term] = std::move(paths);
    }
    
    cacheLoaded_ = true;
}

void SearchService::saveCache() {
    if (cacheFile_.empty()) return;

    // Ensure directory exists
    std::error_code ec;
    fs::create_directories(cacheFile_.parent_path(), ec);
    if (ec) return;

    std::ofstream out(cacheFile_);
    if (!out) return;

    for (const auto& [term, paths] : cache_) {
        out << term << "|";
        for (size_t i = 0; i < paths.size(); ++i) {
            if (i > 0) out << ",";
            out << paths[i];
        }
        out << "\n";
    }
}

void SearchService::updateCacheEntry(const std::string& term, const std::string& path) {
    auto& paths = cache_[term];
    if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
    }
}

}  // namespace gitter
