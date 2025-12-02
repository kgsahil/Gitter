#pragma once

#include "cli/ICommand.hpp"
#include "core/SearchService.hpp"
#include "util/Expected.hpp"

#include <string>
#include <vector>

namespace gitter {

class SearchCommand : public ICommand {
public:
    Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) override;
    const char* name() const override { return "search"; }
    const char* description() const override { return "Search for terms in files"; }
    const char* helpNameLine() const override { return "search - Search for terms in files"; }
    const char* helpSynopsis() const override { return "gitter search <term> [<term>...] [--index]"; }
    const char* helpDescription() const override { 
        return "Search for terms in files.\n"
               "By default searches all files in working directory.\n"
               "Use --index to search only staged and committed files.";
    }
    std::vector<std::pair<std::string, std::string>> helpOptions() const override {
        return {{"--index", "Search staged and committed files instead of working directory"}};
    }

private:
    Expected<void> runSearch(const std::vector<std::string>& terms, SearchMode mode);
};

}
