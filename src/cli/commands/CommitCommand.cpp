#include "cli/commands/CommitCommand.hpp"

#include <iostream>

namespace gitter {

Expected<void> CommitCommand::execute(const AppContext&, const std::vector<std::string>&) {
    std::cout << "commit: not implemented yet\n";
    return {};
}

}


