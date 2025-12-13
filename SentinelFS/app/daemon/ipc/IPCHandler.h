#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <cstdint>
#include <vector>
#include <map>
#include <mutex>
#include <sys/types.h>
#include <json/json.h>
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "IFileAPI.h"
#include "HealthReport.h"
#include "ipc/commands/CommandHandler.h"
#include "ipc/commands/FalconStoreCommands.h"

namespace SentinelFS {
    class DaemonCore;
    class AutoRemeshManager;
    
    // Forward declare command handlers
    class StatusCommands;
    class PeerCommands;
    class ConfigCommands;
    class FileCommands;
    class TransferCommands;
    class RelayCommands;
}

namespace SentinelFS {

/**
 * @brief IPC socket security configuration
 */
struct IPCSecurityConfig {
    // Socket file permissions (default: 0660 - owner and group read/write)
    mode_t socketPermissions{0660};
    
    // Required group for socket access (empty = process group)
    std::string requiredGroup;
    
    // Whether to verify client UID matches daemon UID
    bool requireSameUid{true};
    
    // Allowed UIDs for connection (empty = same UID only)
    std::vector<uid_t> allowedUids;
    
    // Allowed GIDs for connection (empty = no GID check)
    std::vector<gid_t> allowedGids;
    
    // Maximum concurrent connections
    size_t maxConnections{32};
    
    // Connection timeout in seconds (0 = no timeout)
    int connectionTimeoutSec{300};
    
    // Rate limiting: max commands per minute per client
    size_t maxCommandsPerMinute{120};
    
    // Enable credential logging for audit
    bool auditConnections{false};
    
    // Create parent directories with secure permissions if needed
    bool createParentDirs{true};
    
    // Parent directory permissions (if creating)
    mode_t parentDirPermissions{0750};
};

/**
 * @brief IPC command handler interface
 * 
 * Manages Unix domain socket for CLI communication.
 * Handles commands like: status, list, pause, resume, bandwidth limits, connect
 * 
 * Security features:
 * - Configurable socket file permissions
 * - UID/GID based access control
 * - Rate limiting per client
 * - Connection auditing
 */
class IPCHandler {
public:
    /**
     * @brief Constructor with default security
     * @param socketPath Path to Unix domain socket (e.g., /run/sentinelfs/daemon.sock)
     * @param network Network plugin for connect commands
     * @param storage Storage plugin for status/list commands
     * @param filesystem Filesystem plugin for sync control
     */
    IPCHandler(const std::string& socketPath,
               INetworkAPI* network,
               IStorageAPI* storage,
               IFileAPI* filesystem,
               DaemonCore* daemonCore = nullptr,
               AutoRemeshManager* autoRemesh = nullptr);
    
    /**
     * @brief Constructor with custom security configuration
     * @param socketPath Path to Unix domain socket
     * @param securityConfig Security configuration
     * @param network Network plugin for connect commands
     * @param storage Storage plugin for status/list commands
     * @param filesystem Filesystem plugin for sync control
     */
    IPCHandler(const std::string& socketPath,
               const IPCSecurityConfig& securityConfig,
               INetworkAPI* network,
               IStorageAPI* storage,
               IFileAPI* filesystem,
               DaemonCore* daemonCore = nullptr,
               AutoRemeshManager* autoRemesh = nullptr);
    
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
    std::string sanitizeResponse(const std::string& response);
    
    std::string socketPath_;
    int serverSocket_{-1};
    std::atomic<bool> running_{false};
    std::thread serverThread_;
    
    // Security configuration
    IPCSecurityConfig securityConfig_;
    
    // Rate limiting state
    struct ClientRateLimit {
        time_t windowStart{0};
        size_t commandCount{0};
    };
    std::map<uid_t, ClientRateLimit> clientRateLimits_;
    std::mutex rateLimitMutex_;
    
    // Plugin references (not owned)
    INetworkAPI* network_;
    IStorageAPI* storage_;
    IFileAPI* filesystem_;
    DaemonCore* daemonCore_;
    
    // Callbacks
    std::function<void(bool)> syncEnabledCallback_;
    
    // Auto-remesh manager for peer health metrics
    AutoRemeshManager* autoRemesh_;
    
    // Command context and handlers (modular command processing)
    CommandContext cmdContext_;
    std::unique_ptr<StatusCommands> statusCmds_;
    std::unique_ptr<PeerCommands> peerCmds_;
    std::unique_ptr<ConfigCommands> configCmds_;
    std::unique_ptr<FileCommands> fileCmds_;
    std::unique_ptr<TransferCommands> transferCmds_;
    std::unique_ptr<RelayCommands> relayCmds_;
    std::unique_ptr<FalconStoreCommands> falconstoreCmds_;
    
    // Security helper methods
    bool ensureSocketDirectory();
    bool verifyClientCredentials(int clientSocket, uid_t& clientUid, gid_t& clientGid);
    bool isClientAuthorized(uid_t uid, gid_t gid);
    bool checkRateLimit(uid_t uid);
    void auditConnection(uid_t uid, gid_t gid, bool authorized);
    
    // Initialize command handlers
    void initializeCommandHandlers();
    
    // NetFalcon command handlers
    std::string handleNetFalconStatus();
    std::string handleNetFalconSetStrategy(const std::string& args);
    std::string handleNetFalconSetTransport(const std::string& args);
    
    // FalconStore command handlers
    std::string handleFalconStoreStatus();
    std::string handleFalconStoreStats();
    std::string handleFalconStoreOptimize();
    std::string handleFalconStoreBackup(const std::string& args);
    
    // Helper methods for JSON command handling
    Json::Value parseJsonData(const std::string& args);
    std::string formatJsonResponse(const Json::Value& response);
};

} // namespace SentinelFS
