#include "IPCHandler.h"
#include "DaemonCore.h"
#include "SessionCode.h"
#include "MetricsCollector.h"
#include "PluginManager.h"
#include "AutoRemeshManager.h"
#include "../../plugins/storage/include/SQLiteHandler.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sqlite3.h>
#include <ctime>
#include <filesystem>
#include <chrono>
#include <random>

namespace SentinelFS {

// Helper function to format bytes
static std::string formatBytes(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
    return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
}

IPCHandler::IPCHandler(const std::string& socketPath,
                       INetworkAPI* network,
                       IStorageAPI* storage,
                       IFileAPI* filesystem,
                       DaemonCore* daemonCore,
                       AutoRemeshManager* autoRemesh)
    : socketPath_(socketPath)
    , network_(network)
    , storage_(storage)
    , filesystem_(filesystem)
    , daemonCore_(daemonCore)
    , autoRemesh_(autoRemesh)
{
}

IPCHandler::~IPCHandler() {
    stop();
}

bool IPCHandler::start() {
    if (running_) return true;
    
    // Remove old socket if exists
    unlink(socketPath_.c_str());
    
    // Set restrictive umask BEFORE creating socket to prevent permission race
    // This ensures the socket file is created with restricted permissions from the start
    mode_t oldMask = umask(S_IRWXO);  // Block all permissions for "others"
    
    // Create Unix domain socket
    serverSocket_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        umask(oldMask);  // Restore umask
        std::cerr << "IPC: Cannot create socket" << std::endl;
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);
    
    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        umask(oldMask);  // Restore umask
        std::cerr << "IPC: Cannot bind socket" << std::endl;
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }

    // Restore original umask
    umask(oldMask);

    // Additionally set explicit permissions on socket file for defense in depth
    if (chmod(socketPath_.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) < 0) {
        std::cerr << "IPC: Warning - Failed to set socket file permissions" << std::endl;
        // Continue anyway - umask should have protected us
    }
    
    if (listen(serverSocket_, 5) < 0) {
        std::cerr << "IPC: Cannot listen" << std::endl;
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }
    
    std::cout << "IPC Server listening on " << socketPath_ << std::endl;
    
    running_ = true;
    serverThread_ = std::thread(&IPCHandler::serverLoop, this);
    
    return true;
}

void IPCHandler::stop() {
    if (!running_) return;
    
    running_ = false;
    
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    unlink(socketPath_.c_str());
}

void IPCHandler::serverLoop() {
    while (running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket_, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(serverSocket_ + 1, &readfds, NULL, NULL, &tv);
        if (activity <= 0) continue;
        
        int clientSocket = accept(serverSocket_, NULL, NULL);
        if (clientSocket < 0) continue;
        
        // Spawn thread for persistent client connection
        std::thread([this, clientSocket]() {
            handleClient(clientSocket);
            close(clientSocket);
        }).detach();
    }
}

