#include "cli/commands/InitCommand.hpp"

#include <filesystem>
#include <iostream>

#include "core/Repository.hpp"

namespace gitter {

Expected<void> InitCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    std::filesystem::path target = std::filesystem::current_path();
    if (!args.empty()) {
        target = args.front();
    }
    auto res = Repository::instance().init(target);
    if (!res) {
        if (res.error().code == ErrorCode::AlreadyInitialized) {
            std::cout << "Gitter repository is already initialised in "
                << std::filesystem::absolute(target).string() << "/.gitter/\n";
        }
        else {
            std::cout << "Failed to initialize Gitter repository in "
                << std::filesystem::absolute(target).string() << "/.gitter/\n";

        }
    } else {
        std::cout << "Initialized empty Gitter repository in "
            << std::filesystem::absolute(target).string() << "/.gitter/\n";
    }
    return {};
}

}