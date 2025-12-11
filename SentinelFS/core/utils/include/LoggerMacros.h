/**
 * @file LoggerMacros.h
 * @brief Performance-optimized logging macros
 * 
 * These macros prevent expensive string construction when logging is disabled.
 * 
 * Example:
 *   LOG_DEBUG("Processing file: " + filename);  // BAD: Always constructs string
 *   LOG_DEBUG_IF("Processing file: " + filename);  // GOOD: Only if debug enabled
 */

#pragma once

#include "Logger.h"

namespace SentinelFS {

// Conditional logging macros - avoid string construction overhead
#define LOG_DEBUG_IF(msg) \
    do { \
        auto& logger__ = Logger::instance(); \
        if (logger__.isDebugEnabled()) { \
            logger__.debug(msg); \
        } \
    } while(0)

#define LOG_DEBUG_COMP_IF(msg, component) \
    do { \
        auto& logger__ = Logger::instance(); \
        if (logger__.isDebugEnabled()) { \
            logger__.debug(msg, component); \
        } \
    } while(0)

#define LOG_INFO_IF(msg) \
    do { \
        auto& logger__ = Logger::instance(); \
        if (logger__.isInfoEnabled()) { \
            logger__.info(msg); \
        } \
    } while(0)

#define LOG_INFO_COMP_IF(msg, component) \
    do { \
        auto& logger__ = Logger::instance(); \
        if (logger__.isInfoEnabled()) { \
            logger__.info(msg, component); \
        } \
    } while(0)

// Regular logging (always executes)
#define LOG_WARN(msg) Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)
#define LOG_CRITICAL(msg) Logger::instance().critical(msg)

#define LOG_WARN_COMP(msg, component) Logger::instance().warn(msg, component)
#define LOG_ERROR_COMP(msg, component) Logger::instance().error(msg, component)
#define LOG_CRITICAL_COMP(msg, component) Logger::instance().critical(msg, component)

// Scoped performance timer (logs elapsed time on destruction)
class ScopedTimer {
public:
    explicit ScopedTimer(const std::string& name, const std::string& component = "Performance")
        : name_(name), component_(component), start_(std::chrono::steady_clock::now()) {}
    
    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
        
        auto& logger = Logger::instance();
        if (logger.isDebugEnabled()) {
            logger.debug(name_ + " took " + std::to_string(duration) + "ms", component_);
        }
    }

private:
    std::string name_;
    std::string component_;
    std::chrono::steady_clock::time_point start_;
};

#define SCOPED_TIMER(name) ScopedTimer timer__(name)
#define SCOPED_TIMER_COMP(name, component) ScopedTimer timer__(name, component)

} // namespace SentinelFS
