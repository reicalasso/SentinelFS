#include "TCPHandler.h"
#include "Crypto.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>

namespace SentinelFS {

TCPHandler::TCPHandler(EventBus* eventBus, HandshakeProtocol* handshake, BandwidthManager* bandwidthManager)
    : eventBus_(eventBus)
    , handshake_(handshake)
    , bandwidthManager_(bandwidthManager)
{
}

TCPHandler::~TCPHandler() {
    stopListening();
}

bool TCPHandler::startListening(int port) {
    logger_.log(LogLevel::INFO, "Starting TCP listener on port " + std::to_string(port), "TCPHandler");
    
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        logger_.log(LogLevel::ERROR, "Failed to create TCP server socket: " + std::string(strerror(errno)), "TCPHandler");
        metrics_.incrementSyncErrors();
        return false;
    }

    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logger_.log(LogLevel::ERROR, "Failed to set socket options: " + std::string(strerror(errno)), "TCPHandler");
        close(serverSocket_);
        metrics_.incrementSyncErrors();
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logger_.log(LogLevel::ERROR, "Failed to bind TCP server socket to port " + std::to_string(port) + ": " + std::string(strerror(errno)), "TCPHandler");
        close(serverSocket_);
        metrics_.incrementSyncErrors();
        return false;
    }

    if (listen(serverSocket_, 10) < 0) {
        logger_.log(LogLevel::ERROR, "Failed to listen on TCP server socket: " + std::string(strerror(errno)), "TCPHandler");
        close(serverSocket_);
        metrics_.incrementSyncErrors();
        return false;
    }

    listening_ = true;
    listeningPort_ = port;
    listenThread_ = std::thread(&TCPHandler::listenLoop, this);
    
    logger_.log(LogLevel::INFO, "TCP server listening on port " + std::to_string(port), "TCPHandler");
    return true;
}

void TCPHandler::stopListening() {
    if (!listening_) return;
    
    logger_.log(LogLevel::INFO, "Stopping TCP server", "TCPHandler");
    shuttingDown_ = true;
    listening_ = false;
    
    if (serverSocket_ >= 0) {
        ::shutdown(serverSocket_, SHUT_RDWR);
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    if (listenThread_.joinable()) {
        listenThread_.join();
    }
    
    // Close all connections to unblock read threads
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        count = connections_.size();
        for (auto& pair : connections_) {
            ::shutdown(pair.second, SHUT_RDWR);
            close(pair.second);
        }
        connections_.clear();
    }
    
    // Wait for all read threads to finish
    {
        std::lock_guard<std::mutex> lock(threadMutex_);
        for (auto& pair : readThreads_) {
            if (pair.second.joinable()) {
                pair.second.join();
            }
        }
        readThreads_.clear();
    }
    
    logger_.log(LogLevel::INFO, "TCP server stopped, closed " + std::to_string(count) + " connections", "TCPHandler");
}

void TCPHandler::cleanupThread(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(threadMutex_);
    auto it = readThreads_.find(peerId);
    if (it != readThreads_.end()) {
        if (it->second.joinable()) {
            it->second.detach();  // Detach completed thread
        }
        readThreads_.erase(it);
    }
}

void TCPHandler::listenLoop() {
    while (listening_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket_, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(serverSocket_ + 1, &readfds, NULL, NULL, &tv);
        if (activity <= 0) continue;
        
        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &len);
        
        if (clientSocket >= 0) {
            char clientIpBuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIpBuf, INET_ADDRSTRLEN);
            std::string clientIp(clientIpBuf);

            logger_.log(LogLevel::INFO, "New connection from " + clientIp, "TCPHandler");
            metrics_.incrementConnections();
            
            // Handle client in a new thread - handshake will add to readThreads_
            std::thread(&TCPHandler::handleClient, this, clientSocket).detach();
        }
    }
}

void TCPHandler::handleClient(int clientSocket) {
    logger_.log(LogLevel::DEBUG, "Handling new client connection", "TCPHandler");
    
    // Perform server-side handshake
    auto result = handshake_->performServerHandshake(clientSocket);
    
    if (!result.success) {
        logger_.log(LogLevel::WARN, "Handshake failed: " + result.errorMessage, "TCPHandler");
        metrics_.incrementSyncErrors();
        close(clientSocket);
        return;
    }
    
    logger_.log(LogLevel::INFO, "Handshake successful with peer: " + result.peerId, "TCPHandler");
    
    // Store connection
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        connections_[result.peerId] = clientSocket;
    }
    
    // Publish peer connected event
    if (eventBus_) {
        eventBus_->publish("PEER_CONNECTED", result.peerId);
    }
    
    // Start reading from this peer - track thread for graceful shutdown
    {
        std::lock_guard<std::mutex> lock(threadMutex_);
        // Clean up any existing thread for this peer
        auto it = readThreads_.find(result.peerId);
        if (it != readThreads_.end() && it->second.joinable()) {
            it->second.detach();
        }
        readThreads_[result.peerId] = std::thread(&TCPHandler::readLoop, this, clientSocket, result.peerId);
    }
}

