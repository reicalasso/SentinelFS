#include "TCPTransport.h"
#include "EventBus.h"
#include "BandwidthLimiter.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <sstream>

namespace SentinelFS {
namespace NetFalcon {

TCPTransport::TCPTransport(EventBus* eventBus, SessionManager* sessionManager, BandwidthManager* bandwidthManager)
    : eventBus_(eventBus)
    , sessionManager_(sessionManager)
    , bandwidthManager_(bandwidthManager)
{
}

TCPTransport::~TCPTransport() {
    shutdown();
}

bool TCPTransport::startListening(int port) {
    auto& logger = Logger::instance();
    
    logger.log(LogLevel::INFO, "NetFalcon TCP: Starting listener on port " + std::to_string(port), "TCPTransport");
    
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        logger.log(LogLevel::ERROR, "Failed to create server socket: " + std::string(strerror(errno)), "TCPTransport");
        return false;
    }
    
    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logger.log(LogLevel::ERROR, "Failed to bind: " + std::string(strerror(errno)), "TCPTransport");
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }
    
    if (listen(serverSocket_, 10) < 0) {
        logger.log(LogLevel::ERROR, "Failed to listen: " + std::string(strerror(errno)), "TCPTransport");
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }
    
    listening_ = true;
    listeningPort_ = port;
    listenThread_ = std::thread(&TCPTransport::listenLoop, this);
    
    logger.log(LogLevel::INFO, "NetFalcon TCP: Listening on port " + std::to_string(port), "TCPTransport");
    return true;
}

void TCPTransport::stopListening() {
    if (!listening_) return;
    
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "NetFalcon TCP: Stopping listener", "TCPTransport");
    
    listening_ = false;
    
    if (serverSocket_ >= 0) {
        ::shutdown(serverSocket_, SHUT_RDWR);
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    if (listenThread_.joinable()) {
        listenThread_.join();
    }
}

bool TCPTransport::connect(const std::string& address, int port, const std::string& expectedPeerId) {
    auto& logger = Logger::instance();
    
    logger.log(LogLevel::INFO, "NetFalcon TCP: Connecting to " + address + ":" + std::to_string(port), "TCPTransport");
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logger.log(LogLevel::ERROR, "Failed to create socket: " + std::string(strerror(errno)), "TCPTransport");
        return false;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
        logger.log(LogLevel::ERROR, "Invalid address: " + address, "TCPTransport");
        close(sock);
        return false;
    }
    
    if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logger.log(LogLevel::ERROR, "Connection failed: " + std::string(strerror(errno)), "TCPTransport");
        close(sock);
        return false;
    }
    
    // Perform handshake
    std::string remotePeerId;
    if (!performHandshake(sock, false, remotePeerId)) {
        logger.log(LogLevel::WARN, "Handshake failed", "TCPTransport");
        close(sock);
        return false;
    }
    
    // Check for duplicate connection
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        if (connections_.find(remotePeerId) != connections_.end()) {
            logger.log(LogLevel::INFO, "Already connected to " + remotePeerId, "TCPTransport");
            close(sock);
            return true;
        }
        
        // Ensure capacity
        if (!ensureCapacity()) {
            logger.log(LogLevel::WARN, "Connection pool full", "TCPTransport");
            close(sock);
            return false;
        }
        
        // Create connection
        auto conn = std::make_unique<TCPConnection>();
        conn->peerId = remotePeerId;
        conn->address = address;
        conn->port = port;
        conn->socket = sock;
        conn->state = ConnectionState::CONNECTED;
        conn->connectedAt = std::chrono::steady_clock::now();
        conn->lastActivity = conn->connectedAt;
        conn->isIncoming = false;
        
        connections_[remotePeerId] = std::move(conn);
        reconnectTargets_[remotePeerId] = {address, port};
    }
    
    // Start read thread
    {
        std::lock_guard<std::mutex> lock(threadMutex_);
        readThreads_[remotePeerId] = std::thread(&TCPTransport::readLoop, this, remotePeerId);
    }
    
    logger.log(LogLevel::INFO, "Connected to peer: " + remotePeerId, "TCPTransport");
    emitEvent(TransportEvent::CONNECTED, remotePeerId);
    
    // Register with session manager
    if (sessionManager_) {
        sessionManager_->registerPeer(remotePeerId);
    }
    
    return true;
}

