#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

class InitCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "init"; }
    const char* description() const override { return "Initialize a new Gitter repository"; }
    const char* helpNameLine() const override { return "init -  Create an empty Gitter repository"; }
    const char* helpSynopsis() const override { return "gitter init [<directory>]"; }
    const char* helpDescription() const override { return "Create a new empty Gitter repository. If <directory> is omitted, the current directory is used."; }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override { return {}; }
};

}

 
