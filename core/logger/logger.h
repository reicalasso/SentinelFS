#ifndef SFS_LOGGER_H
#define SFS_LOGGER_H

#include <string>
#include <memory>
#include <mutex>
#include <fstream>
#include <sstream>

namespace sfs {
namespace core {

/**
 * @brief Log levels
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

/**
 * @brief Logger - Simple thread-safe logging system
 * 
 * Provides basic logging capabilities for Core and plugins.
 * Supports console and file output with configurable log levels.
 */
class Logger {
public:
    /**
     * @brief Get singleton instance
     */
    static Logger& instance();
    
    /**
     * @brief Set minimum log level
     * 
     * Messages below this level will be ignored.
     */
    void set_level(LogLevel level);
    
    /**
     * @brief Get current log level
     */
    LogLevel get_level() const;
    
    /**
     * @brief Enable/disable console output
     */
    void set_console_output(bool enabled);
    
    /**
     * @brief Enable file output
     * 
     * @param path Path to log file
     * @return true if file opened successfully
     */
    bool set_file_output(const std::string& path);
    
    /**
     * @brief Log a message
     * 
     * @param level Log level
     * @param component Component name (e.g., "Core", "PluginLoader", "watcher.linux")
     * @param message Message to log
     */
    void log(LogLevel level, const std::string& component, const std::string& message);
    
    /**
     * @brief Convenience methods
     */
    void trace(const std::string& component, const std::string& message);
    void debug(const std::string& component, const std::string& message);
    void info(const std::string& component, const std::string& message);
    void warn(const std::string& component, const std::string& message);
    void error(const std::string& component, const std::string& message);
    void fatal(const std::string& component, const std::string& message);

private:
    Logger();
    ~Logger();
    
    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    LogLevel level_;
    bool console_enabled_;
    std::ofstream file_stream_;
    mutable std::mutex mutex_;
    
    std::string level_to_string(LogLevel level) const;
    std::string get_timestamp() const;
};

/**
 * @brief Macro helpers for convenient logging
 */
#define SFS_LOG_TRACE(component, msg) \
    sfs::core::Logger::instance().trace(component, msg)

#define SFS_LOG_DEBUG(component, msg) \
    sfs::core::Logger::instance().debug(component, msg)

#define SFS_LOG_INFO(component, msg) \
    sfs::core::Logger::instance().info(component, msg)

#define SFS_LOG_WARN(component, msg) \
    sfs::core::Logger::instance().warn(component, msg)

#define SFS_LOG_ERROR(component, msg) \
    sfs::core::Logger::instance().error(component, msg)

#define SFS_LOG_FATAL(component, msg) \
    sfs::core::Logger::instance().fatal(component, msg)

} // namespace core
} // namespace sfs

#endif // SFS_LOGGER_H
