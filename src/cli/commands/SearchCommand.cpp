#include "cli/commands/SearchCommand.hpp"

#include "core/SearchService.hpp"
#include "util/Logger.hpp"

#include <iostream>

namespace gitter {

Expected<void> SearchCommand::execute(const AppContext&, const std::vector<std::string>& args) {
    if (args.empty()) {
        return Error{ErrorCode::InvalidArgs, "Usage: gitter search <term> [<term>...] [--index]"};
    }

    // Parse arguments
    std::vector<std::string> terms;
    SearchMode mode = SearchMode::WorkingTree;

    for (const auto& arg : args) {
        if (arg == "--index") {
            mode = SearchMode::Index;
        } else {
            terms.push_back(arg);
        }
    }

    if (terms.empty()) {
        return Error{ErrorCode::InvalidArgs, "At least one search term is required"};
    }

    return runSearch(terms, mode);
}

Expected<void> SearchCommand::runSearch(const std::vector<std::string>& terms, SearchMode mode) {
    SearchService service;
    auto result = service.search(terms, mode);
    if (!result) {
        return result.error();
    }

    const auto& res = result.value();
    std::cout << "Found " << res.totalHits << " hit(s) in " << res.totalFiles << " file(s)\n";

    for (const auto& hit : res.hits) {
        std::cout << hit.path << " (" << hit.frequency << " match(es)): ";
        for (size_t i = 0; i < hit.lineNumbers.size() && i < 10; ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << hit.lineNumbers[i];
        }
        if (hit.lineNumbers.size() > 10) {
            std::cout << "...";
        }
        std::cout << "\n";
    }

    return {};
}

}  // namespace gitter