void TCPTransport::disconnect(const std::string& peerId) {
    auto& logger = Logger::instance();
    
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        auto it = connections_.find(peerId);
        if (it != connections_.end()) {
            logger.log(LogLevel::INFO, "Disconnecting from " + peerId, "TCPTransport");
            ::shutdown(it->second->socket, SHUT_RDWR);
            close(it->second->socket);
            connections_.erase(it);
        }
    }
    
    // Cleanup thread
    {
        std::lock_guard<std::mutex> lock(threadMutex_);
        auto it = readThreads_.find(peerId);
        if (it != readThreads_.end()) {
            if (it->second.joinable()) {
                it->second.detach();
            }
            readThreads_.erase(it);
        }
    }
    
    if (sessionManager_) {
        sessionManager_->unregisterPeer(peerId);
    }
    
    emitEvent(TransportEvent::DISCONNECTED, peerId);
}

bool TCPTransport::send(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto& logger = Logger::instance();
    
    int sock = -1;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        auto it = connections_.find(peerId);
        if (it == connections_.end() || it->second->state != ConnectionState::CONNECTED) {
            logger.log(LogLevel::WARN, "Cannot send: peer not connected: " + peerId, "TCPTransport");
            return false;
        }
        sock = it->second->socket;
        it->second->lastActivity = std::chrono::steady_clock::now();
    }
    
    // Bandwidth limiting
    if (bandwidthManager_) {
        bandwidthManager_->requestUpload(peerId, sizeof(uint32_t) + data.size());
    }
    
    // Send length prefix
    uint32_t len = htonl(data.size());
    if (::send(sock, &len, sizeof(len), 0) < 0) {
        logger.log(LogLevel::ERROR, "Failed to send length: " + std::string(strerror(errno)), "TCPTransport");
        return false;
    }
    
    // Send data
    ssize_t sent = ::send(sock, data.data(), data.size(), 0);
    if (sent < 0) {
        logger.log(LogLevel::ERROR, "Failed to send data: " + std::string(strerror(errno)), "TCPTransport");
        return false;
    }
    
    MetricsCollector::instance().incrementBytesSent(sent);
    return true;
}

bool TCPTransport::isConnected(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    auto it = connections_.find(peerId);
    return it != connections_.end() && it->second->state == ConnectionState::CONNECTED;
}

ConnectionState TCPTransport::getConnectionState(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    auto it = connections_.find(peerId);
    if (it != connections_.end()) {
        return it->second->state;
    }
    return ConnectionState::DISCONNECTED;
}

ConnectionQuality TCPTransport::getConnectionQuality(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    auto it = connections_.find(peerId);
    if (it != connections_.end()) {
        return it->second->quality;
    }
    return ConnectionQuality{};
}

std::vector<std::string> TCPTransport::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    std::vector<std::string> peers;
    peers.reserve(connections_.size());
    for (const auto& [peerId, conn] : connections_) {
        if (conn->state == ConnectionState::CONNECTED) {
            peers.push_back(peerId);
        }
    }
    return peers;
}

void TCPTransport::setEventCallback(TransportEventCallback callback) {
    eventCallback_ = std::move(callback);
}

int TCPTransport::measureRTT(const std::string& peerId) {
    int sock = -1;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        auto it = connections_.find(peerId);
        if (it == connections_.end()) return -1;
        sock = it->second->socket;
    }
    
    // Simple RTT estimation using select
    auto start = std::chrono::high_resolution_clock::now();
    
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);
    
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    
    int ready = select(sock + 1, nullptr, &writefds, nullptr, &tv);
    if (ready <= 0) return -1;
    
    auto end = std::chrono::high_resolution_clock::now();
    int rtt = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Update quality
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        auto it = connections_.find(peerId);
        if (it != connections_.end()) {
            it->second->quality.rttMs = std::max(1, rtt);
            it->second->quality.lastUpdated = std::chrono::steady_clock::now();
        }
    }
    
    return std::max(1, rtt);
}

