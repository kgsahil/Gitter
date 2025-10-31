// Modular CLI entry using Command Pattern and Repository facade.

#include <iostream>
#include <string>
#include <vector>

#include "cli/CommandFactory.hpp"
#include "cli/CommandInvoker.hpp"
#include "cli/ICommand.hpp"
#include "cli/commands/HelpCommand.hpp"
#include "cli/commands/InitCommand.hpp"
#include "cli/commands/AddCommand.hpp"
#include "cli/commands/CommitCommand.hpp"
#include "cli/commands/StatusCommand.hpp"
#include "cli/commands/LogCommand.hpp"
#include "cli/commands/CheckoutCommand.hpp"
#include "cli/commands/RestoreCommand.hpp"
#include "cli/commands/CatFileCommand.hpp"
#include "cli/commands/ResetCommand.hpp"
#include "util/Logger.hpp"

using namespace gitter;

static void registerCommands() {
    auto& f = CommandFactory::instance();
    f.registerCreator("help", [] { return std::make_unique<HelpCommand>(); });
    f.registerCreator("init", [] { return std::make_unique<InitCommand>(); });
    f.registerCreator("add", [] { return std::make_unique<AddCommand>(); });
    f.registerCreator("commit", [] { return std::make_unique<CommitCommand>(); });
    f.registerCreator("status", [] { return std::make_unique<StatusCommand>(); });
    f.registerCreator("log", [] { return std::make_unique<LogCommand>(); });
    f.registerCreator("checkout", [] { return std::make_unique<CheckoutCommand>(); });
    f.registerCreator("restore", [] { return std::make_unique<RestoreCommand>(); });
    f.registerCreator("cat-file", [] { return std::make_unique<CatFileCommand>(); });
    f.registerCreator("reset", [] { return std::make_unique<ResetCommand>(); });
}

int main(int argc, char** argv) {
    registerCommands();
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    AppContext ctx{};
    CommandInvoker invoker;
    if (args.empty()) {
        auto cmd = CommandFactory::instance().create("help");
        invoker.invoke(*cmd, ctx, {});
        return 0;
    }
    std::string cmdName = args.front();
    args.erase(args.begin());
    auto cmd = CommandFactory::instance().create(cmdName);
    if (!cmd) {
        std::cerr << "Unknown command: " << cmdName << "\n";
        auto help = CommandFactory::instance().create("help");
        invoker.invoke(*help, ctx, {});
        return 1;
    }
    auto res = invoker.invoke(*cmd, ctx, args);
    return res ? 0 : 1;
}

