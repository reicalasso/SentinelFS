#include "DiscoveryService.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <random>

namespace SentinelFS {
namespace NetFalcon {

DiscoveryService::DiscoveryService() : config_() {}

DiscoveryService::DiscoveryService(const DiscoveryConfig& config) : config_(config) {}

DiscoveryService::~DiscoveryService() {
    stop();
}

void DiscoveryService::setLocalPeer(const std::string& peerId, int tcpPort, const std::string& sessionCode) {
    std::lock_guard<std::mutex> lock(mutex_);
    localPeerId_ = peerId;
    localTcpPort_ = tcpPort;
    localSessionCode_ = sessionCode;
}

bool DiscoveryService::start() {
    if (running_) return true;
    
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Starting NetFalcon discovery service", "DiscoveryService");
    
    running_ = true;
    
    if (config_.enableUdpBroadcast) {
        if (!startUdpDiscovery()) {
            logger.log(LogLevel::ERROR, "Failed to start UDP discovery", "DiscoveryService");
            running_ = false;
            return false;
        }
    }
    
    if (config_.enableMdns) {
        startMdns();
    }
    
    return true;
}

void DiscoveryService::stop() {
    if (!running_) return;
    
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Stopping NetFalcon discovery service", "DiscoveryService");
    
    running_ = false;
    
    stopUdpDiscovery();
    stopMdns();
    
    logger.log(LogLevel::INFO, "Discovery service stopped", "DiscoveryService");
}

void DiscoveryService::broadcastPresence() {
    if (!running_ || !config_.enableUdpBroadcast) return;
    sendUdpBroadcast();
}

std::vector<DiscoveredPeer> DiscoveryService::getDiscoveredPeers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DiscoveredPeer> result;
    result.reserve(peers_.size());
    for (const auto& [_, peer] : peers_) {
        result.push_back(peer);
    }
    return result;
}

std::optional<DiscoveredPeer> DiscoveryService::getPeer(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(peerId);
    if (it != peers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<DiscoveredPeer> DiscoveryService::getPeersBySession(const std::string& sessionCode) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DiscoveredPeer> result;
    for (const auto& [_, peer] : peers_) {
        if (peer.sessionCode == sessionCode) {
            result.push_back(peer);
        }
    }
    return result;
}

void DiscoveryService::addPeer(const DiscoveredPeer& peer) {
    std::lock_guard<std::mutex> lock(mutex_);
    peers_[peer.peerId] = peer;
    stats_.totalDiscovered++;
}

void DiscoveryService::removePeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    peers_.erase(peerId);
}

void DiscoveryService::setDiscoveryCallback(DiscoveryCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    discoveryCallback_ = std::move(callback);
}

void DiscoveryService::setConfig(const DiscoveryConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

DiscoveryConfig DiscoveryService::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void DiscoveryService::cleanupStalePeers() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(config_.peerTimeoutSec);
    
    for (auto it = peers_.begin(); it != peers_.end();) {
        if (now - it->second.lastSeen > timeout) {
            it = peers_.erase(it);
        } else {
            ++it;
        }
    }
    
    stats_.activePeers = peers_.size();
}

DiscoveryService::DiscoveryStats DiscoveryService::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    DiscoveryStats stats = stats_;
    stats.activePeers = peers_.size();
    return stats;
}

// UDP Discovery Implementation

bool DiscoveryService::startUdpDiscovery() {
    auto& logger = Logger::instance();
    
    udpSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket_ < 0) {
        logger.log(LogLevel::ERROR, "Failed to create UDP socket: " + std::string(strerror(errno)), "DiscoveryService");
        return false;
    }
    
    // Enable broadcast
    int broadcast = 1;
    if (setsockopt(udpSocket_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        logger.log(LogLevel::ERROR, "Failed to enable broadcast: " + std::string(strerror(errno)), "DiscoveryService");
        close(udpSocket_);
        udpSocket_ = -1;
        return false;
    }
    
    // Enable address reuse
    int reuse = 1;
    setsockopt(udpSocket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.udpPort);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(udpSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logger.log(LogLevel::ERROR, "Failed to bind UDP socket: " + std::string(strerror(errno)), "DiscoveryService");
        close(udpSocket_);
        udpSocket_ = -1;
        return false;
    }
    
    // Start threads
    udpListenThread_ = std::thread(&DiscoveryService::udpListenLoop, this);
    udpBroadcastThread_ = std::thread(&DiscoveryService::udpBroadcastLoop, this);
    
    logger.log(LogLevel::INFO, "UDP discovery started on port " + std::to_string(config_.udpPort), "DiscoveryService");
    return true;
}

void DiscoveryService::stopUdpDiscovery() {
    if (udpSocket_ >= 0) {
        shutdown(udpSocket_, SHUT_RDWR);
        close(udpSocket_);
        udpSocket_ = -1;
    }
    
    if (udpListenThread_.joinable()) {
        udpListenThread_.join();
    }
    
    if (udpBroadcastThread_.joinable()) {
        udpBroadcastThread_.join();
    }
}