void IPCHandler::handleClient(int clientSocket) {
    struct ucred cred;
    socklen_t credLen = sizeof(cred);
    if (getsockopt(clientSocket, SOL_SOCKET, SO_PEERCRED, &cred, &credLen) == 0) {
        // Only allow same UID by default
        if (cred.uid != geteuid()) {
            const char* msg = "Unauthorized IPC client\n";
            send(clientSocket, msg, strlen(msg), 0);
            return;
        }
    }
    
    // Persistent connection: keep reading commands until client disconnects
    char buffer[1024];
    std::string lineBuffer;
    
    while (running_) {
        ssize_t n = recv(clientSocket, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        
        if (n > 0) {
            buffer[n] = '\0';
            lineBuffer += buffer;
            
            // Process complete lines (commands end with \n)
            size_t pos;
            while ((pos = lineBuffer.find('\n')) != std::string::npos) {
                std::string command = lineBuffer.substr(0, pos);
                lineBuffer.erase(0, pos + 1);
                
                if (!command.empty()) {
                    std::string response = processCommand(command);
                    send(clientSocket, response.c_str(), response.length(), 0);
                }
            }
        } else if (n == 0) {
            // Client disconnected
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Real error
            break;
        }
        
        // Small sleep to avoid busy-wait
        usleep(10000); // 10ms
    }
}

std::string IPCHandler::processCommand(const std::string& command) {
    // Parse command and arguments
    std::string cmd = command;
    std::string args;
    
    size_t spacePos = command.find(' ');
    if (spacePos != std::string::npos) {
        cmd = command.substr(0, spacePos);
        args = command.substr(spacePos + 1);
    }
    
    // Route to appropriate handler
    if (cmd == "STATUS") {
        return handleStatusCommand();
    } else if (cmd == "PLUGINS") {
        return handlePluginsCommand();
    } else if (cmd == "PEERS") {
        return handleListCommand();
    } else if (cmd == "PAUSE") {
        return handlePauseCommand();
    } else if (cmd == "RESUME") {
        return handleResumeCommand();
    } else if (cmd == "CONNECT") {
        return handleConnectCommand(args);
    } else if (cmd == "ADD_PEER") {
        return handleAddPeerCommand(args);
    } else if (cmd == "UPLOAD-LIMIT") {
        return handleUploadLimitCommand(args);
    } else if (cmd == "DOWNLOAD-LIMIT") {
        return handleDownloadLimitCommand(args);
    } else if (cmd == "METRICS") {
        return handleMetricsCommand();
    } else if (cmd == "STATUS_JSON") {
        return handleStatusJsonCommand();
    } else if (cmd == "PEERS_JSON") {
        return handlePeersJsonCommand();
    } else if (cmd == "METRICS_JSON") {
        return handleMetricsJsonCommand();
    } else if (cmd == "FILES_JSON") {
        return handleFilesJsonCommand();
    } else if (cmd == "ACTIVITY_JSON") {
        return handleActivityJsonCommand();
    } else if (cmd == "TRANSFERS_JSON") {
        return handleTransfersJsonCommand();
    } else if (cmd == "CONFIG_JSON") {
        return handleConfigJsonCommand();
    } else if (cmd == "SET_CONFIG") {
        return handleSetConfigCommand(args);
    } else if (cmd == "CONFLICTS_JSON") {
        return handleConflictsJsonCommand();
    } else if (cmd == "SYNC_QUEUE_JSON") {
        return handleSyncQueueJsonCommand();
    } else if (cmd == "EXPORT_CONFIG") {
        return handleExportConfigCommand();
    } else if (cmd == "IMPORT_CONFIG") {
        return handleImportConfigCommand(args);
    } else if (cmd == "ADD_IGNORE") {
        return handleAddIgnoreCommand(args);
    } else if (cmd == "REMOVE_IGNORE") {
        return handleRemoveIgnoreCommand(args);
    } else if (cmd == "LIST_IGNORE") {
        return handleListIgnoreCommand();
    } else if (cmd == "RESOLVE_CONFLICT") {
        return handleResolveConflictCommand(args);
    } else if (cmd == "BLOCK_PEER") {
        return handleBlockPeerCommand(args);
    } else if (cmd == "UNBLOCK_PEER") {
        return handleUnblockPeerCommand(args);
    } else if (cmd == "CLEAR_PEERS") {
        // Clear all stale peers from database
        if (storage_) {
            sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
            sqlite3_exec(db, "DELETE FROM peers", nullptr, nullptr, nullptr);
            return "Success: All peers cleared from database\n";
        }
        return "Error: Storage not initialized\n";
    } else if (cmd == "STATS") {
        return handleStatsCommand();
    } else if (cmd == "CONFLICTS") {
        // List conflicts
        auto conflicts = storage_->getUnresolvedConflicts();
        std::stringstream ss;
        ss << "=== File Conflicts ===\n";
        
        if (conflicts.empty()) {
            ss << "No conflicts detected. ✓\n";
        } else {
            ss << "Found " << conflicts.size() << " unresolved conflict(s):\n\n";
            for (const auto& c : conflicts) {
                ss << "  ID: " << c.id << "\n";
                ss << "  File: " << c.path << "\n";
                ss << "  Remote Peer: " << c.remotePeerId << "\n";
                ss << "  Local: " << c.localSize << " bytes @ " << c.localTimestamp << "\n";
                ss << "  Remote: " << c.remoteSize << " bytes @ " << c.remoteTimestamp << "\n";
                ss << "  Strategy: " << c.strategy << "\n";
                ss << "  ---\n";
            }
        }
        
        auto [total, unresolved] = storage_->getConflictStats();
        ss << "\nTotal conflicts: " << total << " (Unresolved: " << unresolved << ")\n";
        return ss.str();
        
    } else if (cmd == "RESOLVE") {
        // Resolve conflict by ID
        try {
            int conflictId = std::stoi(args);
            if (storage_->markConflictResolved(conflictId)) {
                return "Conflict " + std::to_string(conflictId) + " marked as resolved.\n";
            } else {
                return "Failed to resolve conflict " + std::to_string(conflictId) + ".\n";
            }
        } catch (const std::invalid_argument&) {
            return "Invalid conflict ID.\n";
        } catch (const std::out_of_range&) {
            return "Conflict ID out of range.\n";
        }
    } else if (cmd == "ADD_FOLDER") {
        if (args.empty()) {
            return "Error: No folder path provided\n";
        }
        
        if (!daemonCore_) {
            return "Error: Daemon core not initialized\n";
        }
        
        // Sanitize path: remove file:// prefix and fix unicode slashes
        std::string cleanPath = args;
        if (cleanPath.find("file://") == 0) {
            cleanPath = cleanPath.substr(7);
        }
        // Replace unicode fraction slash (⁄ U+2044) with regular slash
        size_t pos = 0;
        while ((pos = cleanPath.find("\xe2\x81\x84", pos)) != std::string::npos) {
            cleanPath.replace(pos, 3, "/");
        }
        
        // Add the folder to watch list
        if (daemonCore_->addWatchDirectory(cleanPath)) {
            return "Success: Folder added to watch list: " + cleanPath + "\n";
        } else {
            return "Error: Failed to add folder to watch list: " + cleanPath + "\n";
        }
    } else if (cmd == "REMOVE_WATCH") {
        if (args.empty()) {
            return "Error: No path provided\n";
        }
        
        if (!storage_) {
            return "Error: Storage not initialized\n";
        }
        
        // Sanitize path
        std::string cleanPath = args;
        if (cleanPath.find("file://") == 0) {
            cleanPath = cleanPath.substr(7);
        }
        size_t pos = 0;
        while ((pos = cleanPath.find("\xe2\x81\x84", pos)) != std::string::npos) {
            cleanPath.replace(pos, 3, "/");
        }
        
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        sqlite3_stmt* stmt;
        
        // Count files that will be removed from monitoring (but NOT deleted)
        std::string folderPrefix = cleanPath;
        if (!folderPrefix.empty() && folderPrefix.back() != '/') {
            folderPrefix += '/';
        }
        
        int fileCount = 0;
        const char* countSql = "SELECT COUNT(*) FROM files WHERE path LIKE ?";
        if (sqlite3_prepare_v2(db, countSql, -1, &stmt, nullptr) == SQLITE_OK) {
            std::string pattern = folderPrefix + "%";
            sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                fileCount = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        
        // Remove files from database (but NOT from disk)
        const char* deleteFilesSql = "DELETE FROM files WHERE path LIKE ?";
        if (sqlite3_prepare_v2(db, deleteFilesSql, -1, &stmt, nullptr) == SQLITE_OK) {
            std::string pattern = folderPrefix + "%";
            sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        
        // Remove watched folder from database
        const char* deleteFolderSql = "DELETE FROM watched_folders WHERE path = ?";
        if (sqlite3_prepare_v2(db, deleteFolderSql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, cleanPath.c_str(), -1, SQLITE_TRANSIENT);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                sqlite3_finalize(stmt);
                
                // Stop filesystem watcher
                if (filesystem_) {
                    filesystem_->stopWatching(cleanPath);
                }
                
                // Files removed from database but remain on disk
                return "Success: Stopped watching " + cleanPath + " (" + 
                       std::to_string(fileCount) + " files remain on disk and will no longer be monitored)\n";
            }
            sqlite3_finalize(stmt);
        }
        
        return "Error: Failed to remove watch for: " + cleanPath + "\n";
    } else if (cmd == "DISCOVER") {
        if (!network_ || !daemonCore_) {
            return "Error: Network subsystem not ready.\n";
        }

        const auto& cfg = daemonCore_->getConfig();
        network_->broadcastPresence(cfg.discoveryPort, cfg.tcpPort);
        return "Discovery broadcast sent.\n";
    } else if (cmd == "SET_DISCOVERY") {
        // SET_DISCOVERY udp=1 or SET_DISCOVERY tcp=0
        // args contains "udp=1" or "tcp=0"
        std::string setting = args;
        
        // Parse key=value
        size_t eq = setting.find('=');
        if (eq == std::string::npos) {
            return "Error: Invalid format. Use SET_DISCOVERY key=value\n";
        }
        
        std::string key = setting.substr(0, eq);
        std::string value = setting.substr(eq + 1);
        bool enabled = (value == "1" || value == "true");
        
        if (key == "udp") {
            // UDP discovery is always enabled in current implementation
            return "OK: UDP discovery " + std::string(enabled ? "enabled" : "disabled") + " (note: UDP is always active)\n";
        } else if (key == "tcp") {
            // Enable/disable TCP relay for NAT traversal
            if (network_) {
                network_->setRelayEnabled(enabled);
                bool connected = network_->isRelayConnected();
                std::string status = enabled 
                    ? (connected ? "enabled and connected" : "enabled (connecting...)")
                    : "disabled";
                return "OK: TCP relay " + status + "\n";
            }
            return "Error: Network subsystem not ready\n";
        }
        
        return "Error: Unknown discovery setting: " + key + "\n";
    } else if (cmd == "GET_RELAY_STATUS") {
        // Get relay connection status
        if (!network_) {
            return "Error: Network subsystem not ready\n";
        }
        
        bool enabled = network_->isRelayEnabled();
        bool connected = network_->isRelayConnected();
        
        std::stringstream ss;
        ss << "{\"enabled\":" << (enabled ? "true" : "false") << ",";
        ss << "\"connected\":" << (connected ? "true" : "false") << "}\n";
        return ss.str();
    } else if (cmd == "LIST_IGNORE") {
        // List ignore patterns
        if (!storage_) {
            return "Error: Storage not initialized\n";
        }
        
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        sqlite3_stmt* stmt;
        const char* sql = "SELECT pattern, type FROM ignore_patterns WHERE active = 1";
        
        std::stringstream ss;
        ss << "{\"patterns\":[";
        bool first = true;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                
                const char* pattern = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                const char* type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                
                ss << "{\"pattern\":\"" << (pattern ? pattern : "") << "\",";
                ss << "\"type\":\"" << (type ? type : "glob") << "\"}";
            }
            sqlite3_finalize(stmt);
        }
        ss << "]}\n";
        return ss.str();
        
    } else if (cmd == "ADD_IGNORE") {
        // Add ignore pattern
        if (args.empty()) {
            return "Error: No pattern provided\n";
        }
        
        if (!storage_) {
            return "Error: Storage not initialized\n";
        }
        
        // Normalize pattern: if it's an absolute path inside watch directory, make it relative
        std::string pattern = args;
        if (daemonCore_) {
            std::string watchDir = daemonCore_->getConfig().watchDirectory;
            // Ensure watchDir has consistent slash
            if (watchDir.back() != '/') watchDir += '/';
            
            // Check if pattern starts with watchDir
            if (pattern.find(watchDir) == 0) {
                pattern = pattern.substr(watchDir.length());
            } else if (pattern.find("file://" + watchDir) == 0) {
                pattern = pattern.substr(7 + watchDir.length());
            }
            
            // Remove leading slash if present
            if (!pattern.empty() && pattern[0] == '/') {
                pattern = pattern.substr(1);
            }
        }
        
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        sqlite3_stmt* stmt;
        const char* sql = "INSERT OR REPLACE INTO ignore_patterns (pattern, type, active) VALUES (?, 'glob', 1)";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                sqlite3_finalize(stmt);
                return "Success: Added ignore pattern: " + pattern + "\n";
            }
            sqlite3_finalize(stmt);
        }
        return "Error: Failed to add ignore pattern\n";
        
    } else if (cmd == "REMOVE_IGNORE") {
        // Remove ignore pattern
        if (args.empty()) {
            return "Error: No pattern provided\n";
        }
        
        if (!storage_) {
            return "Error: Storage not initialized\n";
        }
        
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        sqlite3_stmt* stmt;
        const char* sql = "DELETE FROM ignore_patterns WHERE pattern = ?";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, args.c_str(), -1, SQLITE_TRANSIENT);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                sqlite3_finalize(stmt);
                return "Success: Removed ignore pattern: " + args + "\n";
            }
            sqlite3_finalize(stmt);
        }
        return "Error: Failed to remove ignore pattern\n";
        
    } else if (cmd == "SET_ENCRYPTION") {
        // Enable/disable encryption
        bool enable = (args == "true" || args == "1" || args == "on");
        if (network_) {
            network_->setEncryptionEnabled(enable);
            return enable ? "Encryption enabled.\n" : "Encryption disabled.\n";
        }
        return "Error: Network not initialized\n";
        
    } else if (cmd == "SET_SESSION_CODE") {
        // Set session code
        if (args.length() != 6) {
            return "Error: Session code must be 6 characters\n";
        }
        if (network_) {
            network_->setSessionCode(args);
            return "Session code set: " + args + "\n";
        }
        return "Error: Network not initialized\n";
        
    } else if (cmd == "GENERATE_CODE") {
        // Generate a random 6-character session code
        static const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // Avoiding confusing chars like 0/O, 1/I
        std::string code;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
        for (int i = 0; i < 6; i++) {
            code += chars[dis(gen)];
        }
        if (network_) {
            network_->setSessionCode(code);
            return "CODE:" + code + "\n";
        }
        return "Error: Network plugin not initialized.\n";
    } else {
        std::stringstream ss;
        ss << "Unknown command: " << cmd << "\n";
        ss << "Available commands: STATUS, PEERS, PAUSE, RESUME, CONNECT, ";
        ss << "CONFLICTS, RESOLVE, UPLOAD-LIMIT, DOWNLOAD-LIMIT\n";
        return ss.str();
    }
}

