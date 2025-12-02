#pragma once

#include <string>
#include <vector>

namespace gitter {

/**
 * @brief Diff engine for computing file differences
 * 
 * This class provides unified diff computation between two text contents.
 * Uses google/diff-match-patch library for robust diff algorithms.
 * Converts diff-match-patch output to unified diff format with hunks.
 */
class DiffEngine {
public:
    struct DiffLine {
        enum Type { Context, Addition, Deletion };
        Type type;
        std::string content;
        int oldLineNum;
        int newLineNum;
    };

    struct DiffHunk {
        int oldStart;
        int oldLines;
        int newStart;
        int newLines;
        std::vector<DiffLine> lines;
    };

    struct DiffResult {
        std::vector<DiffHunk> hunks;
    };

    /**
     * @brief Compute diff between two file contents
     * @param oldContent Original file content
     * @param newContent New file content
     * @return Diff result with hunks in unified diff format
     */
    static DiffResult computeDiff(const std::string& oldContent, const std::string& newContent);
};

}

