#include "IPCHandler.h"
#include "SessionCode.h"
#include "MetricsCollector.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

namespace SentinelFS {

IPCHandler::IPCHandler(const std::string& socketPath,
                       INetworkAPI* network,
                       IStorageAPI* storage,
                       IFileAPI* filesystem)
    : socketPath_(socketPath)
    , network_(network)
    , storage_(storage)
    , filesystem_(filesystem)
{
}

IPCHandler::~IPCHandler() {
    stop();
}

bool IPCHandler::start() {
    if (running_) return true;
    
    // Remove old socket if exists
    unlink(socketPath_.c_str());
    
    // Create Unix domain socket
    serverSocket_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        std::cerr << "IPC: Cannot create socket" << std::endl;
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);
    
    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "IPC: Cannot bind socket" << std::endl;
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
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
        
        handleClient(clientSocket);
        close(clientSocket);
    }
}

void IPCHandler::handleClient(int clientSocket) {
    char buffer[1024];
    ssize_t n = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (n > 0) {
        buffer[n] = '\0';
        std::string command(buffer);
        std::string response = processCommand(command);
        send(clientSocket, response.c_str(), response.length(), 0);
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
    } else if (cmd == "PEERS") {
        return handleListCommand();
    } else if (cmd == "PAUSE") {
        return handlePauseCommand();
    } else if (cmd == "RESUME") {
        return handleResumeCommand();
    } else if (cmd == "CONNECT") {
        return handleConnectCommand(args);
    } else if (cmd == "UPLOAD-LIMIT") {
        return handleUploadLimitCommand(args);
    } else if (cmd == "DOWNLOAD-LIMIT") {
        return handleDownloadLimitCommand(args);
    } else if (cmd == "METRICS") {
        return handleMetricsCommand();
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
        } catch (...) {
            return "Invalid conflict ID.\n";
        }
        
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
    ss << "Sync Status: ENABLED\n";  // TODO: Track sync status properly
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
    } catch (...) {
        return "Invalid port number.\n";
    }
    
    if (network_->connectToPeer(ip, port)) {
        return "Connecting to " + ip + ":" + std::to_string(port) + "...\n";
    } else {
        return "Failed to initiate connection.\n";
    }
}

std::string IPCHandler::handleUploadLimitCommand(const std::string& args) {
    // TODO: Implement bandwidth limiting via network plugin interface
    return "Bandwidth limiting requires daemon restart with --upload-limit flag\n";
}

std::string IPCHandler::handleDownloadLimitCommand(const std::string& args) {
    // TODO: Implement bandwidth limiting via network plugin interface
    return "Bandwidth limiting requires daemon restart with --download-limit flag\n";
}

std::string IPCHandler::handleMetricsCommand() {
    auto& metrics = MetricsCollector::instance();
    return metrics.getMetricsSummary();
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

} // namespace SentinelFS
