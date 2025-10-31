#pragma once

#include <string>
#include <vector>

#include "util/Expected.hpp"

namespace gitter {

struct AppContext {
    // Extend with services if needed later
};

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual Expected<void> execute(const AppContext& ctx, const std::vector<std::string>& args) = 0;
    virtual const char* name() const = 0;
    virtual const char* description() const = 0;
    // Detailed help getters
    virtual const char* helpNameLine() const = 0;      // "<cmd> - <one line>"
    virtual const char* helpSynopsis() const = 0;      // usage synopsis
    virtual const char* helpDescription() const = 0;   // long description
    virtual std::vector<std::pair<std::string, std::string>> helpOptions() const = 0; // flag -> description
};

}

 