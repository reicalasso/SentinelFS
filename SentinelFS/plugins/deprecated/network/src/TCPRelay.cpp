#include "TCPRelay.h"
#include "Logger.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <chrono>

namespace SentinelFS {

TCPRelay::TCPRelay(const std::string& serverHost, int serverPort)
    : serverHost_(serverHost), serverPort_(serverPort) {
}

TCPRelay::~TCPRelay() {
    disconnect();
}

void TCPRelay::setEnabled(bool enabled) {
    enabled_ = enabled;
    auto& logger = Logger::instance();
    
    if (enabled && !connected_ && !localPeerId_.empty()) {
        logger.info("TCP Relay enabled, connecting to " + getServerAddress(), "TCPRelay");
        // Reconnect if we have credentials
        connect(localPeerId_, sessionCode_);
    } else if (!enabled && connected_) {
        logger.info("TCP Relay disabled, disconnecting", "TCPRelay");
        disconnect();
    }
}

bool TCPRelay::connect(const std::string& localPeerId, const std::string& sessionCode) {
    if (!enabled_) {
        return false;
    }
    
    auto& logger = Logger::instance();
    
    localPeerId_ = localPeerId;
    sessionCode_ = sessionCode;
    
    // Resolve hostname
    struct addrinfo hints{}, *result;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    std::string portStr = std::to_string(serverPort_);
    int status = getaddrinfo(serverHost_.c_str(), portStr.c_str(), &hints, &result);
    if (status != 0) {
        logger.warn("Failed to resolve relay server: " + serverHost_ + " - " + gai_strerror(status), "TCPRelay");
        return false;
    }
    
    // Create socket
    socket_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (socket_ < 0) {
        logger.error("Failed to create relay socket: " + std::string(strerror(errno)), "TCPRelay");
        freeaddrinfo(result);
        return false;
    }
    
    // Set non-blocking for connect with timeout
    int flags = fcntl(socket_, F_GETFL, 0);
    fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
    
    // Connect
    int connectResult = ::connect(socket_, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
    
    if (connectResult < 0 && errno != EINPROGRESS) {
        logger.warn("Failed to connect to relay server: " + std::string(strerror(errno)), "TCPRelay");
        close(socket_);
        socket_ = -1;
        return false;
    }
    
    // Wait for connection with timeout
    struct pollfd pfd;
    pfd.fd = socket_;
    pfd.events = POLLOUT;
    
    int pollResult = poll(&pfd, 1, CONNECT_TIMEOUT_SEC * 1000);
    if (pollResult <= 0) {
        logger.warn("Relay connection timeout", "TCPRelay");
        close(socket_);
        socket_ = -1;
        return false;
    }
    
    // Check for connection error
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &len);
    if (error != 0) {
        logger.warn("Relay connection failed: " + std::string(strerror(error)), "TCPRelay");
        close(socket_);
        socket_ = -1;
        return false;
    }
    
    // Set back to blocking
    fcntl(socket_, F_SETFL, flags);
    
    connected_ = true;
    running_ = true;
    
    logger.info("Connected to relay server: " + getServerAddress(), "TCPRelay");
    
    // Send registration
    std::vector<uint8_t> regPayload;
    // Format: peerId length (1 byte) + peerId + sessionCode length (1 byte) + sessionCode
    regPayload.push_back(static_cast<uint8_t>(localPeerId_.size()));
    regPayload.insert(regPayload.end(), localPeerId_.begin(), localPeerId_.end());
    regPayload.push_back(static_cast<uint8_t>(sessionCode_.size()));
    regPayload.insert(regPayload.end(), sessionCode_.begin(), sessionCode_.end());
    
    sendMessage(RelayMessageType::REGISTER, regPayload);
    
    // Start threads
    readThread_ = std::thread(&TCPRelay::readLoop, this);
    writeThread_ = std::thread(&TCPRelay::writeLoop, this);
    heartbeatThread_ = std::thread(&TCPRelay::heartbeatLoop, this);
    
    return true;
}

void TCPRelay::disconnect() {
    running_ = false;
    connected_ = false;
    
    if (socket_ >= 0) {
        shutdown(socket_, SHUT_RDWR);
        close(socket_);
        socket_ = -1;
    }
    
    if (readThread_.joinable()) readThread_.join();
    if (writeThread_.joinable()) writeThread_.join();
    if (heartbeatThread_.joinable()) heartbeatThread_.join();
    
    std::lock_guard<std::mutex> lock(mutex_);
    relayPeers_.clear();
}

bool TCPRelay::sendToPeer(const std::string& peerId, const std::vector<uint8_t>& data) {
    if (!connected_ || !enabled_) return false;
    
    // Format: target peerId length (1 byte) + peerId + data
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>(peerId.size()));
    payload.insert(payload.end(), peerId.begin(), peerId.end());
    payload.insert(payload.end(), data.begin(), data.end());
    