bool TCPHandler::connectToPeer(const std::string& address, int port) {
    logger_.log(LogLevel::INFO, "Connecting to peer " + address + ":" + std::to_string(port), "TCPHandler");
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logger_.log(LogLevel::ERROR, "Failed to create socket: " + std::string(strerror(errno)), "TCPHandler");
        metrics_.incrementSyncErrors();
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
        logger_.log(LogLevel::ERROR, "Invalid address: " + address, "TCPHandler");
        close(sock);
        metrics_.incrementSyncErrors();
        return false;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logger_.log(LogLevel::ERROR, "Failed to connect to " + address + ": " + std::string(strerror(errno)), "TCPHandler");
        close(sock);
        metrics_.incrementSyncErrors();
        return false;
    }

    logger_.log(LogLevel::DEBUG, "TCP connection established to " + address, "TCPHandler");
    
    // Perform client-side handshake
    auto result = handshake_->performClientHandshake(sock);
    
    if (!result.success) {
        logger_.log(LogLevel::WARN, "Handshake failed: " + result.errorMessage, "TCPHandler");
        close(sock);
        metrics_.incrementSyncErrors();
        return false;
    }
    
    logger_.log(LogLevel::INFO, "Successfully connected to peer: " + result.peerId, "TCPHandler");
    metrics_.incrementConnections();
    
    // Store connection
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        connections_[result.peerId] = sock;
    }
    
    // Publish peer connected event
    if (eventBus_) {
        eventBus_->publish("PEER_CONNECTED", result.peerId);
    }
    
    // Start reading from this peer - track thread for graceful shutdown
    {
        std::lock_guard<std::mutex> lock(threadMutex_);
        auto it = readThreads_.find(result.peerId);
        if (it != readThreads_.end() && it->second.joinable()) {
            it->second.detach();
        }
        readThreads_[result.peerId] = std::thread(&TCPHandler::readLoop, this, sock, result.peerId);
    }
    
    return true;
}

bool TCPHandler::sendData(const std::string& peerId, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    
    auto it = connections_.find(peerId);
    if (it == connections_.end()) {
        logger_.log(LogLevel::WARN, "Cannot send data, peer not connected: " + peerId, "TCPHandler");
        metrics_.incrementSyncErrors();
        return false;
    }
    
    int sock = it->second;
    logger_.log(LogLevel::DEBUG, "Sending " + std::to_string(data.size()) + " bytes to peer " + peerId, "TCPHandler");
    
    // Rate limiting
    if (bandwidthManager_) {
        size_t totalSize = sizeof(uint32_t) + data.size();
        bandwidthManager_->requestUpload(peerId, totalSize);
    }

    // Send length prefix (network byte order)
    uint32_t len = htonl(data.size());
    if (send(sock, &len, sizeof(len), 0) < 0) {
        logger_.log(LogLevel::ERROR, "Failed to send length prefix to " + peerId + ": " + std::string(strerror(errno)), "TCPHandler");
        metrics_.incrementSyncErrors();
        return false;
    }

    // Send data
    ssize_t sent = send(sock, data.data(), data.size(), 0);
    if (sent < 0) {
        logger_.log(LogLevel::ERROR, "Failed to send data to " + peerId + ": " + std::string(strerror(errno)), "TCPHandler");
        metrics_.incrementSyncErrors();
        return false;
    }
    
    logger_.log(LogLevel::DEBUG, "Successfully sent " + std::to_string(sent) + " bytes to peer " + peerId, "TCPHandler");
    metrics_.incrementBytesSent(sent);
    return true;
}

void TCPHandler::disconnectPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    
    auto it = connections_.find(peerId);
    if (it != connections_.end()) {
        logger_.log(LogLevel::INFO, "Disconnecting from peer: " + peerId, "TCPHandler");
        close(it->second);
        connections_.erase(it);
        metrics_.incrementDisconnections();
    } else {
        logger_.log(LogLevel::DEBUG, "Peer already disconnected: " + peerId, "TCPHandler");
    }
}

