/**
 * @file IPCHandler.cpp
 * @brief IPC socket handler for CLI/GUI communication
 * 
 * This is the refactored version that delegates command processing
 * to specialized handler classes in ipc/commands/
 */

#include "IPCHandler.h"
#include "DaemonCore.h"
#include "ipc/commands/Commands.h"
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
#include <sqlite3.h>
#include <filesystem>
#include <algorithm>
#include <grp.h>
#include <pwd.h>

namespace SentinelFS {

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
    // Use secure defaults
    securityConfig_.socketPermissions = 0660;  // rw-rw----
    securityConfig_.requireSameUid = true;
    securityConfig_.auditConnections = false;
    securityConfig_.maxCommandsPerMinute = 0;  // Disable rate limiting by default
    
    initializeCommandHandlers();
}

IPCHandler::IPCHandler(const std::string& socketPath,
                       const IPCSecurityConfig& securityConfig,
                       INetworkAPI* network,
                       IStorageAPI* storage,
                       IFileAPI* filesystem,
                       DaemonCore* daemonCore,
                       AutoRemeshManager* autoRemesh)
    : socketPath_(socketPath)
    , securityConfig_(securityConfig)
    , network_(network)
    , storage_(storage)
    , filesystem_(filesystem)
    , daemonCore_(daemonCore)
    , autoRemesh_(autoRemesh)
{
    initializeCommandHandlers();
}

IPCHandler::~IPCHandler() {
    stop();
}

void IPCHandler::initializeCommandHandlers() {
    // Initialize command context
    cmdContext_.network = network_;
    cmdContext_.storage = storage_;
    cmdContext_.filesystem = filesystem_;
    cmdContext_.daemonCore = daemonCore_;
    cmdContext_.autoRemesh = autoRemesh_;
    
    // Create command handlers
    statusCmds_ = std::make_unique<StatusCommands>(cmdContext_);
    peerCmds_ = std::make_unique<PeerCommands>(cmdContext_);
    configCmds_ = std::make_unique<ConfigCommands>(cmdContext_);
    fileCmds_ = std::make_unique<FileCommands>(cmdContext_);
    transferCmds_ = std::make_unique<TransferCommands>(cmdContext_);
    relayCmds_ = std::make_unique<RelayCommands>(cmdContext_);
}

bool IPCHandler::ensureSocketDirectory() {
    std::filesystem::path sockPath(socketPath_);
    std::filesystem::path parentDir = sockPath.parent_path();
    
    if (parentDir.empty()) {
        return true;
    }
    
    if (!std::filesystem::exists(parentDir)) {
        if (!securityConfig_.createParentDirs) {
            std::cerr << "IPC: Socket directory does not exist: " << parentDir << std::endl;
            return false;
        }
        
        try {
            std::filesystem::create_directories(parentDir);
            
            if (chmod(parentDir.c_str(), securityConfig_.parentDirPermissions) != 0) {
                std::cerr << "IPC: Warning - Failed to set directory permissions" << std::endl;
            }
            
            std::cout << "IPC: Created socket directory: " << parentDir << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "IPC: Failed to create socket directory: " << e.what() << std::endl;
            return false;
        }
    }
    
    return true;
}

bool IPCHandler::verifyClientCredentials(int clientSocket, uid_t& clientUid, gid_t& clientGid) {
    struct ucred cred;
    socklen_t credLen = sizeof(cred);
    
    if (getsockopt(clientSocket, SOL_SOCKET, SO_PEERCRED, &cred, &credLen) != 0) {
        std::cerr << "IPC: Failed to get client credentials" << std::endl;
        return false;
    }
    
    clientUid = cred.uid;
    clientGid = cred.gid;
    return true;
}

