#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

class ResetCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "reset"; }
    const char* description() const override { return "Reset current HEAD to the specified state"; }
    const char* helpNameLine() const override { return "reset -  Reset current HEAD to the specified state"; }
    const char* helpSynopsis() const override { return "gitter reset [HEAD~<n>]"; }
    const char* helpDescription() const override { return "Reset HEAD to a previous commit. Changes after the target commit are unindexed."; }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override {
        return { {"HEAD~<n>", "Reset to the commit n steps back from HEAD (e.g., HEAD~1, HEAD~2)"}, {"HEAD", "Reset to current HEAD (no change)"} };
    }
};

}