std::string IPCHandler::handleStatusCommand() {
    std::stringstream ss;
    
    ss << "=== SentinelFS Daemon Status ===\n";
    if (daemonCore_) {
        ss << "Sync Status: " << (daemonCore_->isSyncEnabled() ? "ENABLED" : "PAUSED") << "\n";
    } else {
        ss << "Sync Status: UNKNOWN\n";
    }
    ss << "Encryption: " << (network_->isEncryptionEnabled() ? "ENABLED 🔒" : "Disabled") << "\n";
    
    std::string code = network_->getSessionCode();
    if (!code.empty()) {
        ss << "Session Code: " << SessionCode::format(code) << " ✓\n";
    } else {
        ss << "Session Code: Not set ⚠️\n";
    }
    
    auto peers = storage_->getAllPeers();
    ss << "Connected Peers: " << peers.size() << "\n";
    
    return ss.str();
}

std::string IPCHandler::handlePluginsCommand() {
    if (!daemonCore_) {
        return "Plugin status unavailable.\n";
    }

    std::stringstream ss;
    ss << "=== Plugin Status ===\n";

    // Get plugin statuses from DaemonCore's PluginManager
    // Since we don't have direct access, we'll report what we know
    ss << "Storage: " << (storage_ ? "LOADED ✓" : "FAILED ✗") << "\n";
    ss << "Network: " << (network_ ? "LOADED ✓" : "FAILED ✗") << "\n";
    ss << "Filesystem: " << (filesystem_ ? "LOADED ✓" : "FAILED ✗") << "\n";
    ss << "ML: " << (daemonCore_->getConfig().uploadLimit >= 0 ? "Optional" : "Optional") << "\n";

    return ss.str();
}

std::string IPCHandler::handleListCommand() {
    std::stringstream ss;
    
    auto sortedPeers = storage_->getPeersByLatency();
    ss << "=== Discovered Peers ===\n";
    
    if (sortedPeers.empty()) {
        ss << "No peers discovered yet.\n";
    } else {
        for (const auto& peer : sortedPeers) {
            ss << peer.id << " @ " << peer.ip << ":" << peer.port;
            if (peer.latency >= 0) {
                ss << " [" << peer.latency << "ms]";
            }
            ss << " (" << peer.status << ")\n";
        }
    }
    
    return ss.str();
}

std::string IPCHandler::handlePauseCommand() {
    if (syncEnabledCallback_) {
        syncEnabledCallback_(false);
        return "Synchronization PAUSED.\n";
    }
    return "Pause callback not set.\n";
}

std::string IPCHandler::handleResumeCommand() {
    if (syncEnabledCallback_) {
        syncEnabledCallback_(true);
        return "Synchronization RESUMED.\n";
    }
    return "Resume callback not set.\n";
}

std::string IPCHandler::handleConnectCommand(const std::string& args) {
    // Parse IP:PORT
    size_t colonPos = args.find(':');
    if (colonPos == std::string::npos) {
        return "Invalid format. Use: CONNECT <ip>:<port>\n";
    }
    
    std::string ip = args.substr(0, colonPos);
    int port;
    try {
        port = std::stoi(args.substr(colonPos + 1));
    } catch (const std::invalid_argument&) {
        return "Invalid port number.\n";
    } catch (const std::out_of_range&) {
        return "Port number out of range.\n";
    }
    
    if (network_->connectToPeer(ip, port)) {
        return "Connecting to " + ip + ":" + std::to_string(port) + "...\n";
    } else {
        return "Failed to initiate connection.\n";
    }
}

std::string IPCHandler::handleAddPeerCommand(const std::string& args) {
    // Parse IP:PORT
    size_t colonPos = args.find(':');
    if (colonPos == std::string::npos) {
        return "Error: Invalid format. Use: ADD_PEER <ip>:<port>\n";
    }
    
    std::string ip = args.substr(0, colonPos);
    int port;
    try {
        port = std::stoi(args.substr(colonPos + 1));
    } catch (const std::invalid_argument&) {
        return "Error: Invalid port number.\n";
    } catch (const std::out_of_range&) {
        return "Error: Port number out of range.\n";
    }
    
    if (!network_) {
        return "Error: Network subsystem not initialized.\n";
    }
    
    if (network_->connectToPeer(ip, port)) {
        return "Success: Connecting to peer " + ip + ":" + std::to_string(port) + "...\n";
    } else {
        return "Error: Failed to initiate connection to " + ip + ":" + std::to_string(port) + "\n";
    }
}

