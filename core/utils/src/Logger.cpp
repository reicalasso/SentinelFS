#include "Logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace SentinelFS {

    Logger& Logger::instance() {
        static Logger instance;
        return instance;
    }

    Logger::~Logger() {
        if (logFile_.is_open()) {
            logFile_.close();
        }
    }

    void Logger::setLogFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (logFile_.is_open()) {
            logFile_.close();
        }
        logFile_.open(path, std::ios::app);
    }

    void Logger::setLevel(LogLevel level) {
        currentLevel_ = level;
    }

    void Logger::log(LogLevel level, const std::string& message) {
        if (level < currentLevel_) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        std::string timestamp = getCurrentTime();
        std::string levelStr = levelToString(level);
        std::string logEntry = "[" + timestamp + "] [" + levelStr + "] " + message;

        // Output to console
        std::cout << logEntry << std::endl;

        // Output to file if open
        if (logFile_.is_open()) {
            logFile_ << logEntry << std::endl;
        }
    }

    void Logger::debug(const std::string& message) { log(LogLevel::DEBUG, message); }
    void Logger::info(const std::string& message) { log(LogLevel::INFO, message); }
    void Logger::warn(const std::string& message) { log(LogLevel::WARN, message); }
    void Logger::error(const std::string& message) { log(LogLevel::ERROR, message); }

    std::string Logger::levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    std::string Logger::getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

}
