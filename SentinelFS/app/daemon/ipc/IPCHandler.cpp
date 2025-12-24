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
#include "FalconStore.h"
#include "Logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <poll.h>
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
    securityConfig_.maxCommandsPerMinute = 12000;  // 1200 commands/minute (20/sec) - GUI needs higher limit
    
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
    cmdContext_.versionManager = daemonCore_ ? daemonCore_->getVersionManager() : nullptr;
    
    // Create command handlers
    statusCmds_ = std::make_unique<StatusCommands>(cmdContext_);
    peerCmds_ = std::make_unique<PeerCommands>(cmdContext_);
    configCmds_ = std::make_unique<ConfigCommands>(cmdContext_);
    fileCmds_ = std::make_unique<FileCommands>(cmdContext_);
    transferCmds_ = std::make_unique<TransferCommands>(cmdContext_);
    relayCmds_ = std::make_unique<RelayCommands>(cmdContext_);
    falconstoreCmds_ = std::make_unique<FalconStoreCommands>(cmdContext_);
}

bool IPCHandler::ensureSocketDirectory() {
    std::filesystem::path sockPath(socketPath_);
    std::filesystem::path parentDir = sockPath.parent_path();
    
    if (parentDir.empty()) {
        return true;
    }
    
    if (!std::filesystem::exists(parentDir)) {
        if (!securityConfig_.createParentDirs) {
            Logger::instance().error("IPC: Cannot access socket directory. Please check permissions for: " + parentDir.string(), "IPCHandler");
            return false;
        }
        
        try {
            std::filesystem::create_directories(parentDir);
            
            if (chmod(parentDir.c_str(), securityConfig_.parentDirPermissions) != 0) {
                Logger::instance().warn("IPC: Failed to set directory permissions", "IPCHandler");
            }
            
            Logger::instance().info("IPC: Created socket directory: " + parentDir.string(), "IPCHandler");
        } catch (const std::exception& e) {
            Logger::instance().error("IPC: Failed to create socket directory. Check permissions and disk space for: " + parentDir.string(), "IPCHandler");
            return false;
        }
    }
    
    return true;
}

