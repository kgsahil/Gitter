#pragma once

#include <vector>

#include "cli/ICommand.hpp"

namespace gitter {

class CommandInvoker {
public:
    Expected<void> invoke(ICommand& cmd, const AppContext& ctx, const std::vector<std::string>& args);
};

}

 