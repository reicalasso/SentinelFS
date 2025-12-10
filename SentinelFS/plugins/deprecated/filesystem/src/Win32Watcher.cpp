#include "Win32Watcher.h"
#include "Logger.h"
#include "MetricsCollector.h"

namespace SentinelFS {

    Win32Watcher::Win32Watcher() = default;

    Win32Watcher::~Win32Watcher() {
        shutdown();
    }

    bool Win32Watcher::initialize(EventCallback callback) {
        callback_ = callback;
    #ifdef _WIN32
        auto& logger = Logger::instance();
        logger.log(LogLevel::INFO, "Win32Watcher initialized (skeleton)", "Win32Watcher");
        // TODO: set up ReadDirectoryChangesW worker thread and handles here.
        return true;
    #else
        (void)callback_;
        auto& logger = Logger::instance();
        logger.log(LogLevel::WARN, "Win32Watcher used on non-Windows platform", "Win32Watcher");
        MetricsCollector::instance().incrementSyncErrors();
        return false;
    #endif
    }

    void Win32Watcher::shutdown() {
    #ifdef _WIN32
        // TODO: clean up handles and stop worker thread.
    #else
        // No-op on non-Windows.
    #endif
    }

    bool Win32Watcher::addWatch(const std::string& path) {
    #ifdef _WIN32
        auto& logger = Logger::instance();
        logger.log(LogLevel::DEBUG, "Adding Win32 watch for: " + path, "Win32Watcher");
        // TODO: register directory handle with ReadDirectoryChangesW.
        return true;
    #else
        (void)path;
        return false;
    #endif
    }

    bool Win32Watcher::removeWatch(const std::string& path) {
    #ifdef _WIN32
        auto& logger = Logger::instance();
        logger.log(LogLevel::DEBUG, "Removing Win32 watch for: " + path, "Win32Watcher");
        // TODO: unregister directory handle.
        return true;
    #else
        (void)path;
        return false;
    #endif
    }

} // namespace SentinelFS
