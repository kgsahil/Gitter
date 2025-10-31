#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

class CommitCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "commit"; }
    const char* description() const override { return "Commit staged changes"; }
    const char* helpNameLine() const override { return "commit -  Record changes to the repository"; }
    const char* helpSynopsis() const override { return "gitter commit -m  [-a] <msg>"; }
    const char* helpDescription() const override { return "Create a new commit containing the current content of the index."; }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override {
        return { {"-a", "Automatically stage modified tracked files."}, {"-m <msg>", "Use <msg> as the commit message; multiple -m concatenate paragraphs."} };
    }
};

}


