#include "TCPHandler.h"
#include "../../core/include/Crypto.h"
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
    std::cout << "Starting TCP listener on port " << port << std::endl;
    
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        std::cerr << "Failed to create TCP server socket" << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options" << std::endl;
        close(serverSocket_);
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind TCP server socket" << std::endl;
        close(serverSocket_);
        return false;
    }

    if (listen(serverSocket_, 10) < 0) {
        std::cerr << "Failed to listen on TCP server socket" << std::endl;
        close(serverSocket_);
        return false;
    }

    listening_ = true;
    listenThread_ = std::thread(&TCPHandler::listenLoop, this);
    
    std::cout << "TCP server listening on port " << port << std::endl;
    return true;
}

void TCPHandler::stopListening() {
    if (!listening_) return;
    
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
    for (auto& pair : connections_) {
        close(pair.second);
    }
    connections_.clear();
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
            std::cout << "New connection from " << clientIp << std::endl;
            
            std::thread(&TCPHandler::handleClient, this, clientSocket).detach();
        }
    }
}

void TCPHandler::handleClient(int clientSocket) {
    // Perform server-side handshake
    auto result = handshake_->performServerHandshake(clientSocket);
    
    if (!result.success) {
        std::cerr << "Handshake failed: " << result.errorMessage << std::endl;
        close(clientSocket);
        return;
    }
    
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
    std::cout << "Connecting to peer " << address << ":" << port << std::endl;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << address << std::endl;
        close(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to connect to " << address << std::endl;
        close(sock);
        return false;
    }

    // Perform client-side handshake
    auto result = handshake_->performClientHandshake(sock);
    
    if (!result.success) {
        std::cerr << "Handshake failed: " << result.errorMessage << std::endl;
        close(sock);
        return false;
    }
    
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
        std::cerr << "Peer " << peerId << " not found" << std::endl;
        return false;
    }
    
    int sock = it->second;
    
    // Send length prefix (network byte order)
    uint32_t len = htonl(data.size());
    if (send(sock, &len, sizeof(len), 0) < 0) {
        std::cerr << "Failed to send length prefix to " << peerId << std::endl;
        return false;
    }

    // Send data
    ssize_t sent = send(sock, data.data(), data.size(), 0);
    if (sent < 0) {
        std::cerr << "Failed to send data to " << peerId << std::endl;
        return false;
    }
    
    std::cout << "Sent " << sent << " bytes to peer " << peerId << std::endl;
    return true;
}

void TCPHandler::disconnectPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    
    auto it = connections_.find(peerId);
    if (it != connections_.end()) {
        close(it->second);
        connections_.erase(it);
        std::cout << "Disconnected from peer " << peerId << std::endl;
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
        return -1; // Not connected
    }

    int sock = it->second;
    
    // Send PING message
    std::string ping = "PING";
    auto start = std::chrono::high_resolution_clock::now();
    
    if (send(sock, ping.c_str(), ping.length(), 0) < 0) {
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
        return -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    int rtt = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "RTT to " << peerId << ": " << rtt << "ms" << std::endl;
    return rtt;
}

void TCPHandler::readLoop(int sock, const std::string& remotePeerId) {
    while (true) {
        // Read length prefix
        uint32_t netLen;
        ssize_t received = recv(sock, &netLen, sizeof(netLen), MSG_WAITALL);
        if (received <= 0) break;

        uint32_t len = ntohl(netLen);
        
        // Read data
        std::vector<uint8_t> data(len);
        received = recv(sock, data.data(), len, MSG_WAITALL);
        if (received <= 0) break;

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
    std::cout << "Connection closed from " << remotePeerId << std::endl;
}

} // namespace SentinelFS