void TCPTransport::shutdown() {
    shuttingDown_ = true;
    
    stopListening();
    
    // Close all connections
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        for (auto& [_, conn] : connections_) {
            ::shutdown(conn->socket, SHUT_RDWR);
            close(conn->socket);
        }
        connections_.clear();
    }
    
    // Join all threads
    {
        std::lock_guard<std::mutex> lock(threadMutex_);
        for (auto& [_, thread] : readThreads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        readThreads_.clear();
    }
}

std::size_t TCPTransport::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return connections_.size();
}

// Private methods

void TCPTransport::listenLoop() {
    auto& logger = Logger::instance();
    
    while (listening_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket_, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(serverSocket_ + 1, &readfds, nullptr, nullptr, &tv);
        if (activity <= 0) continue;
        
        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &len);
        
        if (clientSocket >= 0) {
            char clientIpBuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIpBuf, INET_ADDRSTRLEN);
            std::string clientIp(clientIpBuf);
            int clientPort = ntohs(clientAddr.sin_port);
            
            logger.log(LogLevel::INFO, "Incoming connection from " + clientIp + ":" + std::to_string(clientPort), "TCPTransport");
            
            std::thread(&TCPTransport::handleIncomingConnection, this, clientSocket, clientIp, clientPort).detach();
        }
    }
}

void TCPTransport::handleIncomingConnection(int clientSocket, const std::string& clientIp, int clientPort) {
    auto& logger = Logger::instance();
    
    // Perform handshake
    std::string remotePeerId;
    if (!performHandshake(clientSocket, true, remotePeerId)) {
        logger.log(LogLevel::WARN, "Handshake failed for " + clientIp, "TCPTransport");
        close(clientSocket);
        return;
    }
    
    // Check for duplicate
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        if (connections_.find(remotePeerId) != connections_.end()) {
            logger.log(LogLevel::INFO, "Duplicate connection from " + remotePeerId, "TCPTransport");
            close(clientSocket);
            return;
        }
        
        if (!ensureCapacity()) {
            logger.log(LogLevel::WARN, "Connection pool full, rejecting " + remotePeerId, "TCPTransport");
            close(clientSocket);
            return;
        }
        
        auto conn = std::make_unique<TCPConnection>();
        conn->peerId = remotePeerId;
        conn->address = clientIp;
        conn->port = clientPort;
        conn->socket = clientSocket;
        conn->state = ConnectionState::CONNECTED;
        conn->connectedAt = std::chrono::steady_clock::now();
        conn->lastActivity = conn->connectedAt;
        conn->isIncoming = true;
        
        connections_[remotePeerId] = std::move(conn);
    }
    
    // Start read thread
    {
        std::lock_guard<std::mutex> lock(threadMutex_);
        readThreads_[remotePeerId] = std::thread(&TCPTransport::readLoop, this, remotePeerId);
    }
    
    logger.log(LogLevel::INFO, "Accepted connection from peer: " + remotePeerId, "TCPTransport");
    emitEvent(TransportEvent::CONNECTED, remotePeerId);
    
    if (sessionManager_) {
        sessionManager_->registerPeer(remotePeerId);
    }
}

