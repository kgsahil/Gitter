#pragma once

#include "cli/ICommand.hpp"

namespace gitter {

    class MergeBaseCommand : public ICommand {
    public:
        Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
        const char* name() const override { return "merge-base"; }
        const char* description() const override { return "Find the LCN b/w two branch"; }
        const char* helpNameLine() const override { return "merge-base -  Find the LCN b/w branches"; }
        const char* helpSynopsis() const override { return "gitter merge-base [branch1] [branch2]"; }
        const char* helpDescription() const override { return ""; }
        std::vector<std::pair<std::string, std::string>> helpOptions() const override { return {}; }
    };

}

