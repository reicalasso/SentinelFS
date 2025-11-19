#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <iostream>

namespace SentinelFS {

    enum class LogLevel {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    class Logger {
    public:
        static Logger& instance();

        void setLogFile(const std::string& path);
        void setLevel(LogLevel level);

        void log(LogLevel level, const std::string& message);

        // Helper methods
        void debug(const std::string& message);
        void info(const std::string& message);
        void warn(const std::string& message);
        void error(const std::string& message);

    private:
        Logger() = default;
        ~Logger();

        std::mutex mutex_;
        std::ofstream logFile_;
        LogLevel currentLevel_ = LogLevel::INFO;

        std::string levelToString(LogLevel level);
        std::string getCurrentTime();
    };

}
