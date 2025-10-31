#include "cli/commands/LogCommand.hpp"

#include <iostream>

namespace gitter {

Expected<void> LogCommand::execute(const AppContext&, const std::vector<std::string>&) {
    std::cout << "log: not implemented yet\n";
    return {};
}

}