bool TCPTransport::performHandshake(int socket, bool isServer, std::string& remotePeerId) {
    auto& logger = Logger::instance();
    
    logger.log(LogLevel::DEBUG, "Starting handshake - isServer: " + std::string(isServer ? "true" : "false"), "TCPTransport");
    
    if (!sessionManager_) {
        logger.log(LogLevel::ERROR, "No session manager for handshake", "TCPTransport");
        return false;
    }
    
    char buffer[1024];
    
    if (isServer) {
        // Receive client hello
        logger.log(LogLevel::DEBUG, "Server waiting for client HELLO", "TCPTransport");
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socket, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        
        if (select(socket + 1, &readfds, nullptr, nullptr, &tv) <= 0) {
            return false;
        }
        
        ssize_t len = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) {
            logger.log(LogLevel::DEBUG, "Failed to receive HELLO message", "TCPTransport");
            return false;
        }
        buffer[len] = '\0';
        
        std::string hello(buffer);
        logger.log(LogLevel::DEBUG, "Received: " + hello, "TCPTransport");
        
        // Parse: FALCON_HELLO|VERSION|PEER_ID|SESSION_CODE|NONCE
        if (hello.find("FALCON_HELLO|") != 0 && hello.find("SENTINEL_HELLO|") != 0) {
            logger.log(LogLevel::DEBUG, "Invalid HELLO format", "TCPTransport");
            return false;
        }
        
        std::vector<std::string> parts;
        std::stringstream ss(hello);
        std::string part;
        while (std::getline(ss, part, '|')) {
            parts.push_back(part);
        }
        
        if (parts.size() < 4) {
            logger.log(LogLevel::DEBUG, "Invalid HELLO parts count: " + std::to_string(parts.size()), "TCPTransport");
            return false;
        }
        
        remotePeerId = parts[2];
        std::string sessionCode = parts[3];
        
        logger.log(LogLevel::DEBUG, "Peer ID: " + remotePeerId + ", Session: " + sessionCode, "TCPTransport");
        
        // Verify session code
        if (!sessionManager_->verifySessionCode(sessionCode)) {
            logger.log(LogLevel::WARN, "Invalid session code from " + remotePeerId, "TCPTransport");
            std::string reject = "FALCON_REJECT|Invalid session code";
            ::send(socket, reject.c_str(), reject.length(), 0);
            return false;
        }
        
        logger.log(LogLevel::DEBUG, "Session code verified, getting local peer ID", "TCPTransport");
        
        std::string localPeerId = sessionManager_->getLocalPeerId();
        logger.log(LogLevel::DEBUG, "Got local peer ID: " + localPeerId, "TCPTransport");
        
        // Send welcome
        std::string welcome = "FALCON_WELCOME|1|" + localPeerId;
        logger.log(LogLevel::DEBUG, "Sending WELCOME message", "TCPTransport");
        
        if (::send(socket, welcome.c_str(), welcome.length(), 0) < 0) {
            logger.log(LogLevel::DEBUG, "Failed to send WELCOME", "TCPTransport");
            return false;
        }
        
        logger.log(LogLevel::DEBUG, "Handshake complete (server)", "TCPTransport");
        return true;
    } else {
        logger.log(LogLevel::DEBUG, "Client sending HELLO", "TCPTransport");
        
        // Client: send hello
        std::string sessionCode = sessionManager_->getSessionCode();
        std::string localPeerId = sessionManager_->getLocalPeerId();
        
        std::string hello = "FALCON_HELLO|1|" + localPeerId + "|" + sessionCode + "|";
        logger.log(LogLevel::DEBUG, "Sending: " + hello, "TCPTransport");
        
        if (::send(socket, hello.c_str(), hello.length(), 0) < 0) {
            logger.log(LogLevel::DEBUG, "Failed to send HELLO", "TCPTransport");
            return false;
        }
        
        logger.log(LogLevel::DEBUG, "Waiting for server response", "TCPTransport");
        
        // Receive response
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socket, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        
        if (select(socket + 1, &readfds, nullptr, nullptr, &tv) <= 0) {
            logger.log(LogLevel::DEBUG, "Timeout waiting for server response", "TCPTransport");
            return false;
        }
        
        ssize_t len = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) {
            logger.log(LogLevel::DEBUG, "Failed to receive server response", "TCPTransport");
            return false;
        }
        buffer[len] = '\0';
        
        std::string response(buffer);
        logger.log(LogLevel::DEBUG, "Received: " + response, "TCPTransport");
        
        if (response.find("FALCON_REJECT|") == 0 || response.find("SENTINEL_REJECT|") == 0) {
            logger.log(LogLevel::WARN, "Connection rejected: " + response, "TCPTransport");
            return false;
        }
        
        if (response.find("FALCON_WELCOME|") != 0 && response.find("SENTINEL_WELCOME|") != 0) {
            logger.log(LogLevel::DEBUG, "Invalid WELCOME format", "TCPTransport");
            return false;
        }
        
        // Parse welcome
        std::vector<std::string> parts;
        std::stringstream ss(response);
        std::string part;
        while (std::getline(ss, part, '|')) {
            parts.push_back(part);
        }
        
        if (parts.size() >= 3) {
            remotePeerId = parts[2];
            logger.log(LogLevel::DEBUG, "Handshake complete (client) - connected to " + remotePeerId, "TCPTransport");
        } else {
            remotePeerId = "UNKNOWN";
            logger.log(LogLevel::DEBUG, "Handshake complete but unknown peer ID", "TCPTransport");
        }
        
        return true;
    }
}

