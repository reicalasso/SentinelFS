#include "FSEventsWatcher.h"
#include "Logger.h"
#include "MetricsCollector.h"

namespace SentinelFS {

    FSEventsWatcher::FSEventsWatcher() = default;

    FSEventsWatcher::~FSEventsWatcher() {
        shutdown();
    }

    bool FSEventsWatcher::initialize(EventCallback callback) {
        callback_ = callback;
    #ifdef __APPLE__
        auto& logger = Logger::instance();
        logger.log(LogLevel::INFO, "FSEventsWatcher initialized (skeleton)", "FSEventsWatcher");
        // TODO: configure FSEventStream and start run loop.
        return true;
    #else
        (void)callback_;
        auto& logger = Logger::instance();
        logger.log(LogLevel::WARN, "FSEventsWatcher used on non-macOS platform", "FSEventsWatcher");
        MetricsCollector::instance().incrementSyncErrors();
        return false;
    #endif
    }

    void FSEventsWatcher::shutdown() {
    #ifdef __APPLE__
        // TODO: stop FSEventStream and clean up.
    #else
        // No-op on non-macOS.
    #endif
    }

    bool FSEventsWatcher::addWatch(const std::string& path) {
    #ifdef __APPLE__
        auto& logger = Logger::instance();
        logger.log(LogLevel::DEBUG, "Adding FSEvents watch for: " + path, "FSEventsWatcher");
        // TODO: configure stream for this path if needed.
        return true;
    #else
        (void)path;
        return false;
    #endif
    }

    bool FSEventsWatcher::removeWatch(const std::string& path) {
    #ifdef __APPLE__
        auto& logger = Logger::instance();
        logger.log(LogLevel::DEBUG, "Removing FSEvents watch for: " + path, "FSEventsWatcher");
        // TODO: remove path from stream configuration.
        return true;
    #else
        (void)path;
        return false;
    #endif
    }

} // namespace SentinelFS