bool IPCHandler::isClientAuthorized(uid_t uid, gid_t gid) {
    if (securityConfig_.requireSameUid && uid == geteuid()) {
        return true;
    }
    
    if (!securityConfig_.allowedUids.empty()) {
        auto it = std::find(securityConfig_.allowedUids.begin(),
                            securityConfig_.allowedUids.end(), uid);
        if (it != securityConfig_.allowedUids.end()) {
            return true;
        }
    }
    
    if (!securityConfig_.allowedGids.empty()) {
        auto it = std::find(securityConfig_.allowedGids.begin(),
                            securityConfig_.allowedGids.end(), gid);
        if (it != securityConfig_.allowedGids.end()) {
            return true;
        }
    }
    
    if (!securityConfig_.requireSameUid && 
        securityConfig_.allowedUids.empty() &&
        securityConfig_.allowedGids.empty()) {
        return true;
    }
    
    return false;
}

bool IPCHandler::checkRateLimit(uid_t uid) {
    if (securityConfig_.maxCommandsPerMinute == 0) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(rateLimitMutex_);
    
    time_t now = time(nullptr);
    auto& limit = clientRateLimits_[uid];
    
    if (now - limit.windowStart >= 60) {
        limit.windowStart = now;
        limit.commandCount = 0;
    }
    
    if (limit.commandCount >= securityConfig_.maxCommandsPerMinute) {
        std::cerr << "IPC: Rate limit exceeded for UID " << uid << std::endl;
        return false;
    }
    
    limit.commandCount++;
    return true;
}

void IPCHandler::auditConnection(uid_t uid, gid_t gid, bool authorized) {
    if (!securityConfig_.auditConnections) {
        return;
    }
    
    std::string username = "unknown";
    struct passwd* pw = getpwuid(uid);
    if (pw) {
        username = pw->pw_name;
    }
    
    std::string groupname = "unknown";
    struct group* gr = getgrgid(gid);
    if (gr) {
        groupname = gr->gr_name;
    }
    
    std::cout << "IPC AUDIT: Connection from UID=" << uid << " (" << username << ") "
              << "GID=" << gid << " (" << groupname << ") - "
              << (authorized ? "AUTHORIZED" : "DENIED") << std::endl;
}

bool IPCHandler::start() {
    if (running_) return true;
    
    if (!ensureSocketDirectory()) {
        return false;
    }
    
    unlink(socketPath_.c_str());
    
    mode_t oldMask = umask(0077);
    
    serverSocket_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        umask(oldMask);
        std::cerr << "IPC: Cannot create socket" << std::endl;
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);
    
    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        umask(oldMask);
        std::cerr << "IPC: Cannot bind socket: " << strerror(errno) << std::endl;
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }

    umask(oldMask);

    if (chmod(socketPath_.c_str(), securityConfig_.socketPermissions) < 0) {
        std::cerr << "IPC: Warning - Failed to set socket file permissions: " 
                  << strerror(errno) << std::endl;
    }
    
    if (!securityConfig_.requiredGroup.empty()) {
        struct group* gr = getgrnam(securityConfig_.requiredGroup.c_str());
        if (gr) {
            if (chown(socketPath_.c_str(), -1, gr->gr_gid) < 0) {
                std::cerr << "IPC: Warning - Failed to set socket group ownership" << std::endl;
            }
        } else {
            std::cerr << "IPC: Warning - Group '" << securityConfig_.requiredGroup 
                      << "' not found" << std::endl;
        }
    }
    
    if (listen(serverSocket_, 5) < 0) {
        std::cerr << "IPC: Cannot listen" << std::endl;
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }
    
    std::cout << "IPC Server listening on " << socketPath_ 
              << " (mode: " << std::oct << securityConfig_.socketPermissions << std::dec << ")"
              << std::endl;
    
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
        
        std::thread([this, clientSocket]() {
            handleClient(clientSocket);
            close(clientSocket);
        }).detach();
    }
}