void TCPTransport::readLoop(const std::string& peerId) {
    auto& logger = Logger::instance();
    
    int sock = -1;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        auto it = connections_.find(peerId);
        if (it == connections_.end()) return;
        sock = it->second->socket;
    }
    
    while (!shuttingDown_) {
        // Read length prefix
        uint32_t netLen;
        ssize_t received = recv(sock, &netLen, sizeof(netLen), MSG_WAITALL);
        if (received <= 0) {
            if (received < 0 && !shuttingDown_) {
                logger.log(LogLevel::WARN, "Read error from " + peerId + ": " + std::string(strerror(errno)), "TCPTransport");
            }
            break;
        }
        
        uint32_t len = ntohl(netLen);
        if (len > 100 * 1024 * 1024) {  // 100MB limit
            logger.log(LogLevel::ERROR, "Message too large from " + peerId, "TCPTransport");
            break;
        }
        
        // Read data
        std::vector<uint8_t> data(len);
        received = recv(sock, data.data(), len, MSG_WAITALL);
        if (received <= 0) {
            break;
        }
        
        MetricsCollector::instance().incrementBytesReceived(received);
        
        // Bandwidth limiting
        if (bandwidthManager_) {
            bandwidthManager_->requestDownload(peerId, received);
        }
        
        // Update activity
        {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            auto it = connections_.find(peerId);
            if (it != connections_.end()) {
                it->second->lastActivity = std::chrono::steady_clock::now();
            }
        }
        
        // Emit event
        emitEvent(TransportEvent::DATA_RECEIVED, peerId, "", data);
    }
    
    // Connection closed
    cleanupConnection(peerId);
}

void TCPTransport::cleanupConnection(const std::string& peerId) {
    auto& logger = Logger::instance();
    
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        auto it = connections_.find(peerId);
        if (it != connections_.end()) {
            close(it->second->socket);
            connections_.erase(it);
        }
    }
    
    logger.log(LogLevel::INFO, "Connection closed: " + peerId, "TCPTransport");
    emitEvent(TransportEvent::DISCONNECTED, peerId);
    
    if (sessionManager_) {
        sessionManager_->unregisterPeer(peerId);
    }
}

bool TCPTransport::ensureCapacity() {
    if (connections_.size() < maxConnections_) {
        return true;
    }
    
    pruneOldestConnection();
    return connections_.size() < maxConnections_;
}

void TCPTransport::pruneOldestConnection() {
    std::string oldestPeer;
    std::chrono::steady_clock::time_point oldestTime = std::chrono::steady_clock::now();
    
    for (const auto& [peerId, conn] : connections_) {
        if (conn->lastActivity < oldestTime) {
            oldestTime = conn->lastActivity;
            oldestPeer = peerId;
        }
    }
    
    if (!oldestPeer.empty()) {
        auto it = connections_.find(oldestPeer);
        if (it != connections_.end()) {
            close(it->second->socket);
            connections_.erase(it);
        }
    }
}

void TCPTransport::emitEvent(TransportEvent event, const std::string& peerId,
                             const std::string& message, const std::vector<uint8_t>& data) {
    if (eventCallback_) {
        TransportEventData eventData;
        eventData.event = event;
        eventData.peerId = peerId;
        eventData.message = message;
        eventData.data = data;
        
        eventCallback_(eventData);
    }
    
    // Also publish to EventBus for daemon integration
    if (eventBus_) {
        switch (event) {
            case TransportEvent::CONNECTED:
                eventBus_->publish("PEER_CONNECTED", peerId + "|||");
                break;
            case TransportEvent::DISCONNECTED:
                eventBus_->publish("PEER_DISCONNECTED", peerId);
                break;
            case TransportEvent::DATA_RECEIVED:
                eventBus_->publish("DATA_RECEIVED", std::make_pair(peerId, data));
                break;
            default:
                break;
        }
    }
}

} // namespace NetFalcon
} // namespace SentinelFS
