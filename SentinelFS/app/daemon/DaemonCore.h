#pragma once

#include "IPlugin.h"
#include "IStorageAPI.h"
#include "INetworkAPI.h"
#include "IFileAPI.h"
#include "EventBus.h"
#include "PluginManager.h"
#include "DatabaseManager.h"
#include <memory>
#include <atomic>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>

// Forward declaration
namespace SentinelFS {
    class FileVersionManager;
}

namespace SentinelFS {

/**
 * @brief Configuration for daemon startup
 */
struct DaemonConfig {
    int tcpPort = 8000;
    int discoveryPort = 9999;
    int metricsPort = 9100;
    std::string watchDirectory = ".";
    std::string sessionCode;
    bool encryptionEnabled = false;
    size_t uploadLimit = 0;     // 0 = unlimited
    size_t downloadLimit = 0;   // 0 = unlimited
    std::string socketPath;     // Empty = use default
    std::string dbPath;         // Empty = use default
};

/**
 * @brief Core daemon orchestrator
 * 
 * Manages plugin lifecycle, event routing, and main daemon loop.
 * Separated from IPC and event handlers for better modularity.
 */
class DaemonCore {
public:
    DaemonCore(const DaemonConfig& config);
    ~DaemonCore();
    
    /**
     * @brief Initialize all plugins and event bus
     * @return true if successful
     */
    bool initialize();
    
    /**
     * @brief Start daemon main loop (blocking)
     * Handles signals and keeps daemon running
     */
    void run();
    
    /**
     * @brief Graceful shutdown
     */
    void shutdown();
    
    /**
     * @brief Get plugin instances (for IPC/handlers)
     */
    IStorageAPI* getStorage() const { return storage_.get(); }
    INetworkAPI* getNetwork() const { return network_.get(); }
    IFileAPI* getFilesystem() const { return filesystem_.get(); }
    EventBus* getEventBus() { return &eventBus_; }
    DatabaseManager* getDatabase() const { return database_.get(); }
    
    /**
     * @brief Get configuration
     */
    const DaemonConfig& getConfig() const { return config_; }
    
    /**
     * @brief Sync control
     */
    void pauseSync() { syncEnabled_ = false; }
    void resumeSync() { syncEnabled_ = true; }
    bool isSyncEnabled() const { return syncEnabled_; }
    
    /**
     * @brief Check if daemon is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Add a new directory to watch
     * @param path Absolute path to directory
     * @return true if successfully added
     */
    bool addWatchDirectory(const std::string& path);
    
    /**
     * @brief Register a managed thread (will be joined on shutdown)
     * @param thread Thread to manage
     */
    void registerThread(std::thread&& thread);
    
    /**
     * @brief Stop all managed threads gracefully
     */
    void stopAllThreads();

    struct InitializationStatus {
        enum class Result {
            Success,
            PlugInLoadFailure,
            NetworkFailure,
            WatcherFailure,
        } result = Result::Success;
        std::string message;
    };

    const InitializationStatus& getInitializationStatus() const { return initStatus_; }
    
    /**
     * @brief Get plugins with proper naming for new interface
     */
    IStorageAPI* getStoragePlugin() const { return storage_.get(); }
    INetworkAPI* getNetworkPlugin() const { return network_.get(); }
    IFileAPI* getFilesystemPlugin() const { return filesystem_.get(); }
    
    /**
     * @brief Get file version manager
     */
    class FileVersionManager* getVersionManager();
    const class FileVersionManager* getVersionManager() const;
    
    /**
     * @brief Get Zer0 threat detection plugin (as IPlugin)
     */
    IPlugin* getZer0Plugin() const;

private:
    DaemonConfig config_;
    EventBus eventBus_;
    PluginManager pluginManager_;
    
    std::shared_ptr<IStorageAPI> storage_;
    std::shared_ptr<INetworkAPI> network_;
    std::shared_ptr<IFileAPI> filesystem_;
    std::shared_ptr<IPlugin> mlPlugin_;
    IPlugin* zer0Plugin_{nullptr};
    
    // Database manager
    std::unique_ptr<DatabaseManager> database_;
    
    // File version manager
    std::unique_ptr<FileVersionManager> versionManager_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> syncEnabled_{true};

    std::mutex runMutex_;
    std::condition_variable runCv_;
    InitializationStatus initStatus_;
    
    // Thread management
    std::vector<std::thread> managedThreads_;
    std::mutex threadMutex_;
    
    bool loadPlugins();
    void setupEventHandlers();
    void printConfiguration() const;
    void reconnectToKnownPeers();
    void cleanupStalePeers();
};

} // namespace SentinelFS
