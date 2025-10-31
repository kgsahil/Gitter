#include "cli/commands/HelpCommand.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "cli/CommandFactory.hpp"

namespace gitter {

namespace {

void printCommandDetail(const ICommand& cmd) {
    std::cout << "Name:\n" << cmd.helpNameLine() << "\n\n";
    std::cout << "SYNOPSIS:\n" << cmd.helpSynopsis() << "\n\n";
    std::cout << "DESCRIPTION:\n" << cmd.helpDescription() << "\n\n";
    auto opts = cmd.helpOptions();
    if (!opts.empty()) {
        std::cout << "OPTIONS:\n";
        for (const auto& [opt, desc] : opts) {
            std::cout << opt << " :  " << desc << "\n\n";
        }
    }
}

}

Expected<void> HelpCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    if (!args.empty()) {
        std::string topic = args.front();
        auto cmd = CommandFactory::instance().create(topic);
        if (cmd) {
            printCommandDetail(*cmd);
            return {};
        }
        std::cerr << "Unknown help topic: " << topic << "\n\n";
    }

    std::vector<std::unique_ptr<ICommand>> cmds;
    CommandFactory::instance().listCommands(cmds);

    std::cout << "These are common Gitter command:\n\n\n";
    for (const auto& c : cmds) {
        std::cout << "  " << c->name() << "\t" << c->description() << "\n";
    }
    return {};
}

}

 