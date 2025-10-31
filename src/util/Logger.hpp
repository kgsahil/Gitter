#pragma once

#include <string>

namespace gitter {

enum class LogLevel { Error = 0, Warn = 1, Info = 2, Debug = 3 };

class Logger {
public:
    static Logger& instance();
    void setLevel(LogLevel level);
    LogLevel level() const;
    void error(const std::string& msg) const;
    void warn(const std::string& msg) const;
    void info(const std::string& msg) const;
    void debug(const std::string& msg) const;

private:
    Logger();
    LogLevel currentLevel;
};

}

 