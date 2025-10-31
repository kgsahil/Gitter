#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

class RestoreCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "restore"; }
    const char* description() const override { return "Restore working tree files or unstage changes"; }
    const char* helpNameLine() const override { return "restore -  Restore working tree files"; }
    const char* helpSynopsis() const override { return "gitter restore --staged <pathspec>..."; }
    const char* helpDescription() const override { return "Unstage files from the index without modifying the working tree. Supports glob patterns."; }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override {
        return { 
            {"--staged", "Remove files from the staging area (index)."},
            {"<pathspec>", "File path or glob pattern (e.g., *.txt, src/*.cpp) to unstage."}
        };
    }
};

}

