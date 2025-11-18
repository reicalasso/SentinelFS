#include "logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>

namespace sfs {
namespace core {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : level_(LogLevel::INFO)
    , console_enabled_(true) {
}

Logger::~Logger() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

LogLevel Logger::get_level() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return level_;
}

void Logger::set_console_output(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    console_enabled_ = enabled;
}

bool Logger::set_file_output(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
    
    file_stream_.open(path, std::ios::app);
    return file_stream_.is_open();
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Filter by level
    if (level < level_) {
        return;
    }
    
    // Format message
    std::ostringstream oss;
    oss << "[" << get_timestamp() << "] "
        << "[" << level_to_string(level) << "] "
        << "[" << component << "] "
        << message;
    
    std::string formatted = oss.str();
    
    // Output to console
    if (console_enabled_) {
        if (level >= LogLevel::ERROR) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }
    }
    
    // Output to file
    if (file_stream_.is_open()) {
        file_stream_ << formatted << std::endl;
        file_stream_.flush();
    }
}

void Logger::trace(const std::string& component, const std::string& message) {
    log(LogLevel::TRACE, component, message);
}

void Logger::debug(const std::string& component, const std::string& message) {
    log(LogLevel::DEBUG, component, message);
}

void Logger::info(const std::string& component, const std::string& message) {
    log(LogLevel::INFO, component, message);
}

void Logger::warn(const std::string& component, const std::string& message) {
    log(LogLevel::WARN, component, message);
}

void Logger::error(const std::string& component, const std::string& message) {
    log(LogLevel::ERROR, component, message);
}

void Logger::fatal(const std::string& component, const std::string& message) {
    log(LogLevel::FATAL, component, message);
}

std::string Logger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::get_timestamp() const {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto time = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

} // namespace core
} // namespace sfs
