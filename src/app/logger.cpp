#include "logger.hpp"
#include <chrono>
#include <iomanip>

Logger::Logger(const std::string& logFile, LogLevel level) 
    : currentLevel(level) {
    
    if (!logFile.empty()) {
        logFilePath = logFile;
        this->logFile.open(logFile, std::ios::app);
    }
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Logger::setLevel(LogLevel level) {
    currentLevel = level;
}

void Logger::setLogFile(const std::string& logFile) {
    if (this->logFile.is_open()) {
        this->logFile.close();
    }
    logFilePath = logFile;
    this->logFile.open(logFile, std::ios::app);
}

void Logger::debug(const std::string& message) {
    if (currentLevel <= LogLevel::DEBUG) {
        logInternal(LogLevel::DEBUG, message);
    }
}

void Logger::info(const std::string& message) {
    if (currentLevel <= LogLevel::INFO) {
        logInternal(LogLevel::INFO, message);
    }
}

void Logger::warning(const std::string& message) {
    if (currentLevel <= LogLevel::WARNING) {
        logInternal(LogLevel::WARNING, message);
    }
}

void Logger::error(const std::string& message) {
    if (currentLevel <= LogLevel::ERROR) {
        logInternal(LogLevel::ERROR, message);
    }
}

void Logger::logInternal(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    std::string levelStr;
    switch (level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO: levelStr = "INFO"; break;
        case LogLevel::WARNING: levelStr = "WARNING"; break;
        case LogLevel::ERROR: levelStr = "ERROR"; break;
    }
    
    std::string logLine = "[" + oss.str() + "] [" + levelStr + "] " + message + "\n";
    
    // Output to console
    std::cout << logLine;
    
    // Output to file if open
    if (logFile.is_open()) {
        logFile << logLine;
        logFile.flush();
    }
}