#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

class CheckoutCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "checkout"; }
    const char* description() const override { return "Switch branches"; }
    const char* helpNameLine() const override { return "checkout -  Switch branches or restore working tree files"; }
    const char* helpSynopsis() const override { return "gitter checkout <branch-name> [-b]"; }
    const char* helpDescription() const override { return "Switch to the specified branch or create a new branch with -b."; }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override {
        return { {"-b <branch-name>", "Create a new branch and switch to it."} };
    }
};

}


