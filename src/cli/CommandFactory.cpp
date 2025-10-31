#include "cli/CommandFactory.hpp"

#include <algorithm>

namespace gitter {

CommandFactory& CommandFactory::instance() {
    static CommandFactory f;
    return f;
}

void CommandFactory::registerCreator(const std::string& name, Creator creator) {
    creators[name] = std::move(creator);
}

std::unique_ptr<ICommand> CommandFactory::create(const std::string& name) const {
    auto it = creators.find(name);
    if (it == creators.end()) return nullptr;
    return it->second();
}

void CommandFactory::listCommands(std::vector<std::unique_ptr<ICommand>>& out) const {
    out.clear();
    out.reserve(creators.size());
    for (const auto& kv : creators) {
        out.emplace_back(kv.second());
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return std::string(a->name()) < std::string(b->name());
    });
}

}

 