void DiscoveryService::udpListenLoop() {
    auto& logger = Logger::instance();
    
    char buffer[1024];
    struct sockaddr_in senderAddr;
    socklen_t senderLen = sizeof(senderAddr);
    
    while (running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(udpSocket_, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(udpSocket_ + 1, &readfds, nullptr, nullptr, &tv);
        if (activity <= 0) continue;
        
        int len = recvfrom(udpSocket_, buffer, sizeof(buffer) - 1, 0,
                          (struct sockaddr*)&senderAddr, &senderLen);
        
        if (len > 0) {
            buffer[len] = '\0';
            
            char senderIpBuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(senderAddr.sin_addr), senderIpBuf, INET_ADDRSTRLEN);
            std::string senderIp(senderIpBuf);
            
            stats_.broadcastsReceived++;
            handleUdpMessage(std::string(buffer), senderIp);
        }
    }
}

void DiscoveryService::udpBroadcastLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.broadcastIntervalMs));
        
        if (running_) {
            sendUdpBroadcast();
            cleanupStalePeers();
        }
    }
}

bool DiscoveryService::sendUdpBroadcast() {
    auto& logger = Logger::instance();
    
    // Rate limiting
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastBroadcast_).count();
    
    int backoffMs = BASE_INTERVAL_MS;
    if (consecutiveBroadcasts_ > 0) {
        backoffMs = std::min(BASE_INTERVAL_MS * (1 << std::min(consecutiveBroadcasts_, MAX_CONSECUTIVE)), MAX_INTERVAL_MS);
    }
    
    if (elapsed < backoffMs) {
        return false;
    }
    
    // Create broadcast socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;
    
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.udpPort);
    addr.sin_addr.s_addr = INADDR_BROADCAST;
    
    // Format: FALCON_DISCOVERY|PEER_ID|TCP_PORT|SESSION_CODE|VERSION|PLATFORM
    std::stringstream ss;
    ss << "FALCON_DISCOVERY|" << localPeerId_ << "|" << localTcpPort_ << "|" 
       << localSessionCode_ << "|1.0.0|linux";
    
    std::string msg = ss.str();
    ssize_t sent = sendto(sock, msg.c_str(), msg.length(), 0,
                          (struct sockaddr*)&addr, sizeof(addr));
    
    close(sock);
    
    if (sent > 0) {
        lastBroadcast_ = now;
        consecutiveBroadcasts_++;
        stats_.broadcastsSent++;
        stats_.lastBroadcast = now;
        return true;
    }
    
    return false;
}

void DiscoveryService::handleUdpMessage(const std::string& message, const std::string& senderIp) {
    auto& logger = Logger::instance();
    
    // Parse: FALCON_DISCOVERY|PEER_ID|TCP_PORT|SESSION_CODE|VERSION|PLATFORM
    if (message.find("FALCON_DISCOVERY|") != 0) {
        // Also accept legacy format for compatibility
        if (message.find("SENTINEL_DISCOVERY|") != 0) {
            return;
        }
    }
    
    std::vector<std::string> parts;
    std::stringstream ss(message);
    std::string part;
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }
    
    if (parts.size() < 4) return;
    
    std::string peerId = parts[1];
    int tcpPort = std::stoi(parts[2]);
    std::string sessionCode = parts[3];
    
    // Ignore self
    if (peerId == localPeerId_) return;
    
    // Check session code match
    if (!localSessionCode_.empty() && sessionCode != localSessionCode_) {
        return;
    }
    
    DiscoveredPeer peer;
    peer.peerId = peerId;
    peer.address = senderIp;
    peer.port = tcpPort;
    peer.method = DiscoveryMethod::UDP_BROADCAST;
    peer.sessionCode = sessionCode;
    peer.discoveredAt = std::chrono::steady_clock::now();
    peer.lastSeen = peer.discoveredAt;
    
    if (parts.size() >= 5) {
        peer.metadata["version"] = parts[4];
    }
    if (parts.size() >= 6) {
        peer.metadata["platform"] = parts[5];
    }
    
    bool isNew = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = peers_.find(peerId);
        if (it == peers_.end()) {
            peers_[peerId] = peer;
            stats_.totalDiscovered++;
            isNew = true;
        } else {
            it->second.lastSeen = peer.lastSeen;
            it->second.address = senderIp;
            it->second.port = tcpPort;
        }
    }
    
    if (isNew) {
        logger.log(LogLevel::INFO, "Discovered peer " + peerId + " at " + senderIp + ":" + std::to_string(tcpPort), "DiscoveryService");
        notifyPeerDiscovered(peer);
    }
}

void DiscoveryService::notifyPeerDiscovered(const DiscoveredPeer& peer) {
    DiscoveryCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = discoveryCallback_;
    }
    
    if (callback) {
        callback(peer);
    }
}

void DiscoveryService::updatePeerLastSeen(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(peerId);
    if (it != peers_.end()) {
        it->second.lastSeen = std::chrono::steady_clock::now();
    }
}

// mDNS placeholders
// Note: mDNS discovery requires additional libraries (Avahi on Linux, Bonjour on macOS/Windows)
// This is a placeholder for future implementation when mDNS support is needed

bool DiscoveryService::startMdns() {
    // TODO: Implement mDNS discovery using Avahi (Linux) or Bonjour (macOS/Windows)
    // This would enable automatic peer discovery on local networks
    return false;
}

void DiscoveryService::stopMdns() {
    // TODO: Implement mDNS cleanup
    // Clean up mDNS service registration and discovery resources
}

} // namespace NetFalcon
} // namespace SentinelFS
