#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

class DiffCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "diff"; }
    const char* description() const override { return "Show changes between working tree and index, or index and HEAD"; }
    const char* helpNameLine() const override { return "diff -  Show changes between working tree and index or index and HEAD"; }
    const char* helpSynopsis() const override { return "gitter diff [--cached|--staged]"; }
    const char* helpDescription() const override { 
        return "Show changes between:\n"
               "  - Working tree and index (default)\n"
               "  - Index and HEAD (with --cached or --staged)\n"
               "\n"
               "TODO: Support for commit comparisons (<commit>, <commit1> <commit2>)"; 
    }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override {
        return {
            {"--cached, --staged", "Show diff between index and HEAD"}
        };
    }
};

}

