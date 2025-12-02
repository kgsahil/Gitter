#include "core/DiffEngine.hpp"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "diff_match_patch.h"

namespace gitter {

DiffEngine::DiffResult DiffEngine::computeDiff(const std::string& oldContent, const std::string& newContent) {
    DiffResult result;
    
    diff_match_patch<std::string> dmp;
    auto diffs = dmp.diff_main(oldContent, newContent);
    dmp.diff_cleanupSemantic(diffs);
    
    std::vector<DiffLine> allLines;
    int oldLineNum = 1;
    int newLineNum = 1;
    std::string oldBuffer;
    std::string newBuffer;
    
    auto trimTrailingNewline = [](std::string line) {
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        return line;
    };
    
    auto flushContextLine = [&]() {
        if (oldBuffer.empty() && newBuffer.empty()) return;
        if (oldBuffer != newBuffer) return;
        std::string line = trimTrailingNewline(oldBuffer);
        allLines.push_back({DiffLine::Context, line, oldLineNum, newLineNum});
        ++oldLineNum;
        ++newLineNum;
        oldBuffer.clear();
        newBuffer.clear();
    };
    
    auto flushDeletionLine = [&]() {
        if (oldBuffer.empty()) return;
        std::string line = trimTrailingNewline(oldBuffer);
        allLines.push_back({DiffLine::Deletion, line, oldLineNum, 0});
        ++oldLineNum;
        oldBuffer.clear();
    };
    
    auto flushAdditionLine = [&]() {
        if (newBuffer.empty()) return;
        std::string line = trimTrailingNewline(newBuffer);
        allLines.push_back({DiffLine::Addition, line, 0, newLineNum});
        ++newLineNum;
        newBuffer.clear();
    };
    
    for (const auto& diff : diffs) {
        const auto& text = diff.text;
        
        if (diff.operation == diff_match_patch<std::string>::EQUAL) {
            for (char c : text) {
                oldBuffer.push_back(c);
                newBuffer.push_back(c);
                if (c == '\n') {
                    flushContextLine();
                }
            }
            flushContextLine();
        } else if (diff.operation == diff_match_patch<std::string>::DELETE) {
            for (char c : text) {
                oldBuffer.push_back(c);
                if (c == '\n') {
                    flushDeletionLine();
                }
            }
            flushDeletionLine();
        } else if (diff.operation == diff_match_patch<std::string>::INSERT) {
            for (char c : text) {
                newBuffer.push_back(c);
                if (c == '\n') {
                    flushAdditionLine();
                }
            }
            flushAdditionLine();
        }
    }
    
    // Flush any remaining buffers
    if (!oldBuffer.empty() && !newBuffer.empty() && oldBuffer == newBuffer) {
        flushContextLine();
    } else {
        flushDeletionLine();
        flushAdditionLine();
    }
    
    // Group into hunks with context
    const int contextLines = 3;
    std::vector<DiffLine> hunkLines;
    int hunkOldStart = 1, hunkNewStart = 1;
    bool inHunk = false;
    int contextCount = 0;
    
    for (size_t idx = 0; idx < allLines.size(); idx++) {
        const auto& line = allLines[idx];
        
        if (line.type == DiffLine::Context) {
            contextCount++;
            if (inHunk) {
                hunkLines.push_back(line);
                // Close hunk if we have enough context after changes
                if (contextCount >= contextLines) {
                    // Finalize hunk
                    DiffHunk hunk;
                    hunk.oldStart = hunkOldStart;
                    hunk.newStart = hunkNewStart;
                    hunk.lines = hunkLines;
                    
                    // Count lines in hunk
                    int oldCount = 0, newCount = 0;
                    for (const auto& l : hunkLines) {
                        if (l.type == DiffLine::Context || l.type == DiffLine::Deletion) oldCount++;
                        if (l.type == DiffLine::Context || l.type == DiffLine::Addition) newCount++;
                    }
                    hunk.oldLines = oldCount;
                    hunk.newLines = newCount;
                    
                    result.hunks.push_back(hunk);
                    hunkLines.clear();
                    inHunk = false;
                    contextCount = 0;
                }
            }
        } else {
            // Change detected
            contextCount = 0;
            if (!inHunk) {
                // Start new hunk
                inHunk = true;
                hunkOldStart = line.oldLineNum > 0 ? line.oldLineNum : 1;
                hunkNewStart = line.newLineNum > 0 ? line.newLineNum : 1;
                
                // Add context before (up to contextLines)
                int contextStart = std::max(0, static_cast<int>(idx) - contextLines);
                for (int k = contextStart; k < static_cast<int>(idx); k++) {
                    if (k >= 0 && allLines[k].type == DiffLine::Context) {
                        hunkLines.push_back(allLines[k]);
                        hunkOldStart = allLines[k].oldLineNum;
                        hunkNewStart = allLines[k].newLineNum;
                    }
                }
            }
            hunkLines.push_back(line);
        }
    }
    
    // Close final hunk if still open
    if (inHunk && !hunkLines.empty()) {
        DiffHunk hunk;
        hunk.oldStart = hunkOldStart;
        hunk.newStart = hunkNewStart;
        hunk.lines = hunkLines;
        
        int oldCount = 0, newCount = 0;
        for (const auto& l : hunkLines) {
            if (l.type == DiffLine::Context || l.type == DiffLine::Deletion) oldCount++;
            if (l.type == DiffLine::Context || l.type == DiffLine::Addition) newCount++;
        }
        hunk.oldLines = oldCount;
        hunk.newLines = newCount;
        
        result.hunks.push_back(hunk);
    }
    
    return result;
}

}


