#include "IronRootPlugin.h"
#include "FanotifyWatcher.h"
#include "InotifyWatcher.h"
#include "Debouncer.h"
#include "FileOps.h"
#include "EventBus.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <filesystem>
#include <fnmatch.h>

namespace SentinelFS {

/**
 * @brief Implementation class (PIMPL)
 */
class IronRootPlugin::Impl {
public:
    EventBus* eventBus{nullptr};
    std::unique_ptr<IronRoot::IWatcher> watcher;
    std::unique_ptr<IronRoot::Debouncer> debouncer;
    
    IronRoot::WatchConfig config;
    IronRoot::IronWatchCallback watchCallback;
    IronRoot::BatchCallback batchCallback;
    
    IronRootPlugin::Stats stats;
    mutable std::mutex statsMutex;
    
    std::set<std::string> watchedPaths;
    mutable std::mutex watchMutex;
    
    bool useFanotify{false};
    
    void handleWatchEvent(const IronRoot::WatchEvent& event) {
        auto& logger = Logger::instance();
        
        // Check ignore patterns
        if (shouldIgnore(event.path)) {
            logger.log(LogLevel::DEBUG, "Ignoring: " + event.path, "IronRoot");
            return;
        }
        
        // Add to debouncer
        if (debouncer) {
            debouncer->addEvent(event);
        }
    }
    
    void handleDebouncedEvent(const IronRoot::WatchEvent& event, bool isAtomicWrite) {
        auto& logger = Logger::instance();
        
        // Update stats
        {
            std::lock_guard<std::mutex> lock(statsMutex);
            stats.eventsProcessed++;
            if (isAtomicWrite) {
                stats.atomicWritesDetected++;
            }
        }
        
        // Create extended event
        IronRoot::IronWatchEvent ironEvent;
        ironEvent.type = event.type;
        ironEvent.path = event.path;
        if (event.oldPath.has_value()) {
            ironEvent.oldPath = event.oldPath.value();
        }
        ironEvent.isDirectory = event.isDirectory;
        ironEvent.pid = event.pid;
        ironEvent.processName = event.processName;
        ironEvent.timestamp = std::chrono::steady_clock::now();
        
        // Call user callback
        if (watchCallback) {
            watchCallback(ironEvent);
        }
        
        // Publish to event bus
        if (eventBus) {
            std::string eventType;
            switch (event.type) {
                case IronRoot::WatchEventType::Create:
                    eventType = "FILE_CREATED";
                    break;
                case IronRoot::WatchEventType::Modify:
                    eventType = "FILE_MODIFIED";
                    break;
                case IronRoot::WatchEventType::Delete:
                    eventType = "FILE_DELETED";
                    break;
                case IronRoot::WatchEventType::Rename:
                    eventType = "FILE_RENAMED";
                    break;
                case IronRoot::WatchEventType::AttribChange:
                    eventType = "FILE_ATTRIB_CHANGED";
                    break;
                default:
                    break;
            }
            
            if (!eventType.empty()) {
                logger.log(LogLevel::INFO, "Publishing " + eventType + ": " + event.path +
                           (isAtomicWrite ? " (atomic write)" : "") +
                           (event.pid > 0 ? " [pid:" + std::to_string(event.pid) + " " + event.processName + "]" : ""),
                           "IronRoot");
                eventBus->publish(eventType, event.path);
            }
        }
        
        // Auto-add watch for new directories
        if (event.type == IronRoot::WatchEventType::Create && event.isDirectory && watcher) {
            watcher->addWatchRecursive(event.path);
        }
    }
    
    bool shouldIgnore(const std::string& path) const {
        std::string filename = std::filesystem::path(path).filename().string();
        
        // Check include patterns first (if set, only these are watched)
        if (!config.includePatterns.empty()) {
            bool matched = false;
            for (const auto& pattern : config.includePatterns) {
                if (fnmatch(pattern.c_str(), filename.c_str(), 0) == 0 ||
                    fnmatch(pattern.c_str(), path.c_str(), 0) == 0) {
                    matched = true;
                    break;
                }
            }
            if (!matched) return true;
        }
        
        // Check ignore patterns
        for (const auto& pattern : config.ignorePatterns) {
            if (fnmatch(pattern.c_str(), filename.c_str(), 0) == 0 ||
                fnmatch(pattern.c_str(), path.c_str(), 0) == 0) {
                return true;
            }
        }
        
        // Built-in ignores
        if (path.find("/.git/") != std::string::npos) return true;
        if (path.find("/.svn/") != std::string::npos) return true;
        if (path.find("/node_modules/") != std::string::npos) return true;
        if (path.find("/__pycache__/") != std::string::npos) return true;
        if (filename.back() == '~') return true;
        if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".swp") return true;
        
        return false;
    }
};

// Main class implementation

