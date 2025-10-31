#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

class LogCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "log"; }
    const char* description() const override { return "Show commit history"; }
    const char* helpNameLine() const override { return "log -  Show commit logs"; }
    const char* helpSynopsis() const override { return "gitter log [--max-count <n>] [--oneline]"; }
    const char* helpDescription() const override { return "Show the commit history beginning at the current branch tip."; }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override {
        return { {"--max-count <n>", "Limit the number of commits."}, {"--oneline", "Condense each commit to a single line."} };
    }
};

}


