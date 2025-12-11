#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <iostream>
#include <memory>

namespace SentinelFS {

    enum class LogLevel {
        DEBUG,
        INFO,
        WARN,
        ERROR,
        CRITICAL
    };

    class Logger {
    public:
        static Logger& instance();

        void setLogFile(const std::string& path);
        void setLevel(LogLevel level);
        void setMaxFileSize(size_t maxSizeMB); // Set max log file size before rotation
        void setComponent(const std::string& component); // Set default component name
        
        // Level checking for conditional logging (avoid string construction overhead)
        bool isDebugEnabled() const { return currentLevel_ <= LogLevel::DEBUG; }
        bool isInfoEnabled() const { return currentLevel_ <= LogLevel::INFO; }
        LogLevel getLevel() const { return currentLevel_; }

        void log(LogLevel level, const std::string& message, const std::string& component = "");

        // Helper methods
        void debug(const std::string& message, const std::string& component = "");
        void info(const std::string& message, const std::string& component = "");
        void warn(const std::string& message, const std::string& component = "");
        void error(const std::string& message, const std::string& component = "");
        void critical(const std::string& message, const std::string& component = "");

    private:
        Logger() = default;
        ~Logger();

        std::mutex mutex_;
        std::ofstream logFile_;
        std::string logFilePath_;
        LogLevel currentLevel_ = LogLevel::INFO;
        std::string defaultComponent_ = "Core";
        size_t maxFileSizeMB_ = 100; // Default 100MB
        size_t currentFileSize_ = 0;

        std::string levelToString(LogLevel level);
        std::string getCurrentTime();
        void rotateLogFile();
        void checkAndRotate();
        size_t getFileSize();
    };

}
