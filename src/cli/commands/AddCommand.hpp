#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

class AddCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "add"; }
    const char* description() const override { return "Add file(s) to the staging area"; }
    const char* helpNameLine() const override { return "add -  Add file contents to the index"; }
    const char* helpSynopsis() const override { return "gitter add <pathspec> [<pathspec> ...]"; }
    const char* helpDescription() const override { return "Update the index using the current content found in the working tree. Supports files, directories, and glob patterns."; }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override {
        return { 
            {"<pathspec>", "File, directory, or glob pattern (e.g., *.txt, src/*.cpp, test?.py). Use '.' for current directory."}
        };
    }
};

}


