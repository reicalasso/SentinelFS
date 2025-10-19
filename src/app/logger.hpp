#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <sstream>

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

class Logger {
public:
    Logger(const std::string& logFile = "", LogLevel level = LogLevel::INFO);
    ~Logger();

    void setLevel(LogLevel level);
    void setLogFile(const std::string& logFile);

    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

    // Simple formatting function - implemented in header to avoid template issues
    template<typename... Args>
    void debug(const std::string& format, Args... args) {
        std::string message = formatString(format, args...);
        debug(message);
    }
    
    template<typename... Args>
    void info(const std::string& format, Args... args) {
        std::string message = formatString(format, args...);
        info(message);
    }
    
    template<typename... Args>
    void warning(const std::string& format, Args... args) {
        std::string message = formatString(format, args...);
        warning(message);
    }
    
    template<typename... Args>
    void error(const std::string& format, Args... args) {
        std::string message = formatString(format, args...);
        error(message);
    }

private:
    LogLevel currentLevel;
    std::string logFilePath;
    std::ofstream logFile;
    mutable std::mutex logMutex;

    void logInternal(LogLevel level, const std::string& message);
    
    template<typename... Args>
    std::string formatString(const std::string& format, Args... args) {
        std::ostringstream oss;
        formatHelper(oss, format, args...);
        return oss.str();
    }

    // Helper functions - implemented in header to avoid template issues
    void formatHelper(std::ostringstream& oss, const std::string& format) {
        oss << format;
    }
    
    template<typename T, typename... Args>
    void formatHelper(std::ostringstream& oss, const std::string& format, T&& t, Args&&... args) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            oss << format.substr(0, pos) << t;
            formatHelper(oss, format.substr(pos + 2), args...);
        } else {
            oss << format << t;
        }
    }
};