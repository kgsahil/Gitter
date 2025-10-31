#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

class StatusCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "status"; }
    const char* description() const override { return "Show working tree status"; }
    const char* helpNameLine() const override { return "status -  Show the working tree status"; }
    const char* helpSynopsis() const override { return "gitter status"; }
    const char* helpDescription() const override { return "Show paths that are staged, unstaged, or untracked."; }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override { return {}; }
};

}