void IPCHandler::handleClient(int clientSocket) {
    uid_t clientUid = 0;
    gid_t clientGid = 0;
    
    if (!verifyClientCredentials(clientSocket, clientUid, clientGid)) {
        const char* msg = "ERROR: Failed to verify credentials\n";
        send(clientSocket, msg, strlen(msg), 0);
        return;
    }
    
    bool authorized = isClientAuthorized(clientUid, clientGid);
    auditConnection(clientUid, clientGid, authorized);
    
    if (!authorized) {
        const char* msg = "ERROR: Unauthorized IPC client\n";
        send(clientSocket, msg, strlen(msg), 0);
        return;
    }
    
    char buffer[1024];
    std::string lineBuffer;
    
    while (running_) {
        ssize_t n = recv(clientSocket, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        
        if (n > 0) {
            buffer[n] = '\0';
            lineBuffer += buffer;
            
            size_t pos;
            while ((pos = lineBuffer.find('\n')) != std::string::npos) {
                std::string command = lineBuffer.substr(0, pos);
                lineBuffer.erase(0, pos + 1);
                
                if (!command.empty()) {
                    if (!checkRateLimit(clientUid)) {
                        const char* msg = "ERROR: Rate limit exceeded. Try again later.\n";
                        send(clientSocket, msg, strlen(msg), 0);
                        continue;
                    }
                    
                    std::string response = processCommand(command);
                    // CodeQL: This is intentional - IPC responses to authorized local clients
                    // Security: Unix socket with peer credential verification ensures only
                    // authorized local processes can receive this data.
                    // lgtm[cpp/system-data-exposure]
                    send(clientSocket, response.c_str(), response.length(), 0);  // NOLINT(codeql)
                }
            }
        } else if (n == 0) {
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            break;
        }
        
        usleep(10000);
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
    
    // Update sync callback in context
    cmdContext_.syncEnabledCallback = syncEnabledCallback_;
    
    // Route to appropriate handler
    // Status commands
    if (cmd == "STATUS") {
        return statusCmds_->handleStatus();
    } else if (cmd == "STATUS_JSON") {
        return statusCmds_->handleStatusJson();
    } else if (cmd == "PLUGINS") {
        return statusCmds_->handlePlugins();
    } else if (cmd == "STATS") {
        return statusCmds_->handleStats();
    }
    
    // Peer commands
    else if (cmd == "PEERS") {
        return peerCmds_->handleList();
    } else if (cmd == "PEERS_JSON") {
        return peerCmds_->handlePeersJson();
    } else if (cmd == "CONNECT") {
        return peerCmds_->handleConnect(args);
    } else if (cmd == "ADD_PEER") {
        return peerCmds_->handleAddPeer(args);
    } else if (cmd == "BLOCK_PEER") {
        return peerCmds_->handleBlockPeer(args);
    } else if (cmd == "UNBLOCK_PEER") {
        return peerCmds_->handleUnblockPeer(args);
    } else if (cmd == "CLEAR_PEERS") {
        return peerCmds_->handleClearPeers();
    }
    
    // Transfer and sync commands
    else if (cmd == "PAUSE") {
        return transferCmds_->handlePause();
    } else if (cmd == "RESUME") {
        return transferCmds_->handleResume();
    } else if (cmd == "UPLOAD-LIMIT") {
        return transferCmds_->handleUploadLimit(args);
    } else if (cmd == "DOWNLOAD-LIMIT") {
        return transferCmds_->handleDownloadLimit(args);
    } else if (cmd == "METRICS") {
        return transferCmds_->handleMetrics();
    } else if (cmd == "METRICS_JSON") {
        return transferCmds_->handleMetricsJson();
    } else if (cmd == "TRANSFERS_JSON") {
        return transferCmds_->handleTransfersJson();
    } else if (cmd == "DISCOVER") {
        return transferCmds_->handleDiscover();
    } else if (cmd == "SET_DISCOVERY") {
        return transferCmds_->handleSetDiscovery(args);
    } else if (cmd == "GET_RELAY_STATUS") {
        return transferCmds_->handleRelayStatus();
    } else if (cmd == "SET_ENCRYPTION") {
        return transferCmds_->handleSetEncryption(args);
    } else if (cmd == "SET_SESSION_CODE") {
        return transferCmds_->handleSetSessionCode(args);
    } else if (cmd == "GENERATE_CODE") {
        return transferCmds_->handleGenerateCode();
    }
    
    // File commands
    else if (cmd == "FILES_JSON") {
        return fileCmds_->handleFilesJson();
    } else if (cmd == "ACTIVITY_JSON") {
        return fileCmds_->handleActivityJson();
    } else if (cmd == "ADD_FOLDER") {
        return fileCmds_->handleAddFolder(args);
    } else if (cmd == "REMOVE_WATCH") {
        return fileCmds_->handleRemoveWatch(args);
    } else if (cmd == "CONFLICTS") {
        return fileCmds_->handleConflicts();
    } else if (cmd == "CONFLICTS_JSON") {
        return fileCmds_->handleConflictsJson();
    } else if (cmd == "RESOLVE") {
        return fileCmds_->handleResolve(args);
    } else if (cmd == "RESOLVE_CONFLICT") {
        return fileCmds_->handleResolveConflict(args);
    } else if (cmd == "SYNC_QUEUE_JSON") {
        return fileCmds_->handleSyncQueueJson();
    } else if (cmd == "VERSIONS_JSON") {
        return fileCmds_->handleVersionsJson();
    } else if (cmd == "RESTORE_VERSION") {
        return fileCmds_->handleRestoreVersion(args);
    } else if (cmd == "DELETE_VERSION") {
        return fileCmds_->handleDeleteVersion(args);
    } else if (cmd == "PREVIEW_VERSION") {
        return fileCmds_->handlePreviewVersion(args);
    }
    
    // Config commands
    else if (cmd == "CONFIG_JSON") {
        return configCmds_->handleConfigJson();
    } else if (cmd == "SET_CONFIG") {
        return configCmds_->handleSetConfig(args);
    } else if (cmd == "EXPORT_CONFIG") {
        return configCmds_->handleExportConfig();
    } else if (cmd == "IMPORT_CONFIG") {
        return configCmds_->handleImportConfig(args);
    } else if (cmd == "ADD_IGNORE") {
        return configCmds_->handleAddIgnore(args);
    } else if (cmd == "REMOVE_IGNORE") {
        return configCmds_->handleRemoveIgnore(args);
    } else if (cmd == "LIST_IGNORE") {
        return configCmds_->handleListIgnore();
    } else if (cmd == "EXPORT_SUPPORT_BUNDLE") {
        return configCmds_->handleExportSupportBundle();
    }
    
    // Relay commands
    else if (cmd == "RELAY_CONNECT") {
        return relayCmds_->handleRelayConnect(args);
    } else if (cmd == "RELAY_DISCONNECT") {
        return relayCmds_->handleRelayDisconnect();
    } else if (cmd == "RELAY_STATUS") {
        return relayCmds_->handleRelayStatus();
    } else if (cmd == "RELAY_PEERS") {
        return relayCmds_->handleRelayPeers();
    }
    
    // Unknown command - sanitized response to avoid information disclosure
    else {
        // Don't echo back the unknown command to prevent potential injection/disclosure
        return "ERROR: Invalid command. Use STATUS for help.\n";
    }
}

// Sanitize response to remove sensitive system information
std::string IPCHandler::sanitizeResponse(const std::string& response) {
    std::string sanitized = response;
    
    // Remove absolute paths that might reveal system structure
    // Keep relative paths and filenames only
    size_t pos = 0;
    while ((pos = sanitized.find("/home/", pos)) != std::string::npos) {
        size_t endPos = sanitized.find_first_of(" \n\t\"'", pos);
        if (endPos != std::string::npos) {
            std::string path = sanitized.substr(pos, endPos - pos);
            std::string filename = std::filesystem::path(path).filename().string();
            sanitized.replace(pos, endPos - pos, "~/" + filename);
        }
        pos++;
    }
    
    return sanitized;
}

} // namespace SentinelFS