std::string IPCHandler::handleUploadLimitCommand(const std::string& args) {
    std::string trimmed = args;
    auto first = trimmed.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "Usage: UPLOAD-LIMIT <KB/s>\n";
    }
    trimmed = trimmed.substr(first);

    try {
        long long kb = std::stoll(trimmed);
        if (kb < 0) {
            return "Upload limit must be >= 0 KB/s.\n";
        }

        std::size_t bytesPerSecond = static_cast<std::size_t>(kb) * 1024;
        network_->setGlobalUploadLimit(bytesPerSecond);

        if (bytesPerSecond == 0) {
            return "Global upload limit set to unlimited.\n";
        }

        std::ostringstream ss;
        ss << "Global upload limit set to " << kb << " KB/s.\n";
        return ss.str();
    } catch (const std::invalid_argument&) {
        return "Invalid upload limit. Usage: UPLOAD-LIMIT <KB/s>\n";
    } catch (const std::out_of_range&) {
        return "Upload limit value out of range.\n";
    }
}

std::string IPCHandler::handleDownloadLimitCommand(const std::string& args) {
    std::string trimmed = args;
    auto first = trimmed.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "Usage: DOWNLOAD-LIMIT <KB/s>\n";
    }
    trimmed = trimmed.substr(first);

    try {
        long long kb = std::stoll(trimmed);
        if (kb < 0) {
            return "Download limit must be >= 0 KB/s.\n";
        }

        std::size_t bytesPerSecond = static_cast<std::size_t>(kb) * 1024;
        network_->setGlobalDownloadLimit(bytesPerSecond);

        if (bytesPerSecond == 0) {
            return "Global download limit set to unlimited.\n";
        }

        std::ostringstream ss;
        ss << "Global download limit set to " << kb << " KB/s.\n";
        return ss.str();
    } catch (const std::invalid_argument&) {
        return "Invalid download limit. Usage: DOWNLOAD-LIMIT <KB/s>\n";
    } catch (const std::out_of_range&) {
        return "Download limit value out of range.\n";
    }
}

std::string IPCHandler::handleMetricsCommand() {
    auto& metrics = MetricsCollector::instance();
    std::string summary = metrics.getMetricsSummary();
    summary += "\n--- Bandwidth Limiter ---\n";
    summary += network_->getBandwidthStats();
    summary += "\n";
    return summary;
}

std::string IPCHandler::handleStatsCommand() {
    auto& metrics = MetricsCollector::instance();
    auto networkMetrics = metrics.getNetworkMetrics();
    auto syncMetrics = metrics.getSyncMetrics();
    
    std::stringstream ss;
    ss << "=== Transfer Statistics ===\n";
    
    double uploadMB = networkMetrics.bytesUploaded / (1024.0 * 1024.0);
    double downloadMB = networkMetrics.bytesDownloaded / (1024.0 * 1024.0);
    
    ss << "Uploaded: " << std::fixed << std::setprecision(2) << uploadMB << " MB\n";
    ss << "Downloaded: " << downloadMB << " MB\n";
    ss << "Files Synced: " << syncMetrics.filesSynced << "\n";
    ss << "Deltas Sent: " << networkMetrics.deltasSent << "\n";
    ss << "Deltas Received: " << networkMetrics.deltasReceived << "\n";
    ss << "Transfers Completed: " << networkMetrics.transfersCompleted << "\n";
    ss << "Transfers Failed: " << networkMetrics.transfersFailed << "\n";
    
    return ss.str();
}

std::string IPCHandler::handleStatusJsonCommand() {
    std::stringstream ss;
    ss << "{";
    ss << "\"syncStatus\": \"" << (daemonCore_ && daemonCore_->isSyncEnabled() ? "ENABLED" : "PAUSED") << "\",";
    ss << "\"encryption\": " << (network_->isEncryptionEnabled() ? "true" : "false") << ",";
    ss << "\"sessionCode\": \"" << network_->getSessionCode() << "\",";
    ss << "\"peerCount\": " << storage_->getAllPeers().size() << ",";
    
    // Anomaly report
    auto anomaly = getAnomalyReport();
    ss << "\"anomaly\": {";
    ss << "\"score\": " << anomaly.score << ",";
    ss << "\"lastType\": \"" << anomaly.lastType << "\",";
    ss << "\"lastDetectedAt\": " << anomaly.lastDetectedAt;
    ss << "},";
    
    // Peer health reports with degradation flags
    auto peerHealth = computePeerHealthReports();
    ss << "\"peerHealth\": [";
    for (size_t i = 0; i < peerHealth.size(); ++i) {
        const auto& ph = peerHealth[i];
        ss << "{";
        ss << "\"peerId\": \"" << ph.peerId << "\",";
        ss << "\"avgRttMs\": " << ph.avgRttMs << ",";
        ss << "\"jitterMs\": " << ph.jitterMs << ",";
        ss << "\"packetLossPercent\": " << ph.packetLossPercent << ",";
        ss << "\"degraded\": " << (ph.degraded ? "true" : "false");
        ss << "}";
        if (i < peerHealth.size() - 1) ss << ",";
    }
    ss << "],";
    
    // Health summary
    auto health = computeHealthSummary();
    ss << "\"health\": {";
    ss << "\"diskTotalBytes\": " << health.diskTotalBytes << ",";
    ss << "\"diskFreeBytes\": " << health.diskFreeBytes << ",";
    ss << "\"diskUsagePercent\": " << health.diskUsagePercent << ",";
    ss << "\"dbConnected\": " << (health.dbConnected ? "true" : "false") << ",";
    ss << "\"dbSizeBytes\": " << health.dbSizeBytes << ",";
    ss << "\"activeWatcherCount\": " << health.activeWatcherCount << ",";
    ss << "\"healthy\": " << (health.healthy ? "true" : "false") << ",";
    ss << "\"statusMessage\": \"" << health.statusMessage << "\"";
    ss << "}";
    
    ss << "}\n";
    return ss.str();
}

std::string IPCHandler::handlePeersJsonCommand() {
    std::stringstream ss;
    auto sortedPeers = storage_->getPeersByLatency();
    
    // Get local peer info to filter out ourselves
    std::string localPeerId;
    int localPort = 0;
    if (network_) {
        localPeerId = network_->getLocalPeerId();
        localPort = network_->getLocalPort();
    }
    
    ss << "{\"peers\": [";
    bool first = true;
    for (size_t i = 0; i < sortedPeers.size(); ++i) {
        const auto& p = sortedPeers[i];
        
        // Skip our own peer ID
        if (p.id == localPeerId) {
            continue;
        }
        
        // Skip peers on our own port (different sessions on same machine)
        // This catches cases where peer ID changed but same IP:port
        if (p.port == localPort && (p.ip == "127.0.0.1" || p.ip == "localhost")) {
            continue;
        }
        
        if (!first) ss << ",";
        first = false;
        
        ss << "{";
        ss << "\"id\": \"" << p.id << "\",";
        ss << "\"ip\": \"" << p.ip << "\",";
        ss << "\"port\": " << p.port << ",";
        ss << "\"latency\": " << p.latency << ",";
        ss << "\"status\": \"" << p.status << "\"";
        ss << "}";
    }
    ss << "]}\n";
    return ss.str();
}

std::string IPCHandler::handleMetricsJsonCommand() {
    auto& metrics = MetricsCollector::instance();
    auto netMetrics = metrics.getNetworkMetrics();
    
    std::stringstream ss;
    ss << "{";
    ss << "\"totalUploaded\": " << netMetrics.bytesUploaded << ",";
    ss << "\"totalDownloaded\": " << netMetrics.bytesDownloaded << ",";
    ss << "\"filesSynced\": " << metrics.getSyncMetrics().filesSynced;
    ss << "}\n";
    return ss.str();
}

