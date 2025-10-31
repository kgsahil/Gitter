#include "cli/commands/CheckoutCommand.hpp"

#include <iostream>

namespace gitter {

Expected<void> CheckoutCommand::execute(const AppContext&, const std::vector<std::string>&) {
    std::cout << "checkout: not implemented yet\n";
    return {};
}

}


