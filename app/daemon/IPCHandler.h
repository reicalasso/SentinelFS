#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "IFileAPI.h"

namespace SentinelFS { class DaemonCore; }

namespace SentinelFS {

/**
 * @brief IPC command handler interface
 * 
 * Manages Unix domain socket for CLI communication.
 * Handles commands like: status, list, pause, resume, bandwidth limits, connect
 */
class IPCHandler {
public:
    /**
     * @brief Constructor
     * @param socketPath Path to Unix domain socket (e.g., /tmp/sentinelfs.sock)
     * @param network Network plugin for connect commands
     * @param storage Storage plugin for status/list commands
     * @param filesystem Filesystem plugin for sync control
     */
    IPCHandler(const std::string& socketPath,
               INetworkAPI* network,
               IStorageAPI* storage,
               IFileAPI* filesystem,
               DaemonCore* daemonCore = nullptr);
    
    ~IPCHandler();
    
    /**
     * @brief Start IPC server thread
     * @return true if server started successfully
     */
    bool start();
    
    /**
     * @brief Stop IPC server and cleanup
     */
    void stop();
    
    /**
     * @brief Check if IPC server is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Set sync enabled/disabled callback
     * 
     * Used for pause/resume commands
     */
    void setSyncEnabledCallback(std::function<void(bool)> callback) {
        syncEnabledCallback_ = callback;
    }
    
private:
    void serverLoop();
    void handleClient(int clientSocket);
    std::string processCommand(const std::string& command);
    
    // Command handlers
    std::string handleStatusCommand();
    std::string handleListCommand();
    std::string handlePauseCommand();
    std::string handleResumeCommand();
    std::string handleConnectCommand(const std::string& args);
    std::string handleUploadLimitCommand(const std::string& args);
    std::string handleDownloadLimitCommand(const std::string& args);
    std::string handleMetricsCommand();
    std::string handleStatsCommand();
    std::string handlePluginsCommand();

    // JSON Command handlers for GUI
    std::string handleStatusJsonCommand();
    std::string handlePeersJsonCommand();
    std::string handleMetricsJsonCommand();
    std::string handleFilesJsonCommand();
    std::string handleActivityJsonCommand();
    std::string handleTransfersJsonCommand();
    std::string handleConfigJsonCommand();
    std::string handleSetConfigCommand(const std::string& args);
    
    std::string socketPath_;
    int serverSocket_{-1};
    std::atomic<bool> running_{false};
    std::thread serverThread_;
    
    // Plugin references (not owned)
    INetworkAPI* network_;
    IStorageAPI* storage_;
    IFileAPI* filesystem_;
    DaemonCore* daemonCore_;
    
    // Callbacks
    std::function<void(bool)> syncEnabledCallback_;
};

} // namespace SentinelFS
