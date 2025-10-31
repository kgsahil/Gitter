#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

class HelpCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "help"; }
    const char* description() const override { return "List commands and usage"; }
    const char* helpNameLine() const override { return "help -  Show help for commands"; }
    const char* helpSynopsis() const override { return "gitter help [command]"; }
    const char* helpDescription() const override { return "Display a list of commands or detailed help for a specific command."; }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override { return {}; }
};

}

 