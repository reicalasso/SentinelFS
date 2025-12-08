#include "RelayTransport.h"
#include "EventBus.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace SentinelFS {
namespace NetFalcon {

RelayTransport::RelayTransport(EventBus* eventBus, SessionManager* sessionManager)
    : eventBus_(eventBus)
    , sessionManager_(sessionManager)
{
}

RelayTransport::~RelayTransport() {
    shutdown();
}

bool RelayTransport::startListening(int port) {
    (void)port;
    // Relay transport doesn't listen directly - it connects to a relay server
    return true;
}

void RelayTransport::stopListening() {
    // No-op for relay
}

bool RelayTransport::connect(const std::string& address, int port, const std::string& peerId) {
    // For relay, "connect" means connecting to a peer through the relay server
    if (!serverConnected_) {
        Logger::instance().log(LogLevel::WARN, "Cannot connect to peer - not connected to relay server", "RelayTransport");
        return false;
    }
    
    // Send connect request to relay server
    std::string targetPeer = peerId.empty() ? address : peerId;
    
    std::vector<uint8_t> payload;
    payload.insert(payload.end(), targetPeer.begin(), targetPeer.end());
    
    if (sendMessage(RelayMessageType::CONNECT, payload)) {
        std::lock_guard<std::mutex> lock(peerMutex_);
        peerStates_[targetPeer] = ConnectionState::CONNECTING;
        return true;
    }
    
    return false;
}

void RelayTransport::disconnect(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(peerMutex_);
    peerStates_.erase(peerId);
    peerQuality_.erase(peerId);
    
    // Send disconnect notification
    std::vector<uint8_t> payload(peerId.begin(), peerId.end());
    sendMessage(RelayMessageType::DISCONNECT, payload);
    
    emitEvent(TransportEvent::DISCONNECTED, peerId);
}

bool RelayTransport::send(const std::string& peerId, const std::vector<uint8_t>& data) {
    if (!serverConnected_) {
        return false;
    }
    
    // Format: [peerId length (1 byte)][peerId][data]
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>(peerId.size()));
    payload.insert(payload.end(), peerId.begin(), peerId.end());
    payload.insert(payload.end(), data.begin(), data.end());
    
    return sendMessage(RelayMessageType::DATA, payload);
}

bool RelayTransport::isConnected(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(peerMutex_);
    auto it = peerStates_.find(peerId);
    return it != peerStates_.end() && it->second == ConnectionState::CONNECTED;
}

ConnectionState RelayTransport::getConnectionState(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(peerMutex_);
    auto it = peerStates_.find(peerId);
    return it != peerStates_.end() ? it->second : ConnectionState::DISCONNECTED;
}

ConnectionQuality RelayTransport::getConnectionQuality(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(peerMutex_);
    auto it = peerQuality_.find(peerId);
    return it != peerQuality_.end() ? it->second : ConnectionQuality{};
}

std::vector<std::string> RelayTransport::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(peerMutex_);
    std::vector<std::string> peers;
    for (const auto& [peerId, state] : peerStates_) {
        if (state == ConnectionState::CONNECTED) {
            peers.push_back(peerId);
        }
    }
    return peers;
}

void RelayTransport::setEventCallback(TransportEventCallback callback) {
    eventCallback_ = std::move(callback);
}

int RelayTransport::measureRTT(const std::string& peerId) {
    // For relay, RTT includes relay server latency
    // Send a ping through relay and measure response time
    auto start = std::chrono::high_resolution_clock::now();
    
    // Simple estimation based on server connection
    if (!serverConnected_) return -1;
    
    // Return cached quality if available
    std::lock_guard<std::mutex> lock(peerMutex_);
    auto it = peerQuality_.find(peerId);
    if (it != peerQuality_.end() && it->second.rttMs >= 0) {
        return it->second.rttMs;
    }
    
    // Default relay RTT estimate (higher than direct)
    return 100;
}

void RelayTransport::shutdown() {
    running_ = false;
    disconnectFromServer();
}