IronRootPlugin::IronRootPlugin() : impl_(std::make_unique<Impl>()) {
    // Set default config
    impl_->config.recursive = true;
    impl_->config.followSymlinks = false;
    impl_->config.trackXattrs = true;
    impl_->config.useFanotify = true;
    impl_->config.fallbackToInotify = true;
    impl_->config.debounce.window = std::chrono::milliseconds(100);
    impl_->config.debounce.maxDelay = std::chrono::milliseconds(500);
    impl_->config.debounce.coalesceModifies = true;
    impl_->config.debounce.detectAtomicWrites = true;
}

IronRootPlugin::~IronRootPlugin() {
    shutdown();
}

bool IronRootPlugin::initialize(EventBus* eventBus) {
    auto& logger = Logger::instance();
    
    impl_->eventBus = eventBus;
    
    logger.log(LogLevel::INFO, "Initializing IronRoot filesystem plugin", "IronRoot");
    
    // Try fanotify first (requires CAP_SYS_ADMIN)
    bool watcherInitialized = false;
    
    if (impl_->config.useFanotify && IronRoot::FanotifyWatcher::isAvailable()) {
        impl_->watcher = std::make_unique<IronRoot::FanotifyWatcher>();
        watcherInitialized = impl_->watcher->initialize([this](const IronRoot::WatchEvent& ev) {
            impl_->handleWatchEvent(ev);
        });
        
        if (watcherInitialized) {
            impl_->useFanotify = true;
            logger.log(LogLevel::INFO, "Using fanotify (process-aware monitoring)", "IronRoot");
        }
    }
    
    // Fallback to inotify
    if (!watcherInitialized && impl_->config.fallbackToInotify) {
        impl_->watcher = std::make_unique<IronRoot::InotifyWatcher>();
        watcherInitialized = impl_->watcher->initialize([this](const IronRoot::WatchEvent& ev) {
            impl_->handleWatchEvent(ev);
        });
        
        if (watcherInitialized) {
            logger.log(LogLevel::INFO, "Using inotify (fallback)", "IronRoot");
        }
    }
    
    if (!watcherInitialized) {
        logger.log(LogLevel::ERROR, "Failed to initialize any file watcher", "IronRoot");
        return false;
    }
    
    // Initialize debouncer
    impl_->debouncer = std::make_unique<IronRoot::Debouncer>();
    impl_->debouncer->start(impl_->config.debounce, [this](const IronRoot::WatchEvent& ev, bool isAtomic) {
        impl_->handleDebouncedEvent(ev, isAtomic);
    });
    
    logger.log(LogLevel::INFO, "IronRoot initialized successfully", "IronRoot");
    return true;
}

void IronRootPlugin::shutdown() {
    auto& logger = Logger::instance();
    
    logger.log(LogLevel::INFO, "Shutting down IronRoot", "IronRoot");
    
    if (impl_->debouncer) {
        impl_->debouncer->stop();
        impl_->debouncer.reset();
    }
    
    if (impl_->watcher) {
        impl_->watcher->shutdown();
        impl_->watcher.reset();
    }
    
    logger.log(LogLevel::INFO, "IronRoot shut down", "IronRoot");
}

std::vector<uint8_t> IronRootPlugin::readFile(const std::string& path) {
    auto data = IronRoot::FileOps::readFile(path);
    
    std::lock_guard<std::mutex> lock(impl_->statsMutex);
    impl_->stats.bytesRead += data.size();
    
    return data;
}

bool IronRootPlugin::writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    bool result = IronRoot::FileOps::writeFile(path, data);
    
    if (result) {
        std::lock_guard<std::mutex> lock(impl_->statsMutex);
        impl_->stats.bytesWritten += data.size();
    }
    
    return result;
}

void IronRootPlugin::startWatching(const std::string& path) {
    auto& logger = Logger::instance();
    
    if (!impl_->watcher) {
        logger.log(LogLevel::ERROR, "Watcher not initialized", "IronRoot");
        return;
    }
    
    logger.log(LogLevel::INFO, "Starting watch: " + path, "IronRoot");
    
    if (impl_->config.recursive) {
        impl_->watcher->addWatchRecursive(path);
    } else {
        impl_->watcher->addWatch(path);
    }
    
    {
        std::lock_guard<std::mutex> lock(impl_->watchMutex);
        impl_->watchedPaths.insert(path);
    }
    
    // Scan existing files and emit events
    namespace fs = std::filesystem;
    std::error_code ec;
    
    if (fs::is_directory(path, ec)) {
        for (auto it = fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); ++it) {
            if (ec) break;
            
            std::string itemPath = it->path().string();
            if (impl_->shouldIgnore(itemPath)) {
                if (it->is_directory(ec)) {
                    it.disable_recursion_pending();
                }
                continue;
            }
            
            if (it->is_regular_file(ec) && impl_->eventBus) {
                impl_->eventBus->publish("FILE_CREATED", itemPath);
            }
        }
    }
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(impl_->statsMutex);
        impl_->stats.dirsWatched = impl_->watcher->getWatchCount();
    }
}

