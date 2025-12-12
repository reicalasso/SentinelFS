#pragma once

#include <string>
#include <functional>
#include <memory>
#include <cstdint>

namespace SentinelFS {

// Forward declarations
class INetworkAPI;
class IStorageAPI;
class IFileAPI;
class DaemonCore;
class AutoRemeshManager;
class FileVersionManager;

/**
 * @brief Context passed to all command handlers
 * 
 * Contains references to all subsystems that commands may need
 */
struct CommandContext {
    INetworkAPI* network{nullptr};
    IStorageAPI* storage{nullptr};
    IFileAPI* filesystem{nullptr};
    DaemonCore* daemonCore{nullptr};
    AutoRemeshManager* autoRemesh{nullptr};
    FileVersionManager* versionManager{nullptr};
    
    // Callbacks
    std::function<void(bool)> syncEnabledCallback;
};

/**
 * @brief Base class for command handler categories
 * 
 * Each derived class handles a specific category of IPC commands
 * (e.g., StatusCommands, PeerCommands, ConfigCommands)
 */
class CommandHandler {
public:
    explicit CommandHandler(CommandContext& ctx) : ctx_(ctx) {}
    virtual ~CommandHandler() = default;
    
    // Disable copy
    CommandHandler(const CommandHandler&) = delete;
    CommandHandler& operator=(const CommandHandler&) = delete;
    
protected:
    CommandContext& ctx_;
    
    // Utility methods available to all handlers
    static std::string formatBytes(uint64_t bytes);
    static std::string formatTime(int64_t timestamp);
    static std::string escapeJson(const std::string& str);
};

} // namespace SentinelFS
