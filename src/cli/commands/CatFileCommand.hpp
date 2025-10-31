#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

/**
 * @brief Execute 'gitter cat-file' command
 * 
 * Inspects Git objects stored in .gitter/objects/.
 * Similar to 'git cat-file', allows viewing blob/tree/commit objects.
 * 
 * Usage:
 *   gitter cat-file blob <hash>     - Show blob content
 *   gitter cat-file tree <hash>     - Show tree entries
 *   gitter cat-file commit <hash>   - Show commit content
 *   gitter cat-file -t <hash>       - Show object type
 *   gitter cat-file -s <hash>       - Show object size
 */
class CatFileCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "cat-file"; }
    const char* description() const override { return "Inspect Git objects"; }
    const char* helpNameLine() const override { return "cat-file - Inspect Git objects"; }
    const char* helpSynopsis() const override { 
        return "gitter cat-file <type> <hash>\n"
               "       gitter cat-file (-t | -s) <hash>"; 
    }
    const char* helpDescription() const override {
        return "Display the contents of a Git object (blob, tree, or commit).\n\n"
               "Examples:\n"
               "  gitter cat-file blob abc123...     Show blob file content\n"
               "  gitter cat-file tree def456...     Show tree directory entries\n"
               "  gitter cat-file commit ghi789...   Show commit metadata and message\n"
               "  gitter cat-file -t abc123...       Show object type (blob/tree/commit)\n"
               "  gitter cat-file -s abc123...       Show object size in bytes";
    }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override {
        return {
            {"<type>", "Object type: 'blob', 'tree', or 'commit'"},
            {"<hash>", "40-character SHA-1 hash of the object"},
            {"-t", "Show object type instead of contents"},
            {"-s", "Show object size instead of contents"}
        };
    }
};

}