    return sendMessage(RelayMessageType::DATA, payload);
}

void TCPRelay::requestPeerList() {
    if (!connected_ || !enabled_) return;
    sendMessage(RelayMessageType::PEER_LIST, {});
}

void TCPRelay::setDataCallback(RelayDataCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    dataCallback_ = std::move(callback);
}

void TCPRelay::setPeerCallback(RelayPeerCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    peerCallback_ = std::move(callback);
}

std::vector<RelayPeer> TCPRelay::getRelayPeers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RelayPeer> peers;
    for (const auto& [id, peer] : relayPeers_) {
        peers.push_back(peer);
    }
    return peers;
}

bool TCPRelay::sendMessage(RelayMessageType type, const std::vector<uint8_t>& payload) {
    if (socket_ < 0) return false;
    
    // Message format: type (1 byte) + length (4 bytes) + payload
    std::vector<uint8_t> message;
    message.push_back(static_cast<uint8_t>(type));
    
    uint32_t len = static_cast<uint32_t>(payload.size());
    uint32_t netLen = htonl(len);
    message.insert(message.end(), 
        reinterpret_cast<uint8_t*>(&netLen), 
        reinterpret_cast<uint8_t*>(&netLen) + 4);
    
    message.insert(message.end(), payload.begin(), payload.end());
    
    std::lock_guard<std::mutex> lock(writeMutex_);
    writeQueue_.push(std::move(message));
    return true;
}

void TCPRelay::readLoop() {
    auto& logger = Logger::instance();
    
    while (running_ && socket_ >= 0) {
        // Read message header (type + length)
        uint8_t header[5];
        ssize_t received = recv(socket_, header, 5, MSG_WAITALL);
        
        if (received <= 0) {
            if (running_) {
                logger.warn("Relay connection lost", "TCPRelay");
                reconnect();
            }
            break;
        }
        
        RelayMessageType type = static_cast<RelayMessageType>(header[0]);
        uint32_t netLen;
        memcpy(&netLen, header + 1, 4);
        uint32_t len = ntohl(netLen);
        
        // Sanity check
        if (len > 10 * 1024 * 1024) { // Max 10MB
            logger.error("Invalid relay message length: " + std::to_string(len), "TCPRelay");
            continue;
        }
        
        // Read payload
        std::vector<uint8_t> payload(len);
        if (len > 0) {
            received = recv(socket_, payload.data(), len, MSG_WAITALL);
            if (received <= 0) {
                if (running_) {
                    logger.warn("Relay connection lost during payload read", "TCPRelay");
                    reconnect();
                }
                break;
            }
        }
        
        handleMessage(type, payload);
    }
}

void TCPRelay::writeLoop() {
    while (running_ && socket_ >= 0) {
        std::vector<uint8_t> message;
        
        {
            std::lock_guard<std::mutex> lock(writeMutex_);
            if (!writeQueue_.empty()) {
                message = std::move(writeQueue_.front());
                writeQueue_.pop();
            }
        }
        
        if (!message.empty()) {
            ssize_t sent = send(socket_, message.data(), message.size(), MSG_NOSIGNAL);
            if (sent < 0) {
                Logger::instance().warn("Failed to send relay message: " + std::string(strerror(errno)), "TCPRelay");
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void TCPRelay::heartbeatLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL_SEC));
        
        if (running_ && connected_) {
            sendMessage(RelayMessageType::HEARTBEAT, {});
        }
    }
}

