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

TCPHandler::TCPHandler(EventBus* eventBus, HandshakeProtocol* handshake)
    : eventBus_(eventBus)
    , handshake_(handshake)
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
    listenThread_ = std::thread(&TCPHandler::listenLoop, this);
    
    logger_.log(LogLevel::INFO, "TCP server listening on port " + std::to_string(port), "TCPHandler");
    return true;
}

void TCPHandler::stopListening() {
    if (!listening_) return;
    
    logger_.log(LogLevel::INFO, "Stopping TCP server", "TCPHandler");
    listening_ = false;
    
    if (serverSocket_ >= 0) {
        ::shutdown(serverSocket_, SHUT_RDWR);
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    if (listenThread_.joinable()) {
        listenThread_.join();
    }
    
    // Close all connections
    std::lock_guard<std::mutex> lock(connectionMutex_);
    size_t count = connections_.size();
    for (auto& pair : connections_) {
        close(pair.second);
    }
    connections_.clear();
    
    logger_.log(LogLevel::INFO, "TCP server stopped, closed " + std::to_string(count) + " connections", "TCPHandler");
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
            std::string clientIp = inet_ntoa(clientAddr.sin_addr);
            logger_.log(LogLevel::INFO, "New connection from " + clientIp, "TCPHandler");
            metrics_.incrementConnections();
            
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
    
    // Start reading from this peer
    std::thread(&TCPHandler::readLoop, this, clientSocket, result.peerId).detach();
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
    
    // Start reading from this peer
    std::thread(&TCPHandler::readLoop, this, sock, result.peerId).detach();
    
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
    std::lock_guard<std::mutex> lock(connectionMutex_);
    
    auto it = connections_.find(peerId);
    if (it == connections_.end()) {
        logger_.log(LogLevel::DEBUG, "Cannot measure RTT, peer not connected: " + peerId, "TCPHandler");
        return -1; // Not connected
    }

    int sock = it->second;
    logger_.log(LogLevel::DEBUG, "Measuring RTT to peer: " + peerId, "TCPHandler");
    
    // Send PING message
    std::string ping = "PING";
    auto start = std::chrono::high_resolution_clock::now();
    
    if (send(sock, ping.c_str(), ping.length(), 0) < 0) {
        logger_.log(LogLevel::WARN, "Failed to send PING to " + peerId + ": " + std::string(strerror(errno)), "TCPHandler");
        return -1;
    }

    // Wait for PONG response (with timeout)
    char buffer[64];
    struct timeval tv;
    tv.tv_sec = 2;  // 2 second timeout
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    ssize_t len = recv(sock, buffer, sizeof(buffer) - 1, MSG_PEEK);
    if (len <= 0) {
        logger_.log(LogLevel::WARN, "RTT measurement timed out for peer: " + peerId, "TCPHandler");
        return -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    int rtt = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    logger_.log(LogLevel::INFO, "RTT to " + peerId + ": " + std::to_string(rtt) + "ms", "TCPHandler");
    // Note: RTT could be added to metrics if needed
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
        
        // Notify via callback
        if (dataCallback_) {
            dataCallback_(remotePeerId, data);
        }
        
        // Also publish to event bus
        if (eventBus_) {
            eventBus_->publish("DATA_RECEIVED", std::make_pair(remotePeerId, data));
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
}

} // namespace SentinelFS
