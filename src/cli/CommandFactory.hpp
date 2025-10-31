#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cli/ICommand.hpp"

namespace gitter {

class CommandFactory {
public:
    using Creator = std::function<std::unique_ptr<ICommand>()>;

    static CommandFactory& instance();
    void registerCreator(const std::string& name, Creator creator);
    std::unique_ptr<ICommand> create(const std::string& name) const;
    void listCommands(std::vector<std::unique_ptr<ICommand>>& out) const;

private:
    CommandFactory() = default;
    std::unordered_map<std::string, Creator> creators;
};

}

 