std::string IPCHandler::handleFilesJsonCommand() {
    std::stringstream ss;
    ss << "{\"files\": [";
    
    if (storage_) {
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        bool first = true;
        std::vector<std::string> watchedFolders;
        
        // 1. Get watched folders first (Roots)
        const char* folderSql = "SELECT path FROM watched_folders WHERE status = 'active'";
        sqlite3_stmt* folderStmt;
        
        if (sqlite3_prepare_v2(db, folderSql, -1, &folderStmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(folderStmt) == SQLITE_ROW) {
                const char* folderPath = reinterpret_cast<const char*>(sqlite3_column_text(folderStmt, 0));
                if (folderPath) {
                    std::string pathStr = folderPath;
                    watchedFolders.push_back(pathStr);
                    
                    // Calculate folder size from files table
                    int64_t folderSize = 0;
                    std::string sizeSql = "SELECT COALESCE(SUM(size), 0) FROM files WHERE path LIKE ?";
                    sqlite3_stmt* sizeStmt;
                    if (sqlite3_prepare_v2(db, sizeSql.c_str(), -1, &sizeStmt, nullptr) == SQLITE_OK) {
                        std::string pattern = pathStr + "/%";
                        sqlite3_bind_text(sizeStmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
                        if (sqlite3_step(sizeStmt) == SQLITE_ROW) {
                            folderSize = sqlite3_column_int64(sizeStmt, 0);
                        }
                        sqlite3_finalize(sizeStmt);
                    }
                    
                    if (!first) ss << ",";
                    first = false;
                    
                    ss << "{";
                    ss << "\"path\":\"" << pathStr << "\",";
                    ss << "\"hash\":\"\",";
                    ss << "\"size\":" << folderSize << ",";
                    ss << "\"lastModified\":" << std::time(nullptr) << ",";
                    ss << "\"syncStatus\":\"watching\",";
                    ss << "\"isFolder\":true";
                    ss << "}";
                }
            }
            sqlite3_finalize(folderStmt);
        }

        // 2. Get all files from the files table
        const char* sql = "SELECT path, hash, timestamp, size, synced FROM files ORDER BY timestamp DESC LIMIT 1000";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (!path) continue;
                
                std::string pathStr = path;
                
                // Determine parent folder
                std::string parent = "";
                // Find the longest matching watched folder
                size_t maxLen = 0;
                
                for (const auto& folder : watchedFolders) {
                    if (pathStr.find(folder) == 0) { // Starts with folder path
                        // Ensure exact match or directory boundary
                        if (pathStr.length() == folder.length() || pathStr[folder.length()] == std::filesystem::path::preferred_separator) {
                             if (folder.length() > maxLen) {
                                 maxLen = folder.length();
                                 parent = folder;
                             }
                        }
                    }
                }
                
                // Skip files that don't belong to any active watched folder
                if (parent.empty()) {
                    continue;
                }
                
                if (!first) ss << ",";
                first = false;
                
                const char* hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                int64_t timestamp = sqlite3_column_int64(stmt, 2);
                int64_t size = sqlite3_column_int64(stmt, 3);
                int synced = sqlite3_column_int(stmt, 4);
                
                ss << "{";
                ss << "\"path\":\"" << pathStr << "\",";
                ss << "\"hash\":\"" << (hash ? hash : "") << "\",";
                ss << "\"size\":" << size << ",";
                ss << "\"lastModified\":" << timestamp << ",";
                ss << "\"syncStatus\":\"" << (synced ? "synced" : "pending") << "\"";
                if (!parent.empty()) {
                    ss << ",\"parent\":\"" << parent << "\"";
                }
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string IPCHandler::handleActivityJsonCommand() {
    std::stringstream ss;
    ss << "{\"activity\": [";
    bool first = true;
    
    // First, get recent file sync activities from files table
    if (storage_) {
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        
        // Get recently synced files
        const char* filesSql = "SELECT path, timestamp, synced FROM files ORDER BY timestamp DESC LIMIT 10";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, filesSql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                long long timestamp = sqlite3_column_int64(stmt, 1);
                int synced = sqlite3_column_int(stmt, 2);
                
                std::string pathStr = path ? path : "";
                std::filesystem::path p(pathStr);
                std::string filename = p.filename().string();
                if (filename.empty()) filename = pathStr;
                
                // Format time relative
                auto now = std::time(nullptr);
                auto diff = now - timestamp;
                std::string timeStr;
                if (diff < 60) timeStr = "Just now";
                else if (diff < 3600) timeStr = std::to_string(diff / 60) + " mins ago";
                else if (diff < 86400) timeStr = std::to_string(diff / 3600) + " hours ago";
                else timeStr = std::to_string(diff / 86400) + " days ago";
                
                ss << "{";
                ss << "\"type\":\"" << (synced ? "sync" : "modified") << "\",";
                ss << "\"file\":\"" << filename << "\",";
                ss << "\"time\":\"" << timeStr << "\",";
                ss << "\"details\":\"" << (synced ? "File synced" : "File modified") << "\"";
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
        
        // Also include watched folders
        const char* foldersSql = "SELECT path, added_at FROM watched_folders WHERE status = 'active' ORDER BY added_at DESC LIMIT 3";
        
        if (sqlite3_prepare_v2(db, foldersSql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                long long addedAt = sqlite3_column_int64(stmt, 1);
                
                std::string pathStr = path ? path : "";
                std::filesystem::path p(pathStr);
                std::string filename = p.filename().string();
                if (filename.empty()) filename = pathStr;
                
                // Format time relative
                auto now = std::time(nullptr);
                auto diff = now - addedAt;
                std::string timeStr;
                if (diff < 60) timeStr = "Just now";
                else if (diff < 3600) timeStr = std::to_string(diff / 60) + " mins ago";
                else if (diff < 86400) timeStr = std::to_string(diff / 3600) + " hours ago";
                else timeStr = std::to_string(diff / 86400) + " days ago";
                
                ss << "{";
                ss << "\"type\":\"folder\",";
                ss << "\"file\":\"" << filename << "\",";
                ss << "\"time\":\"" << timeStr << "\",";
                ss << "\"details\":\"Folder watching started\"";
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string IPCHandler::handleTransfersJsonCommand() {
    std::stringstream ss;
    ss << "{\"transfers\": [";
    bool first = true;
    
    // First, get active transfers from MetricsCollector (real-time)
    auto& metrics = MetricsCollector::instance();
    auto activeTransfers = metrics.getActiveTransfers();
    
    for (const auto& transfer : activeTransfers) {
        if (!first) ss << ",";
        first = false;
        
        std::filesystem::path p(transfer.filePath);
        std::string filename = p.filename().string();
        if (filename.empty()) filename = transfer.filePath;
        
        ss << "{";
        ss << "\"file\":\"" << filename << "\",";
        ss << "\"peer\":\"" << transfer.peerId << "\",";
        ss << "\"type\":\"" << (transfer.isUpload ? "upload" : "download") << "\",";
        ss << "\"status\":\"active\",";
        ss << "\"progress\":" << transfer.progress << ",";
        ss << "\"size\":\"" << formatBytes(transfer.totalBytes) << "\",";
        ss << "\"speed\":\"" << formatBytes(transfer.speedBps) << "/s\"";
        ss << "}";
    }
    
    // Then, get pending transfers from sync_queue (database)
    if (storage_) {
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        const char* sql = "SELECT file_path, op_type, status FROM sync_queue WHERE status = 'pending' ORDER BY created_at DESC LIMIT 20";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                const char* op = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                const char* status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                
                std::string pathStr = path ? path : "";
                std::filesystem::path p(pathStr);
                std::string filename = p.filename().string();
                if (filename.empty()) filename = pathStr;
                
                // Determine type from op_type
                std::string type = "upload";
                if (op) {
                    std::string opStr(op);
                    if (opStr == "download" || opStr == "pull") type = "download";
                }
                
                ss << "{";
                ss << "\"file\":\"" << filename << "\",";
                ss << "\"peer\":\"Unknown\",";
                ss << "\"type\":\"" << type << "\",";
                ss << "\"status\":\"" << (status ? status : "pending") << "\",";
                ss << "\"progress\":0,";
                ss << "\"size\":\"-\",";
                ss << "\"speed\":\"-\"";
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    ss << "], \"history\": [";
    first = true;
    
    // Add transfer history from file_access_log, fallback to files table if empty
    if (storage_) {
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        
        // First try file_access_log
        const char* historySql = "SELECT file_path, op_type, timestamp FROM file_access_log ORDER BY timestamp DESC LIMIT 20";
        sqlite3_stmt* stmt;
        bool hasAccessLog = false;
        
        if (sqlite3_prepare_v2(db, historySql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                hasAccessLog = true;
                if (!first) ss << ",";
                first = false;
                
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                const char* op = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                long long timestamp = sqlite3_column_int64(stmt, 2);
                
                std::string pathStr = path ? path : "";
                std::filesystem::path p(pathStr);
                std::string filename = p.filename().string();
                if (filename.empty()) filename = pathStr;
                
                // Format time
                auto now = std::time(nullptr);
                auto diff = now - timestamp;
                std::string timeStr;
                if (diff < 60) timeStr = "Just now";
                else if (diff < 3600) timeStr = std::to_string(diff / 60) + " mins ago";
                else if (diff < 86400) timeStr = std::to_string(diff / 3600) + " hours ago";
                else timeStr = std::to_string(diff / 86400) + " days ago";
                
                ss << "{";
                ss << "\"file\":\"" << filename << "\",";
                ss << "\"type\":\"" << (op ? op : "sync") << "\",";
                ss << "\"time\":\"" << timeStr << "\"";
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
        
        // Fallback: if file_access_log is empty, show synced files from files table
        if (!hasAccessLog) {
            // Only show files with valid positive timestamps
            const char* filesSql = "SELECT path, size, timestamp, synced FROM files WHERE synced = 1 AND timestamp > 0 ORDER BY timestamp DESC LIMIT 20";
            if (sqlite3_prepare_v2(db, filesSql, -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    if (!first) ss << ",";
                    first = false;
                    
                    const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    long long size = sqlite3_column_int64(stmt, 1);
                    long long timestamp = sqlite3_column_int64(stmt, 2);
                    
                    std::string pathStr = path ? path : "";
                    std::filesystem::path p(pathStr);
                    std::string filename = p.filename().string();
                    if (filename.empty()) filename = pathStr;
                    
                    // Format time - handle edge cases
                    auto now = std::time(nullptr);
                    auto diff = now - timestamp;
                    std::string timeStr;
                    if (diff < 0 || timestamp <= 0) {
                        timeStr = "Unknown";
                    } else if (diff < 60) {
                        timeStr = "Just now";
                    } else if (diff < 3600) {
                        timeStr = std::to_string(diff / 60) + " mins ago";
                    } else if (diff < 86400) {
                        timeStr = std::to_string(diff / 3600) + " hours ago";
                    } else if (diff < 2592000) { // 30 days
                        timeStr = std::to_string(diff / 86400) + " days ago";
                    } else {
                        timeStr = "Over a month ago";
                    }
                    
                    ss << "{";
                    ss << "\"file\":\"" << filename << "\",";
                    ss << "\"type\":\"sync\",";
                    ss << "\"size\":\"" << formatBytes(size) << "\",";
                    ss << "\"time\":\"" << timeStr << "\"";
                    ss << "}";
                }
                sqlite3_finalize(stmt);
            }
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string IPCHandler::handleConfigJsonCommand() {
    std::stringstream ss;
    ss << "{";
    
    // Get current config from daemon
    if (daemonCore_) {
        const auto& config = daemonCore_->getConfig();
        
        ss << "\"tcpPort\":" << config.tcpPort << ",";
        ss << "\"discoveryPort\":" << config.discoveryPort << ",";
        ss << "\"metricsPort\":" << config.metricsPort << ",";
        ss << "\"watchDirectory\":\"" << config.watchDirectory << "\",";
        // Get actual session code from network plugin (runtime value)
        std::string currentSessionCode = network_ ? network_->getSessionCode() : config.sessionCode;
        ss << "\"sessionCode\":\"" << currentSessionCode << "\",";
        ss << "\"encryptionEnabled\":" << (config.encryptionEnabled ? "true" : "false") << ",";
        ss << "\"uploadLimit\":" << (config.uploadLimit / 1024) << ",";  // Convert to KB/s
        ss << "\"downloadLimit\":" << (config.downloadLimit / 1024);  // Convert to KB/s
    } else {
        ss << "\"error\":\"Daemon not initialized\"";
    }
    
    // Add network status
    if (network_) {
        ss << ",\"encryption\":" << (network_->isEncryptionEnabled() ? "true" : "false");
        ss << ",\"hasSessionCode\":" << (!network_->getSessionCode().empty() ? "true" : "false");
    }
    
    // Add sync status
    if (daemonCore_) {
        ss << ",\"syncEnabled\":" << (daemonCore_->isSyncEnabled() ? "true" : "false");
    }
    
    // Add watched folders list from database
    ss << ",\"watchedFolders\":[";
    if (storage_) {
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        const char* sql = "SELECT path FROM watched_folders WHERE status = 'active'";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            bool first = true;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (path) {
                    // Simple JSON string escape (for basic paths)
                    ss << "\"" << path << "\"";
                }
            }
            sqlite3_finalize(stmt);
        }
    }
    ss << "]";

    ss << "}\n";
    return ss.str();
}

std::string IPCHandler::handleSetConfigCommand(const std::string& args) {
    // Expected format: SET_CONFIG key=value
    size_t eqPos = args.find('=');
    if (eqPos == std::string::npos) {
        return "Error: Invalid format. Use: SET_CONFIG key=value\n";
    }
    
    std::string key = args.substr(0, eqPos);
    std::string value = args.substr(eqPos + 1);
    
    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t\r\n"));
    key.erase(key.find_last_not_of(" \t\r\n") + 1);
    value.erase(0, value.find_first_not_of(" \t\r\n"));
    value.erase(value.find_last_not_of(" \t\r\n") + 1);
    
    try {
        if (key == "uploadLimit") {
            size_t kb = std::stoull(value);
            if (network_) {
                network_->setGlobalUploadLimit(kb * 1024);
                return "Success: Upload limit set to " + value + " KB/s\n";
            }
        } else if (key == "downloadLimit") {
            size_t kb = std::stoull(value);
            if (network_) {
                network_->setGlobalDownloadLimit(kb * 1024);
                return "Success: Download limit set to " + value + " KB/s\n";
            }
        } else if (key == "sessionCode") {
            if (network_) {
                network_->setSessionCode(value);
                return "Success: Session code updated\n";
            }
        } else if (key == "encryption") {
            bool enable = (value == "true" || value == "1" || value == "enabled");
            if (network_) {
                network_->setEncryptionEnabled(enable);
                return "Success: Encryption " + std::string(enable ? "enabled" : "disabled") + "\n";
            }
        } else if (key == "syncEnabled") {
            bool enable = (value == "true" || value == "1" || value == "enabled");
            if (daemonCore_) {
                if (enable) {
                    daemonCore_->resumeSync();
                } else {
                    daemonCore_->pauseSync();
                }
                return "Success: Sync " + std::string(enable ? "enabled" : "disabled") + "\n";
            }
        } else {
            return "Error: Unknown config key: " + key + "\n";
        }
    } catch (const std::exception& e) {
        return "Error: Invalid value for " + key + ": " + e.what() + "\n";
    }
    
    return "Error: Failed to set config\n";
}

std::string IPCHandler::handleConflictsJsonCommand() {
    std::stringstream ss;
    ss << "{\"conflicts\":[";
    
    if (storage_) {
        auto conflicts = storage_->getUnresolvedConflicts();
        bool first = true;
        for (const auto& c : conflicts) {
            if (!first) ss << ",";
            first = false;
            ss << "{";
            ss << "\"id\":" << c.id << ",";
            ss << "\"path\":\"" << c.path << "\",";
            ss << "\"localSize\":" << c.localSize << ",";
            ss << "\"remoteSize\":" << c.remoteSize << ",";
            ss << "\"localTimestamp\":" << c.localTimestamp << ",";
            ss << "\"remoteTimestamp\":" << c.remoteTimestamp << ",";
            ss << "\"remotePeerId\":\"" << c.remotePeerId << "\",";
            ss << "\"strategy\":" << c.strategy;
            ss << "}";
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string IPCHandler::handleSyncQueueJsonCommand() {
    std::stringstream ss;
    ss << "{\"queue\":[";
    
    if (storage_) {
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        const char* sql = "SELECT id, file_path, operation, status, progress, size, peer_id, created_at FROM sync_queue ORDER BY created_at DESC LIMIT 50";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            bool first = true;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                
                int id = sqlite3_column_int(stmt, 0);
                const char* path = (const char*)sqlite3_column_text(stmt, 1);
                const char* op = (const char*)sqlite3_column_text(stmt, 2);
                const char* status = (const char*)sqlite3_column_text(stmt, 3);
                int progress = sqlite3_column_int(stmt, 4);
                int64_t size = sqlite3_column_int64(stmt, 5);
                const char* peer = (const char*)sqlite3_column_text(stmt, 6);
                const char* created = (const char*)sqlite3_column_text(stmt, 7);
                
                ss << "{";
                ss << "\"id\":" << id << ",";
                ss << "\"path\":\"" << (path ? path : "") << "\",";
                ss << "\"operation\":\"" << (op ? op : "") << "\",";
                ss << "\"status\":\"" << (status ? status : "") << "\",";
                ss << "\"progress\":" << progress << ",";
                ss << "\"size\":" << size << ",";
                ss << "\"peer\":\"" << (peer ? peer : "") << "\",";
                ss << "\"created\":\"" << (created ? created : "") << "\"";
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string IPCHandler::handleExportConfigCommand() {
    std::stringstream ss;
    ss << "{";
    
    if (daemonCore_) {
        const auto& config = daemonCore_->getConfig();
        std::string sessionCode = network_ ? network_->getSessionCode() : config.sessionCode;
        bool encryption = network_ ? network_->isEncryptionEnabled() : config.encryptionEnabled;
        
        ss << "\"tcpPort\":" << config.tcpPort << ",";
        ss << "\"discoveryPort\":" << config.discoveryPort << ",";
        ss << "\"metricsPort\":" << config.metricsPort << ",";
        ss << "\"watchDirectory\":\"" << config.watchDirectory << "\",";
        ss << "\"sessionCode\":\"" << sessionCode << "\",";
        ss << "\"encryptionEnabled\":" << (encryption ? "true" : "false") << ",";
        ss << "\"uploadLimit\":" << config.uploadLimit << ",";
        ss << "\"downloadLimit\":" << config.downloadLimit << ",";
        ss << "\"syncEnabled\":" << (daemonCore_->isSyncEnabled() ? "true" : "false");
    }
    
    ss << "}\n";
    return ss.str();
}

std::string IPCHandler::handleImportConfigCommand(const std::string& args) {
    // Parse JSON config and apply settings
    // Format: IMPORT_CONFIG {"sessionCode":"ABC123","encryption":true,...}
    try {
        // Simple parsing - look for key values
        if (args.find("sessionCode") != std::string::npos) {
            size_t start = args.find("sessionCode\":\"") + 14;
            size_t end = args.find("\"", start);
            if (start != std::string::npos && end != std::string::npos && network_) {
                std::string code = args.substr(start, end - start);
                if (!code.empty()) network_->setSessionCode(code);
            }
        }
        
        if (args.find("\"encryptionEnabled\":true") != std::string::npos && network_) {
            network_->setEncryptionEnabled(true);
        } else if (args.find("\"encryptionEnabled\":false") != std::string::npos && network_) {
            network_->setEncryptionEnabled(false);
        }
        
        if (args.find("uploadLimit\":") != std::string::npos && network_) {
            size_t start = args.find("uploadLimit\":") + 13;
            size_t end = args.find_first_of(",}", start);
            if (start != std::string::npos && end != std::string::npos) {
                size_t limit = std::stoull(args.substr(start, end - start));
                network_->setGlobalUploadLimit(limit);
            }
        }
        
        if (args.find("downloadLimit\":") != std::string::npos && network_) {
            size_t start = args.find("downloadLimit\":") + 15;
            size_t end = args.find_first_of(",}", start);
            if (start != std::string::npos && end != std::string::npos) {
                size_t limit = std::stoull(args.substr(start, end - start));
                network_->setGlobalDownloadLimit(limit);
            }
        }
        
        return "Success: Configuration imported\n";
    } catch (const std::exception& e) {
        return "Error: Failed to import config: " + std::string(e.what()) + "\n";
    }
}

std::string IPCHandler::handleAddIgnoreCommand(const std::string& args) {
    if (args.empty()) {
        return "Error: No pattern provided\n";
    }
    
    if (!storage_) {
        return "Error: Storage not initialized\n";
    }
    
    sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
    
    // Create ignore_patterns table if not exists
    const char* createSql = "CREATE TABLE IF NOT EXISTS ignore_patterns (id INTEGER PRIMARY KEY, pattern TEXT UNIQUE, created_at DATETIME DEFAULT CURRENT_TIMESTAMP)";
    sqlite3_exec(db, createSql, nullptr, nullptr, nullptr);
    
    const char* sql = "INSERT OR IGNORE INTO ignore_patterns (pattern) VALUES (?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, args.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return "Success: Pattern added: " + args + "\n";
        }
        sqlite3_finalize(stmt);
    }
    
    return "Error: Failed to add pattern\n";
}

std::string IPCHandler::handleRemoveIgnoreCommand(const std::string& args) {
    if (args.empty()) {
        return "Error: No pattern provided\n";
    }
    
    if (!storage_) {
        return "Error: Storage not initialized\n";
    }
    
    sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
    const char* sql = "DELETE FROM ignore_patterns WHERE pattern = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, args.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return "Success: Pattern removed: " + args + "\n";
        }
        sqlite3_finalize(stmt);
    }
    
    return "Error: Failed to remove pattern\n";
}

std::string IPCHandler::handleListIgnoreCommand() {
    std::stringstream ss;
    ss << "{\"patterns\":[";
    
    if (storage_) {
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        
        // Create table if not exists
        const char* createSql = "CREATE TABLE IF NOT EXISTS ignore_patterns (id INTEGER PRIMARY KEY, pattern TEXT UNIQUE, created_at DATETIME DEFAULT CURRENT_TIMESTAMP)";
        sqlite3_exec(db, createSql, nullptr, nullptr, nullptr);
        
        const char* sql = "SELECT pattern FROM ignore_patterns ORDER BY pattern";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            bool first = true;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                const char* pattern = (const char*)sqlite3_column_text(stmt, 0);
                ss << "\"" << (pattern ? pattern : "") << "\"";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string IPCHandler::handleResolveConflictCommand(const std::string& args) {
    // Format: RESOLVE_CONFLICT <id> <resolution>
    // resolution: local, remote, both
    std::istringstream iss(args);
    int conflictId;
    std::string resolution;
    
    if (!(iss >> conflictId >> resolution)) {
        return "Error: Usage: RESOLVE_CONFLICT <id> <local|remote|both>\n";
    }
    
    if (!storage_) {
        return "Error: Storage not initialized\n";
    }
    
    sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
    const char* sql = "UPDATE conflicts SET resolved = 1, resolved_at = datetime('now'), strategy = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    
    int strategy = 0; // 0=local, 1=remote, 2=both
    std::string msg;
    
    if (resolution == "local") {
        strategy = 0;
        msg = "keeping local version";
    } else if (resolution == "remote") {
        strategy = 1;
        msg = "keeping remote version";
    } else if (resolution == "both") {
        strategy = 2;
        msg = "keeping both versions";
    } else {
        return "Error: Invalid resolution. Use: local, remote, or both\n";
    }
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, strategy);
        sqlite3_bind_int(stmt, 2, conflictId);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return "Success: Conflict resolved - " + msg + "\n";
        }
        sqlite3_finalize(stmt);
    }
    
    return "Error: Failed to resolve conflict\n";
}

std::string IPCHandler::handleBlockPeerCommand(const std::string& args) {
    if (args.empty()) {
        return "Error: No peer ID provided. Usage: BLOCK_PEER <peer_id>\n";
    }
    
    if (!storage_) {
        return "Error: Storage not initialized\n";
    }
    
    sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
    
    // Create blocked_peers table if not exists
    const char* createSql = "CREATE TABLE IF NOT EXISTS blocked_peers (peer_id TEXT PRIMARY KEY, blocked_at DATETIME DEFAULT CURRENT_TIMESTAMP)";
    sqlite3_exec(db, createSql, nullptr, nullptr, nullptr);
    
    // Add to blocked list
    const char* sql = "INSERT OR REPLACE INTO blocked_peers (peer_id) VALUES (?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, args.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            
            // Also remove from peers table
            const char* delSql = "DELETE FROM peers WHERE peer_id = ?";
            sqlite3_stmt* delStmt;
            if (sqlite3_prepare_v2(db, delSql, -1, &delStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(delStmt, 1, args.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(delStmt);
                sqlite3_finalize(delStmt);
            }
            
            return "Success: Peer blocked: " + args + "\n";
        }
        sqlite3_finalize(stmt);
    }
    
    return "Error: Failed to block peer\n";
}

std::string IPCHandler::handleUnblockPeerCommand(const std::string& args) {
    if (args.empty()) {
        return "Error: No peer ID provided. Usage: UNBLOCK_PEER <peer_id>\n";
    }
    
    if (!storage_) {
        return "Error: Storage not initialized\n";
    }
    
    sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
    const char* sql = "DELETE FROM blocked_peers WHERE peer_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, args.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return "Success: Peer unblocked: " + args + "\n";
        }
        sqlite3_finalize(stmt);
    }
    
    return "Error: Failed to unblock peer\n";
}

HealthSummary IPCHandler::computeHealthSummary() const {
    HealthSummary summary;
    
    // Disk usage - use root filesystem as fallback
    std::string watchDir = "/";
    if (daemonCore_) {
        std::string configDir = daemonCore_->getConfig().watchDirectory;
        // Expand tilde if present
        if (!configDir.empty() && configDir[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home) {
                watchDir = std::string(home) + configDir.substr(1);
            }
        } else if (!configDir.empty()) {
            watchDir = configDir;
        }
    }
    
    struct statvfs statBuf;
    if (statvfs(watchDir.c_str(), &statBuf) == 0) {
        summary.diskTotalBytes = statBuf.f_blocks * statBuf.f_frsize;
        summary.diskFreeBytes = statBuf.f_bavail * statBuf.f_frsize;
        if (summary.diskTotalBytes > 0) {
            summary.diskUsagePercent = 100.0 * (1.0 - static_cast<double>(summary.diskFreeBytes) / summary.diskTotalBytes);
        }
    }
    
    // Database status
    if (storage_) {
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        summary.dbConnected = (db != nullptr);
        
        if (db) {
            // Get DB file size via pragma database_list
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, "PRAGMA database_list", -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* dbPathRaw = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                    if (dbPathRaw) {
                        std::string dbPathStr = dbPathRaw;
                        sqlite3_finalize(stmt);
                        stmt = nullptr;
                        std::error_code ec;
                        auto size = std::filesystem::file_size(dbPathStr, ec);
                        if (!ec) {
                            summary.dbSizeBytes = static_cast<int64_t>(size);
                        }
                    }
                }
                if (stmt) {
                    sqlite3_finalize(stmt);
                }
            }
        }
    }
    
    // Watcher count - count active watched folders from DB
    if (storage_) {
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        if (db) {
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM watched_folders WHERE status = 'active'", -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    summary.activeWatcherCount = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);
            }
        }
    }
    
    // Overall health assessment
    summary.healthy = true;
    summary.statusMessage = "OK";
    
    if (summary.diskUsagePercent > 90.0) {
        summary.healthy = false;
        summary.statusMessage = "Disk usage critical";
    } else if (!summary.dbConnected) {
        summary.healthy = false;
        summary.statusMessage = "Database disconnected";
    } else if (summary.activeWatcherCount == 0) {
        summary.statusMessage = "No active watchers";
    }
    
    return summary;
}

std::vector<PeerHealthReport> IPCHandler::computePeerHealthReports() const {
    std::vector<PeerHealthReport> reports;
    
    if (!autoRemesh_) {
        return reports;
    }
    
    auto metrics = autoRemesh_->snapshotMetrics();
    reports.reserve(metrics.size());
    
    for (const auto& m : metrics) {
        PeerHealthReport report;
        report.peerId = m.peerId;
        report.avgRttMs = m.avgRttMs;
        report.jitterMs = m.jitterMs;
        report.packetLossPercent = m.packetLossPercent;
        
        // Check degradation thresholds
        report.degraded = (m.jitterMs > healthThresholds_.jitterThresholdMs) ||
                          (m.packetLossPercent > healthThresholds_.packetLossThresholdPercent) ||
                          (m.avgRttMs > healthThresholds_.rttThresholdMs);
        
        reports.push_back(std::move(report));
    }
    
    return reports;
}

AnomalyReport IPCHandler::getAnomalyReport() const {
    AnomalyReport report;
    
    // Get anomaly data from MetricsCollector security metrics
    auto& metrics = MetricsCollector::instance();
    auto secMetrics = metrics.getSecurityMetrics();
    
    // Use anomalies detected count as a proxy for score
    // In a full implementation, this would query the ML plugin directly
    if (secMetrics.anomaliesDetected > 0) {
        // Simple score: cap at 1.0, scale by recent anomaly count
        report.score = std::min(1.0, static_cast<double>(secMetrics.anomaliesDetected) / 10.0);
        report.lastType = "ANOMALY_DETECTED";
        report.lastDetectedAt = std::time(nullptr);
    }
    
    return report;
}

} // namespace SentinelFS