void IronRootPlugin::stopWatching(const std::string& path) {
    if (!impl_->watcher) return;
    
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Stopping watch: " + path, "IronRoot");
    
    // Remove the main path
    impl_->watcher->removeWatch(path);
    
    // Also remove all subdirectories that were added recursively
    std::string prefix = path;
    if (!prefix.empty() && prefix.back() != '/') {
        prefix += '/';
    }
    
    // Get list of paths to remove (can't modify while iterating)
    std::vector<std::string> pathsToRemove;
    {
        std::lock_guard<std::mutex> lock(impl_->watchMutex);
        for (const auto& watchedPath : impl_->watchedPaths) {
            if (watchedPath == path || watchedPath.find(prefix) == 0) {
                pathsToRemove.push_back(watchedPath);
            }
        }
    }
    
    // Remove all matching watches
    for (const auto& p : pathsToRemove) {
        impl_->watcher->removeWatch(p);
        logger.log(LogLevel::DEBUG, "Removed watch: " + p, "IronRoot");
    }
    
    // Update internal tracking
    {
        std::lock_guard<std::mutex> lock(impl_->watchMutex);
        for (const auto& p : pathsToRemove) {
            impl_->watchedPaths.erase(p);
        }
    }
    
    logger.log(LogLevel::INFO, "Stopped watching " + path + " and " + 
               std::to_string(pathsToRemove.size()) + " subdirectories", "IronRoot");
}

// Extended API

std::vector<uint8_t> IronRootPlugin::readFileMapped(const std::string& path) {
    auto data = IronRoot::FileOps::readFileMapped(path);
    
    std::lock_guard<std::mutex> lock(impl_->statsMutex);
    impl_->stats.bytesRead += data.size();
    
    return data;
}

bool IronRootPlugin::writeFileAtomic(const std::string& path, const std::vector<uint8_t>& data) {
    bool result = IronRoot::FileOps::writeFileAtomic(path, data);
    
    if (result) {
        std::lock_guard<std::mutex> lock(impl_->statsMutex);
        impl_->stats.bytesWritten += data.size();
    }
    
    return result;
}

IronRoot::FileInfo IronRootPlugin::getFileInfo(const std::string& path) {
    return IronRoot::FileOps::getFileInfo(path);
}

std::map<std::string, std::string> IronRootPlugin::getXattrs(const std::string& path) {
    return IronRoot::FileOps::getXattrs(path);
}

bool IronRootPlugin::setXattr(const std::string& path, const std::string& name, const std::string& value) {
    return IronRoot::FileOps::setXattr(path, name, value);
}

bool IronRootPlugin::removeXattr(const std::string& path, const std::string& name) {
    return IronRoot::FileOps::removeXattr(path, name);
}

bool IronRootPlugin::lockFile(const std::string& path, bool exclusive) {
    return IronRoot::FileOps::lockFile(path, exclusive);
}

bool IronRootPlugin::unlockFile(const std::string& path) {
    return IronRoot::FileOps::unlockFile(path);
}

bool IronRootPlugin::isFileLocked(const std::string& path) const {
    return IronRoot::FileOps::isFileLocked(path);
}

void IronRootPlugin::setWatchConfig(const IronRoot::WatchConfig& config) {
    impl_->config = config;
}

IronRoot::WatchConfig IronRootPlugin::getWatchConfig() const {
    return impl_->config;
}

void IronRootPlugin::setWatchCallback(IronRoot::IronWatchCallback callback) {
    impl_->watchCallback = std::move(callback);
}

void IronRootPlugin::setBatchCallback(IronRoot::BatchCallback callback) {
    impl_->batchCallback = std::move(callback);
    // Note: Debouncer uses WatchEvent, not IronWatchEvent
    // Batch callback is handled at plugin level
}

bool IronRootPlugin::hasFanotifySupport() const {
    return impl_->useFanotify;
}

bool IronRootPlugin::hasXattrSupport() const {
    // Check on a common path
    return IronRoot::FileOps::supportsXattr("/tmp");
}

IronRootPlugin::Stats IronRootPlugin::getStats() const {
    std::lock_guard<std::mutex> lock(impl_->statsMutex);
    
    Stats stats = impl_->stats;
    
    if (impl_->debouncer) {
        auto debounceStats = impl_->debouncer->getStats();
        stats.eventsDebounced = debounceStats.eventsCoalesced;
    }
    
    if (impl_->watcher) {
        stats.dirsWatched = impl_->watcher->getWatchCount();
    }
    
    return stats;
}

} // namespace SentinelFS

// Plugin factory

extern "C" {
    SentinelFS::IPlugin* create_plugin() {
        return new SentinelFS::IronRootPlugin();
    }
    
    void destroy_plugin(SentinelFS::IPlugin* plugin) {
        delete plugin;
    }
}
