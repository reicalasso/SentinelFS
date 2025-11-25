#include "Logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <ctime>

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
        logFilePath_ = path;
        logFile_.open(path, std::ios::app);
        currentFileSize_ = getFileSize();
    }

    void Logger::setMaxFileSize(size_t maxSizeMB) {
        maxFileSizeMB_ = maxSizeMB;
    }

    void Logger::setComponent(const std::string& component) {
        defaultComponent_ = component;
    }

    void Logger::setLevel(LogLevel level) {
        currentLevel_ = level;
    }

    void Logger::log(LogLevel level, const std::string& message, const std::string& component) {
        if (level < currentLevel_) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        std::string timestamp = getCurrentTime();
        std::string levelStr = levelToString(level);
        std::string comp = component.empty() ? defaultComponent_ : component;
        
        std::string logEntry = "[" + timestamp + "] [" + levelStr + "] [" + comp + "] " + message;

        // Output to console with colors
        if (level == LogLevel::ERROR || level == LogLevel::CRITICAL) {
            std::cerr << "\033[1;31m" << logEntry << "\033[0m" << std::endl;
        } else if (level == LogLevel::WARN) {
            std::cout << "\033[1;33m" << logEntry << "\033[0m" << std::endl;
        } else {
            std::cout << logEntry << std::endl;
        }

        // Output to file if open
        if (logFile_.is_open()) {
            logFile_ << logEntry << std::endl;
            logFile_.flush();
            currentFileSize_ += logEntry.length() + 1; // +1 for newline
            checkAndRotate();
        }
    }

    void Logger::debug(const std::string& message, const std::string& component) { 
        log(LogLevel::DEBUG, message, component); 
    }
    
    void Logger::info(const std::string& message, const std::string& component) { 
        log(LogLevel::INFO, message, component); 
    }
    
    void Logger::warn(const std::string& message, const std::string& component) { 
        log(LogLevel::WARN, message, component); 
    }
    
    void Logger::error(const std::string& message, const std::string& component) { 
        log(LogLevel::ERROR, message, component); 
    }
    
    void Logger::critical(const std::string& message, const std::string& component) { 
        log(LogLevel::CRITICAL, message, component); 
    }

    std::string Logger::levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    std::string Logger::getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        struct tm tm_buf;
        localtime_r(&in_time_t, &tm_buf);
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    void Logger::rotateLogFile() {
        if (!logFile_.is_open() || logFilePath_.empty()) {
            return;
        }

        logFile_.close();

        // Generate rotated filename with timestamp
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        struct tm tm_buf;
        localtime_r(&in_time_t, &tm_buf);
        ss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
        
        std::string rotatedPath = logFilePath_ + "." + ss.str();
        
        // Rename current log file
        try {
            std::filesystem::rename(logFilePath_, rotatedPath);
        } catch (const std::exception& e) {
            std::cerr << "Failed to rotate log file: " << e.what() << std::endl;
        }

        // Open new log file
        logFile_.open(logFilePath_, std::ios::app);
        currentFileSize_ = 0;
        
        // Log rotation event
        log(LogLevel::INFO, "Log file rotated to: " + rotatedPath, "Logger");
    }

    void Logger::checkAndRotate() {
        if (currentFileSize_ > maxFileSizeMB_ * 1024 * 1024) {
            rotateLogFile();
        }
    }

    size_t Logger::getFileSize() {
        if (logFilePath_.empty() || !std::filesystem::exists(logFilePath_)) {
            return 0;
        }
        try {
            return std::filesystem::file_size(logFilePath_);
        } catch (...) {
            return 0;
        }
    }

}
