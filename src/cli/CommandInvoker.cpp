#include "cli/CommandInvoker.hpp"

#include "util/Logger.hpp"

namespace gitter {

Expected<void> CommandInvoker::invoke(ICommand& cmd, const AppContext& ctx, const std::vector<std::string>& args) {
    Logger::instance().debug(std::string("Executing command: ") + cmd.name());
    auto res = cmd.execute(ctx, args);
    if (!res) {
        Logger::instance().error(std::string(cmd.name()) + ": " + res.error().message);
        return res;
    }
    return {};
}

}

 