bool TCPHandler::isPeerConnected(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return connections_.find(peerId) != connections_.end();
}

std::vector<std::string> TCPHandler::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    
    std::vector<std::string> peers;
    peers.reserve(connections_.size());
    
    for (const auto& pair : connections_) {
        peers.push_back(pair.first);
    }
    
    return peers;
}

int TCPHandler::measureRTT(const std::string& peerId) {
    int sock = -1;
    
    // Get socket under lock, then release before blocking operations
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        auto it = connections_.find(peerId);
        if (it == connections_.end()) {
            logger_.log(LogLevel::DEBUG, "Cannot measure RTT, peer not connected: " + peerId, "TCPHandler");
            return -1;
        }
        sock = it->second;
    }
    
    logger_.log(LogLevel::DEBUG, "Measuring RTT to peer: " + peerId, "TCPHandler");
    
    // Use select() for timeout instead of modifying socket options
    // This avoids race condition with readLoop using the same socket
    auto start = std::chrono::high_resolution_clock::now();
    
    // Send length-prefixed PING through normal protocol to avoid interfering with readLoop
    // For now, we estimate RTT based on connection responsiveness using select()
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);
    
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    
    int ready = select(sock + 1, nullptr, &writefds, nullptr, &tv);
    if (ready <= 0) {
        logger_.log(LogLevel::WARN, "RTT measurement: socket not ready for peer: " + peerId, "TCPHandler");
        return -1;
    }
    
    // Socket is writable - measure time to confirm connection is alive
    // Note: This is a simplified RTT estimation. For accurate RTT, implement
    // a proper PING/PONG protocol at the application layer with sequence numbers.
    auto end = std::chrono::high_resolution_clock::now();
    int rtt = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Minimum RTT floor for local connections
    if (rtt < 1) rtt = 1;
    
    logger_.log(LogLevel::DEBUG, "RTT estimate to " + peerId + ": " + std::to_string(rtt) + "ms", "TCPHandler");
    return rtt;
}

void TCPHandler::readLoop(int sock, const std::string& remotePeerId) {
    logger_.log(LogLevel::DEBUG, "Starting read loop for peer: " + remotePeerId, "TCPHandler");
    
    while (true) {
        // Read length prefix
        uint32_t netLen;
        ssize_t received = recv(sock, &netLen, sizeof(netLen), MSG_WAITALL);
        if (received <= 0) {
            if (received < 0) {
                logger_.log(LogLevel::WARN, "Error reading from " + remotePeerId + ": " + std::string(strerror(errno)), "TCPHandler");
                metrics_.incrementSyncErrors();
            }
            break;
        }

        uint32_t len = ntohl(netLen);
        logger_.log(LogLevel::DEBUG, "Receiving " + std::to_string(len) + " bytes from " + remotePeerId, "TCPHandler");
        
        // Read data
        std::vector<uint8_t> data(len);
        received = recv(sock, data.data(), len, MSG_WAITALL);
        if (received <= 0) {
            if (received < 0) {
                logger_.log(LogLevel::ERROR, "Error reading data from " + remotePeerId + ": " + std::string(strerror(errno)), "TCPHandler");
                metrics_.incrementSyncErrors();
            }
            break;
        }

        logger_.log(LogLevel::DEBUG, "Successfully received " + std::to_string(received) + " bytes from " + remotePeerId, "TCPHandler");
        metrics_.incrementBytesReceived(received);
        
        // Rate limiting
        if (bandwidthManager_) {
            bandwidthManager_->requestDownload(remotePeerId, received);
        }

        // Notify via callback (NetworkPlugin handles EventBus publishing)
        // NOTE: Do NOT publish directly to EventBus here - callback already does that
        // This prevents duplicate DATA_RECEIVED events
        if (dataCallback_) {
            dataCallback_(remotePeerId, data);
        }
    }
    
    // Connection closed
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        connections_.erase(remotePeerId);
    }
    
    close(sock);
    logger_.log(LogLevel::INFO, "Connection closed from peer: " + remotePeerId, "TCPHandler");
    metrics_.incrementDisconnections();
    
    // Publish disconnect event so peer can be removed from storage
    if (eventBus_) {
        eventBus_->publish("PEER_DISCONNECTED", remotePeerId);
    }
}

} // namespace SentinelFS
