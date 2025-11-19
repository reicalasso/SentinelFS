#ifndef SENTINEL_DAEMON_CORE_H
#define SENTINEL_DAEMON_CORE_H

#include "IPlugin.h"
#include "IStorageAPI.h"
#include "INetworkAPI.h"
#include "IFileAPI.h"
#include "EventBus.h"
#include "PluginLoader.h"
#include <memory>
#include <atomic>
#include <string>

namespace SentinelFS {

/**
 * @brief Configuration for daemon startup
 */
struct DaemonConfig {
    int tcpPort = 8000;
    int discoveryPort = 9999;
    std::string watchDirectory = ".";
    std::string sessionCode;
    bool encryptionEnabled = false;
    size_t uploadLimit = 0;     // 0 = unlimited
    size_t downloadLimit = 0;   // 0 = unlimited
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
     * @brief Get plugins with proper naming for new interface
     */
    IStorageAPI* getStoragePlugin() const { return storage_.get(); }
    INetworkAPI* getNetworkPlugin() const { return network_.get(); }
    IFileAPI* getFilesystemPlugin() const { return filesystem_.get(); }

private:
    DaemonConfig config_;
    EventBus eventBus_;
    PluginLoader loader_;
    
    std::shared_ptr<IStorageAPI> storage_;
    std::shared_ptr<INetworkAPI> network_;
    std::shared_ptr<IFileAPI> filesystem_;
    std::shared_ptr<IPlugin> mlPlugin_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> syncEnabled_{true};
    
    bool loadPlugins();
    void setupEventHandlers();
    void printConfiguration() const;
};

} // namespace SentinelFS

#endif // SENTINEL_DAEMON_CORE_H