bool IPCHandler::verifyClientCredentials(int clientSocket, uid_t& clientUid, gid_t& clientGid) {
    struct ucred cred;
    socklen_t credLen = sizeof(cred);
    
    if (getsockopt(clientSocket, SOL_SOCKET, SO_PEERCRED, &cred, &credLen) != 0) {
        Logger::instance().error("IPC: Security verification failed. Unable to authenticate client connection.", "IPCHandler");
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
        Logger::instance().warn("IPC: Rate limit exceeded for UID " + std::to_string(uid), "IPCHandler");
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
    
    std::string auditMsg = "IPC AUDIT: Connection from UID=" + std::to_string(uid) + " (" + username + ") " +
                           "GID=" + std::to_string(gid) + " (" + groupname + ") - " +
                           (authorized ? "AUTHORIZED" : "DENIED");
    Logger::instance().info(auditMsg, "IPCHandler");
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
        Logger::instance().error("IPC: Cannot create communication socket. Another daemon instance may be running.", "IPCHandler");
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);
    
    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        umask(oldMask);
        Logger::instance().error("IPC: Socket bind failed. Port may be in use or permissions insufficient: " + std::string(strerror(errno)), "IPCHandler");
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }

    umask(oldMask);

    if (chmod(socketPath_.c_str(), securityConfig_.socketPermissions) < 0) {
        Logger::instance().warn("IPC: Failed to set socket file permissions: " + std::string(strerror(errno)), "IPCHandler");
    }
    
    if (!securityConfig_.requiredGroup.empty()) {
        struct group* gr = getgrnam(securityConfig_.requiredGroup.c_str());
        if (gr) {
            if (chown(socketPath_.c_str(), -1, gr->gr_gid) < 0) {
                Logger::instance().warn("IPC: Failed to set socket group ownership", "IPCHandler");
            }
        } else {
            Logger::instance().warn("IPC: Group '" + securityConfig_.requiredGroup + "' not found", "IPCHandler");
        }
    }
    
    if (listen(serverSocket_, 5) < 0) {
        Logger::instance().error("IPC: Cannot start listening for connections. Check system limits and permissions.", "IPCHandler");
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }
    
    std::stringstream ss;
    ss << "IPC Server listening on " << socketPath_ << " (mode: " << std::oct << securityConfig_.socketPermissions << std::dec << ")";
    Logger::instance().info(ss.str(), "IPCHandler");
    
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
    
    // Security: Maximum buffer size to prevent DoS attacks
    static constexpr size_t MAX_LINE_BUFFER_SIZE = 1024 * 1024;  // 1MB max
    static constexpr size_t MAX_COMMAND_LENGTH = 64 * 1024;     // 64KB max per command
    
    char buffer[1024];
    std::string lineBuffer;
    lineBuffer.reserve(4096);  // Pre-allocate to reduce reallocations
    
    while (running_) {
        ssize_t n = recv(clientSocket, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        
        if (n > 0) {
            buffer[n] = '\0';
            
            // Security: Prevent DoS by limiting total buffer size
            if (lineBuffer.size() + n > MAX_LINE_BUFFER_SIZE) {
                const char* msg = "ERROR: Command buffer too large. Connection closed.\n";
                send(clientSocket, msg, strlen(msg), 0);
                break;
            }
            
            lineBuffer += buffer;
            
            size_t pos;
            while ((pos = lineBuffer.find('\n')) != std::string::npos) {
                std::string command = lineBuffer.substr(0, pos);
                lineBuffer.erase(0, pos + 1);
                
                // Security: Limit individual command length
                if (command.length() > MAX_COMMAND_LENGTH) {
                    const char* msg = "ERROR: Command too long. Maximum 64KB allowed.\n";
                    send(clientSocket, msg, strlen(msg), 0);
                    continue;
                }
                
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
        
        // Non-blocking wait with poll instead of busy-wait usleep
        struct pollfd pfd;
        pfd.fd = clientSocket;
        pfd.events = POLLIN;
        poll(&pfd, 1, 10); // 10ms timeout
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
    } else if (cmd == "THREAT_STATUS") {
        return statusCmds_->handleThreatStatus();
    } else if (cmd == "THREAT_STATUS_JSON") {
        return statusCmds_->handleThreatStatusJson();
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
    
    // Threat management commands
    else if (cmd == "THREATS_JSON") {
        return fileCmds_->handleThreatsJson();
    } else if (cmd == "DELETE_THREAT") {
        return fileCmds_->handleDeleteThreat(args);
    } else if (cmd == "MARK_THREAT_SAFE") {
        return fileCmds_->handleMarkThreatSafe(args);
    } else if (cmd == "UNMARK_THREAT_SAFE") {
        return fileCmds_->handleUnmarkThreatSafe(args);
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
    
    // NetFalcon commands
    else if (cmd == "NETFALCON_STATUS") {
        return handleNetFalconStatus();
    } else if (cmd == "NETFALCON_SET_STRATEGY") {
        return handleNetFalconSetStrategy(args);
    } else if (cmd == "NETFALCON_SET_TRANSPORT") {
        return handleNetFalconSetTransport(args);
    }
    
    // FalconStore commands
    else if (cmd == "FALCONSTORE_STATUS") {
        Json::Value response = falconstoreCmds_->getStatus(args, Json::Value());
        return formatJsonResponse(response);
    } else if (cmd == "FALCONSTORE_STATS") {
        Json::Value response = falconstoreCmds_->getStats(args, Json::Value());
        return formatJsonResponse(response);
    } else if (cmd == "FALCONSTORE_OPTIMIZE") {
        Json::Value response = falconstoreCmds_->optimize(args, Json::Value());
        return formatJsonResponse(response);
    } else if (cmd == "FALCONSTORE_BACKUP") {
        Json::Value response = falconstoreCmds_->backup(args, Json::Value());
        return formatJsonResponse(response);
    } else if (cmd == "FALCONSTORE_EXECUTE_QUERY") {
        Json::Value response = falconstoreCmds_->executeQuery(args, parseJsonData(args));
        return formatJsonResponse(response);
    } else if (cmd == "FALCONSTORE_GET_TABLES") {
        Json::Value response = falconstoreCmds_->getTables(args, Json::Value());
        return formatJsonResponse(response);
    } else if (cmd == "FALCONSTORE_GET_TABLE_DATA") {
        Json::Value response = falconstoreCmds_->getTableData(args, parseJsonData(args));
        return formatJsonResponse(response);
    } else if (cmd == "FALCONSTORE_VACUUM") {
        Json::Value response = falconstoreCmds_->vacuum(args, Json::Value());
        return formatJsonResponse(response);
    } else if (cmd == "FALCONSTORE_CLEAR_CACHE") {
        Json::Value response = falconstoreCmds_->clearCache(args, Json::Value());
        return formatJsonResponse(response);
    }
    
    // Zer0 commands
    else if (cmd == "ZER0_STATUS") {
        return handleZer0Status();
    } else if (cmd == "ZER0_CONFIG") {
        return handleZer0Config(args);
    } else if (cmd == "ZER0_START_MONITORING") {
        return handleZer0StartMonitoring();
    } else if (cmd == "ZER0_STOP_MONITORING") {
        return handleZer0StopMonitoring();
    } else if (cmd == "ZER0_RELOAD_YARA") {
        return handleZer0ReloadYara();
    } else if (cmd == "ZER0_TRAIN_MODEL") {
        return handleZer0TrainModel();
    } else if (cmd == "ZER0_SCAN") {
        return handleZer0Scan(args);
    }
    
    // Unknown command - sanitized response to avoid information disclosure
    else {
        // Don't echo back the unknown command to prevent potential injection/disclosure
        return "ERROR: Invalid command. Use STATUS for help.\n";
    }
}

// NetFalcon command handlers
std::string IPCHandler::handleNetFalconStatus() {
    std::ostringstream ss;
    ss << "{";
    ss << "\"plugin\":\"NetFalcon\",";
    ss << "\"version\":\"1.0.0\",";
    ss << "\"transports\":{";
    ss << "\"tcp\":{\"enabled\":" << (network_ && network_->isTransportEnabled("tcp") ? "true" : "false") << ",\"listening\":" << (network_ ? "true" : "false") << "},";
    ss << "\"quic\":{\"enabled\":" << (network_ && network_->isTransportEnabled("quic") ? "true" : "false") << ",\"listening\":false},";
    ss << "\"relay\":{\"enabled\":" << (network_ && network_->isTransportEnabled("relay") ? "true" : "false") << ",\"connected\":" << (network_ && network_->isRelayConnected() ? "true" : "false") << "}";
    ss << "},";
    
    // Get strategy name
    std::string strategyName = "FALLBACK_CHAIN";
    if (network_) {
        switch (network_->getTransportStrategy()) {
            case INetworkAPI::TransportStrategy::PREFER_FAST: strategyName = "PREFER_FAST"; break;
            case INetworkAPI::TransportStrategy::PREFER_RELIABLE: strategyName = "PREFER_RELIABLE"; break;
            case INetworkAPI::TransportStrategy::ADAPTIVE: strategyName = "ADAPTIVE"; break;
            default: strategyName = "FALLBACK_CHAIN"; break;
        }
    }
    ss << "\"strategy\":\"" << strategyName << "\",";
    ss << "\"localPeerId\":\"" << (network_ ? network_->getLocalPeerId() : "N/A") << "\",";
    ss << "\"listeningPort\":" << (network_ ? network_->getLocalPort() : 0) << ",";
    ss << "\"sessionCode\":\"" << (network_ ? network_->getSessionCode() : "") << "\",";
    ss << "\"encryptionEnabled\":" << (network_ && network_->isEncryptionEnabled() ? "true" : "false");
    ss << "}";
    return ss.str();
}

std::string IPCHandler::handleNetFalconSetStrategy(const std::string& args) {
    if (args.empty()) {
        return "ERROR: Strategy required (FALLBACK_CHAIN, PREFER_FAST, PREFER_RELIABLE, ADAPTIVE)\n";
    }
    
    if (!network_) {
        return "ERROR: Network plugin not available\n";
    }
    
    INetworkAPI::TransportStrategy strategy;
    if (args == "FALLBACK_CHAIN") {
        strategy = INetworkAPI::TransportStrategy::FALLBACK_CHAIN;
    } else if (args == "PREFER_FAST") {
        strategy = INetworkAPI::TransportStrategy::PREFER_FAST;
    } else if (args == "PREFER_RELIABLE") {
        strategy = INetworkAPI::TransportStrategy::PREFER_RELIABLE;
    } else if (args == "ADAPTIVE") {
        strategy = INetworkAPI::TransportStrategy::ADAPTIVE;
    } else {
        return "ERROR: Invalid strategy. Use FALLBACK_CHAIN, PREFER_FAST, PREFER_RELIABLE, or ADAPTIVE\n";
    }
    
    network_->setTransportStrategy(strategy);
    return "OK: Transport strategy set to " + args + "\n";
}

std::string IPCHandler::handleNetFalconSetTransport(const std::string& args) {
    if (args.empty()) {
        return "ERROR: Transport setting required (e.g., tcp=1, relay=0)\n";
    }
    
    if (!network_) {
        return "ERROR: Network plugin not available\n";
    }
    
    size_t eqPos = args.find('=');
    if (eqPos == std::string::npos) {
        return "ERROR: Invalid format. Use transport=0|1 (e.g., tcp=1)\n";
    }
    
    std::string transport = args.substr(0, eqPos);
    std::string value = args.substr(eqPos + 1);
    bool enabled = (value == "1" || value == "true");
    
    if (transport == "tcp" || transport == "quic" || transport == "relay" || transport == "webrtc") {
        network_->setTransportEnabled(transport, enabled);
        return "OK: Transport " + transport + " " + (enabled ? "enabled" : "disabled") + "\n";
    }
    
    return "ERROR: Unknown transport. Use tcp, quic, relay, or webrtc\n";
}

// Helper method to parse JSON data from command arguments
Json::Value IPCHandler::parseJsonData(const std::string& args) {
    Json::Value data(Json::objectValue);
    
    if (!args.empty()) {
        // Try to parse the argument as JSON
        Json::Reader reader;
        if (!reader.parse(args, data)) {
            // If parsing fails, return empty object
            data = Json::Value(Json::objectValue);
        }
    }
    
    return data;
}

// Helper method to format JSON response for IPC
std::string IPCHandler::formatJsonResponse(const Json::Value& response) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string jsonStr = Json::writeString(builder, response);
    return jsonStr + "\n";
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
