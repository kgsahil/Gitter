#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

class CheckoutCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "checkout"; }
    const char* description() const override { return "Switch branches"; }
    const char* helpNameLine() const override { return "checkout -  Switch branches or restore working tree files"; }
    const char* helpSynopsis() const override { return "gitter checkout <branch> [--force]"; }
    const char* helpDescription() const override { return "Switch to the specified branch and update the working tree."; }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override {
        return { {"--force", "Proceed even if the working tree has uncommitted changes."} };
    }
};

}


