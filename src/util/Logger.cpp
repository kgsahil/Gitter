#include "util/Logger.hpp"

#include <cstdlib>
#include <iostream>

namespace gitter {

static LogLevel parseEnvLogLevel() {
    const char* env = std::getenv("GITTER_LOG");
    if (!env) return LogLevel::Info;
    std::string v(env);
    if (v == "debug" || v == "3") return LogLevel::Debug;
    if (v == "info" || v == "2") return LogLevel::Info;
    if (v == "warn" || v == "1") return LogLevel::Warn;
    if (v == "error" || v == "0") return LogLevel::Error;
    return LogLevel::Info;
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() : currentLevel(parseEnvLogLevel()) {}

void Logger::setLevel(LogLevel level) { currentLevel = level; }
LogLevel Logger::level() const { return currentLevel; }

void Logger::error(const std::string& msg) const { if (currentLevel >= LogLevel::Error) std::cerr << "[error] " << msg << "\n"; }
void Logger::warn(const std::string& msg) const { if (currentLevel >= LogLevel::Warn) std::cerr << "[warn ] " << msg << "\n"; }
void Logger::info(const std::string& msg) const { if (currentLevel >= LogLevel::Info) std::cout << "[info ] " << msg << "\n"; }
void Logger::debug(const std::string& msg) const { if (currentLevel >= LogLevel::Debug) std::cout << "[debug] " << msg << "\n"; }

}

 