bool RelayTransport::connectToServer(const std::string& host, int port, const std::string& sessionCode) {
    auto& logger = Logger::instance();
    
    if (serverConnected_) {
        disconnectFromServer();
    }
    
    serverHost_ = host;
    serverPort_ = port;
    sessionCode_ = sessionCode;
    localPeerId_ = sessionManager_ ? sessionManager_->getLocalPeerId() : "UNKNOWN";
    
    logger.log(LogLevel::INFO, "Connecting to relay server " + host + ":" + std::to_string(port), "RelayTransport");
    
    // Resolve hostname
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) {
        logger.log(LogLevel::ERROR, "Failed to resolve relay server: " + host, "RelayTransport");
        return false;
    }
    
    serverSocket_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (serverSocket_ < 0) {
        freeaddrinfo(res);
        logger.log(LogLevel::ERROR, "Failed to create socket", "RelayTransport");
        return false;
    }
    
    // Set connection timeout
    struct timeval tv;
    tv.tv_sec = CONNECT_TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(serverSocket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(serverSocket_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    if (::connect(serverSocket_, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        close(serverSocket_);
        serverSocket_ = -1;
        logger.log(LogLevel::ERROR, "Failed to connect to relay server: " + std::string(strerror(errno)), "RelayTransport");
        return false;
    }
    
    freeaddrinfo(res);
    
    // Send registration
    std::ostringstream regData;
    regData << localPeerId_ << "|" << sessionCode_;
    std::string regStr = regData.str();
    std::vector<uint8_t> payload(regStr.begin(), regStr.end());
    
    if (!sendMessage(RelayMessageType::REGISTER, payload)) {
        close(serverSocket_);
        serverSocket_ = -1;
        logger.log(LogLevel::ERROR, "Failed to send registration", "RelayTransport");
        return false;
    }
    
    serverConnected_ = true;
    running_ = true;
    
    // Start threads
    readThread_ = std::thread(&RelayTransport::readLoop, this);
    writeThread_ = std::thread(&RelayTransport::writeLoop, this);
    heartbeatThread_ = std::thread(&RelayTransport::heartbeatLoop, this);
    
    logger.log(LogLevel::INFO, "Connected to relay server", "RelayTransport");
    
    // Request peer list
    requestPeerList();
    
    return true;
}

void RelayTransport::disconnectFromServer() {
    auto& logger = Logger::instance();
    
    running_ = false;
    serverConnected_ = false;
    
    if (serverSocket_ >= 0) {
        ::shutdown(serverSocket_, SHUT_RDWR);
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    if (readThread_.joinable()) readThread_.join();
    if (writeThread_.joinable()) writeThread_.join();
    if (heartbeatThread_.joinable()) heartbeatThread_.join();
    
    {
        std::lock_guard<std::mutex> lock(peerMutex_);
        relayPeers_.clear();
        peerStates_.clear();
    }
    
    logger.log(LogLevel::INFO, "Disconnected from relay server", "RelayTransport");
}

std::string RelayTransport::getServerAddress() const {
    return serverHost_ + ":" + std::to_string(serverPort_);
}

std::vector<RelayPeerInfo> RelayTransport::getRelayPeers() const {
    std::lock_guard<std::mutex> lock(peerMutex_);
    std::vector<RelayPeerInfo> peers;
    for (const auto& [_, peer] : relayPeers_) {
        peers.push_back(peer);
    }
    return peers;
}

void RelayTransport::requestPeerList() {
    sendMessage(RelayMessageType::PEER_LIST, {});
}

void RelayTransport::readLoop() {
    auto& logger = Logger::instance();
    
    while (running_ && serverConnected_) {
        // Read message header: [type (1 byte)][length (4 bytes)]
        uint8_t header[5];
        ssize_t n = recv(serverSocket_, header, 5, MSG_WAITALL);
        
        if (n <= 0) {
            if (running_) {
                logger.log(LogLevel::WARN, "Relay server connection lost", "RelayTransport");
                serverConnected_ = false;
            }
            break;
        }
        
        RelayMessageType type = static_cast<RelayMessageType>(header[0]);
        uint32_t length = (header[1] << 24) | (header[2] << 16) | (header[3] << 8) | header[4];
        
        if (length > 10 * 1024 * 1024) { // 10MB limit
            logger.log(LogLevel::ERROR, "Message too large from relay", "RelayTransport");
            continue;
        }
        
        std::vector<uint8_t> payload(length);
        if (length > 0) {
            n = recv(serverSocket_, payload.data(), length, MSG_WAITALL);
            if (n <= 0) break;
        }
        
        handleMessage(type, payload);
    }
}

void RelayTransport::writeLoop() {
    while (running_ && serverConnected_) {
        std::vector<uint8_t> message;
        
        {
            std::lock_guard<std::mutex> lock(writeMutex_);
            if (!writeQueue_.empty()) {
                message = std::move(writeQueue_.front());
                writeQueue_.pop();
            }
        }
        
        if (!message.empty()) {
            ssize_t sent = ::send(serverSocket_, message.data(), message.size(), 0);
            if (sent < 0) {
                Logger::instance().log(LogLevel::ERROR, "Failed to send to relay", "RelayTransport");
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void RelayTransport::heartbeatLoop() {
    while (running_ && serverConnected_) {
        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL_SEC));
        
        if (running_ && serverConnected_) {
            sendMessage(RelayMessageType::HEARTBEAT, {});
        }
    }
}

bool RelayTransport::sendMessage(RelayMessageType type, const std::vector<uint8_t>& payload) {
    if (serverSocket_ < 0) return false;
    
    // Format: [type (1 byte)][length (4 bytes)][payload]
    std::vector<uint8_t> message;
    message.push_back(static_cast<uint8_t>(type));
    
    uint32_t len = payload.size();
    message.push_back((len >> 24) & 0xFF);
    message.push_back((len >> 16) & 0xFF);
    message.push_back((len >> 8) & 0xFF);
    message.push_back(len & 0xFF);
    
    message.insert(message.end(), payload.begin(), payload.end());
    
    std::lock_guard<std::mutex> lock(writeMutex_);
    writeQueue_.push(std::move(message));
    return true;
}

void RelayTransport::handleMessage(RelayMessageType type, const std::vector<uint8_t>& payload) {
    auto& logger = Logger::instance();
    
    switch (type) {
        case RelayMessageType::REGISTER_ACK:
            logger.log(LogLevel::INFO, "Registration acknowledged by relay", "RelayTransport");
            break;
            
        case RelayMessageType::PEER_LIST:
            handlePeerList(payload);
            break;
            
        case RelayMessageType::CONNECT_ACK: {
            std::string peerId(payload.begin(), payload.end());
            {
                std::lock_guard<std::mutex> lock(peerMutex_);
                peerStates_[peerId] = ConnectionState::CONNECTED;
            }
            logger.log(LogLevel::INFO, "Connected to peer via relay: " + peerId, "RelayTransport");
            emitEvent(TransportEvent::CONNECTED, peerId);
            break;
        }
            
        case RelayMessageType::DATA:
            handleData(payload);
            break;
            
        case RelayMessageType::DISCONNECT: {
            std::string peerId(payload.begin(), payload.end());
            {
                std::lock_guard<std::mutex> lock(peerMutex_);
                peerStates_.erase(peerId);
                relayPeers_.erase(peerId);
            }
            emitEvent(TransportEvent::DISCONNECTED, peerId);
            break;
        }
            
        case RelayMessageType::HEARTBEAT:
            // Server heartbeat response - connection is alive
            break;
            
        case RelayMessageType::ERROR_MSG: {
            std::string error(payload.begin(), payload.end());
            logger.log(LogLevel::ERROR, "Relay error: " + error, "RelayTransport");
            break;
        }
            
        default:
            logger.log(LogLevel::WARN, "Unknown relay message type: " + std::to_string(static_cast<int>(type)), "RelayTransport");
            break;
    }
}

void RelayTransport::handlePeerList(const std::vector<uint8_t>& payload) {
    auto& logger = Logger::instance();
    
    // Parse peer list: peer1|ip1|port1|natType1\npeer2|ip2|port2|natType2\n...
    std::string data(payload.begin(), payload.end());
    std::istringstream stream(data);
    std::string line;
    
    std::lock_guard<std::mutex> lock(peerMutex_);
    relayPeers_.clear();
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        std::istringstream lineStream(line);
        std::string peerId, ip, portStr, natType;
        
        if (std::getline(lineStream, peerId, '|') &&
            std::getline(lineStream, ip, '|') &&
            std::getline(lineStream, portStr, '|') &&
            std::getline(lineStream, natType)) {
            
            if (peerId == localPeerId_) continue; // Skip self
            
            RelayPeerInfo peer;
            peer.peerId = peerId;
            peer.publicIp = ip;
            peer.publicPort = std::stoi(portStr);
            peer.natType = natType;
            peer.online = true;
            
            // Get current time as ISO string
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::ostringstream oss;
            oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
            peer.connectedAt = oss.str();
            
            relayPeers_[peerId] = peer;
            peerStates_[peerId] = ConnectionState::CONNECTED;
            
            logger.log(LogLevel::DEBUG, "Relay peer: " + peerId + " at " + ip, "RelayTransport");
        }
    }
    
    logger.log(LogLevel::INFO, "Received " + std::to_string(relayPeers_.size()) + " peers from relay", "RelayTransport");
}

void RelayTransport::handleData(const std::vector<uint8_t>& payload) {
    if (payload.size() < 2) return;
    
    // Format: [peerId length (1 byte)][peerId][data]
    uint8_t peerIdLen = payload[0];
    if (payload.size() < 1 + peerIdLen) return;
    
    std::string fromPeerId(payload.begin() + 1, payload.begin() + 1 + peerIdLen);
    std::vector<uint8_t> data(payload.begin() + 1 + peerIdLen, payload.end());
    
    MetricsCollector::instance().incrementBytesReceived(data.size());
    
    emitEvent(TransportEvent::DATA_RECEIVED, fromPeerId, "", data);
}

void RelayTransport::emitEvent(TransportEvent event, const std::string& peerId,
                               const std::string& message, const std::vector<uint8_t>& data) {
    if (eventCallback_) {
        TransportEventData eventData;
        eventData.event = event;
        eventData.peerId = peerId;
        eventData.message = message;
        eventData.data = data;
        eventCallback_(eventData);
    }
    
    if (eventBus_) {
        switch (event) {
            case TransportEvent::CONNECTED:
                eventBus_->publish("RELAY_PEER_CONNECTED", peerId);
                break;
            case TransportEvent::DISCONNECTED:
                eventBus_->publish("RELAY_PEER_DISCONNECTED", peerId);
                break;
            case TransportEvent::DATA_RECEIVED:
                eventBus_->publish("RELAY_DATA_RECEIVED", std::make_pair(peerId, data));
                break;
            default:
                break;
        }
    }
}

} // namespace NetFalcon
} // namespace SentinelFS