void TCPRelay::handleMessage(RelayMessageType type, const std::vector<uint8_t>& payload) {
    auto& logger = Logger::instance();
    
    switch (type) {
        case RelayMessageType::REGISTER_ACK:
            logger.info("Registered with relay server", "TCPRelay");
            // Request peer list after registration
            requestPeerList();
            break;
            
        case RelayMessageType::PEER_LIST: {
            // Parse peer list
            // Format: count (1 byte) + [peerId len (1) + peerId + ip len (1) + ip + port (2)] * count
            if (payload.empty()) break;
            
            size_t offset = 0;
            uint8_t count = payload[offset++];
            
            std::lock_guard<std::mutex> lock(mutex_);
            relayPeers_.clear();
            
            for (uint8_t i = 0; i < count && offset < payload.size(); ++i) {
                RelayPeer peer;
                
                uint8_t idLen = payload[offset++];
                if (offset + idLen > payload.size()) break;
                peer.peerId = std::string(payload.begin() + offset, payload.begin() + offset + idLen);
                offset += idLen;
                
                uint8_t ipLen = payload[offset++];
                if (offset + ipLen > payload.size()) break;
                peer.publicIp = std::string(payload.begin() + offset, payload.begin() + offset + ipLen);
                offset += ipLen;
                
                if (offset + 2 > payload.size()) break;
                peer.publicPort = (payload[offset] << 8) | payload[offset + 1];
                offset += 2;
                
                peer.online = true;
                relayPeers_[peer.peerId] = peer;
                
                if (peerCallback_) {
                    peerCallback_(peer);
                }
            }
            
            logger.info("Received " + std::to_string(relayPeers_.size()) + " peers from relay", "TCPRelay");
            break;
        }
            
        case RelayMessageType::DATA: {
            // Parse data message
            // Format: from peerId len (1) + peerId + data
            if (payload.size() < 2) break;
            
            size_t offset = 0;
            uint8_t idLen = payload[offset++];
            if (offset + idLen > payload.size()) break;
            
            std::string fromPeerId(payload.begin() + offset, payload.begin() + offset + idLen);
            offset += idLen;
            
            std::vector<uint8_t> data(payload.begin() + offset, payload.end());
            
            logger.debug("Received " + std::to_string(data.size()) + " bytes from " + fromPeerId + " via relay", "TCPRelay");
            
            if (dataCallback_) {
                dataCallback_(fromPeerId, data);
            }
            break;
        }
            
        case RelayMessageType::DISCONNECT: {
            // Peer disconnected
            if (payload.empty()) break;
            
            uint8_t idLen = payload[0];
            if (1 + idLen > payload.size()) break;
            
            std::string peerId(payload.begin() + 1, payload.begin() + 1 + idLen);
            
            std::lock_guard<std::mutex> lock(mutex_);
            relayPeers_.erase(peerId);
            
            logger.info("Peer disconnected from relay: " + peerId, "TCPRelay");
            break;
        }
            
        case RelayMessageType::ERROR: {
            std::string error(payload.begin(), payload.end());
            logger.error("Relay error: " + error, "TCPRelay");
            break;
        }
            
        case RelayMessageType::HEARTBEAT:
            // Server heartbeat response - connection is alive
            break;
            
        default:
            logger.warn("Unknown relay message type: " + std::to_string(static_cast<int>(type)), "TCPRelay");
            break;
    }
}

void TCPRelay::reconnect() {
    auto& logger = Logger::instance();
    
    connected_ = false;
    
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
    
    if (!enabled_ || !running_) return;
    
    logger.info("Attempting to reconnect to relay server in " + std::to_string(RECONNECT_DELAY_SEC) + "s", "TCPRelay");
    std::this_thread::sleep_for(std::chrono::seconds(RECONNECT_DELAY_SEC));
    
    if (running_ && enabled_) {
        connect(localPeerId_, sessionCode_);
    }
}

} // namespace SentinelFS
