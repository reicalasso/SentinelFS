#include "UDPDiscovery.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace SentinelFS {

UDPDiscovery::UDPDiscovery(EventBus* eventBus, const std::string& localPeerId)
    : eventBus_(eventBus)
    , localPeerId_(localPeerId)
{
}

UDPDiscovery::~UDPDiscovery() {
    stopDiscovery();
}

bool UDPDiscovery::startDiscovery(int port) {
    std::unique_lock<std::mutex> lock(discoveryMutex_);
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();

    if (running_) {
        if (currentPort_ == port) {
            logger.log(LogLevel::DEBUG, "UDP discovery already running on port " + std::to_string(port), "UDPDiscovery");
            return true;
        }

        logger.log(LogLevel::INFO,
                   "Restarting UDP discovery on new port " + std::to_string(port),
                   "UDPDiscovery");
        lock.unlock();
        stopDiscovery();
        lock.lock();
    }

    logger.log(LogLevel::INFO, "Starting UDP discovery on port " + std::to_string(port), "UDPDiscovery");
    
    discoverySocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (discoverySocket_ < 0) {
        logger.log(LogLevel::ERROR, "Failed to create discovery socket: " + std::string(strerror(errno)), "UDPDiscovery");
        metrics.incrementSyncErrors();
        return false;
    }

    // Enable broadcast
    int broadcast = 1;
    if (setsockopt(discoverySocket_, SOL_SOCKET, SO_BROADCAST, 
                   &broadcast, sizeof(broadcast)) < 0) {
        logger.log(LogLevel::ERROR, "Failed to set broadcast option: " + std::string(strerror(errno)), "UDPDiscovery");
        close(discoverySocket_);
        metrics.incrementSyncErrors();
        return false;
    }

    // Enable address reuse
    int reuse = 1;
    if (setsockopt(discoverySocket_, SOL_SOCKET, SO_REUSEADDR, 
                   &reuse, sizeof(reuse)) < 0) {
        logger.log(LogLevel::WARN, "Failed to set reuse addr option: " + std::string(strerror(errno)), "UDPDiscovery");
    }

    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(discoverySocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logger.log(LogLevel::ERROR, "Failed to bind discovery socket to port " + std::to_string(port) + ": " + std::string(strerror(errno)), "UDPDiscovery");
        close(discoverySocket_);
        metrics.incrementSyncErrors();
        return false;
    }

    running_ = true;
    currentPort_ = port;
    discoveryThread_ = std::thread(&UDPDiscovery::discoveryLoop, this);
    
    logger.log(LogLevel::INFO, "UDP discovery listening on port " + std::to_string(port), "UDPDiscovery");
    return true;
}

void UDPDiscovery::stopDiscovery() {
    std::unique_lock<std::mutex> lock(discoveryMutex_);
    if (!running_) return;
    
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Stopping UDP discovery", "UDPDiscovery");
    
    running_ = false;
    
    if (discoverySocket_ >= 0) {
        ::shutdown(discoverySocket_, SHUT_RDWR);
        close(discoverySocket_);
        discoverySocket_ = -1;
    }
    currentPort_ = -1;
    
    std::thread localThread;
    std::swap(localThread, discoveryThread_);
    lock.unlock();
    if (localThread.joinable()) {
        localThread.join();
    }
    
    logger.log(LogLevel::INFO, "UDP discovery stopped", "UDPDiscovery");
}

void UDPDiscovery::broadcastPresence(int discoveryPort, int tcpPort) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::DEBUG, "Broadcasting presence on port " + std::to_string(discoveryPort), "UDPDiscovery");
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        logger.log(LogLevel::ERROR, "Failed to create broadcast socket: " + std::string(strerror(errno)), "UDPDiscovery");
        metrics.incrementSyncErrors();
        return;
    }

    // Enable broadcast
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        logger.log(LogLevel::WARN, "Failed to set broadcast option: " + std::string(strerror(errno)), "UDPDiscovery");
    }

    // Setup broadcast address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(discoveryPort);
    addr.sin_addr.s_addr = INADDR_BROADCAST;

    // Format: SENTINEL_DISCOVERY|PEER_ID|TCP_PORT
    std::string msg = "SENTINEL_DISCOVERY|" + localPeerId_ + "|" + std::to_string(tcpPort);
    
    ssize_t sent = sendto(sock, msg.c_str(), msg.length(), 0, 
                          (struct sockaddr*)&addr, sizeof(addr));
    
    if (sent < 0) {
        logger.log(LogLevel::ERROR, "Failed to broadcast presence: " + std::string(strerror(errno)), "UDPDiscovery");
        metrics.incrementSyncErrors();
    } else {
        logger.log(LogLevel::INFO, "Broadcast sent: " + msg + " to port " + std::to_string(discoveryPort), "UDPDiscovery");
        metrics.incrementBytesSent(sent);
    }
    
    close(sock);
}

void UDPDiscovery::discoveryLoop() {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::DEBUG, "UDP discovery loop started", "UDPDiscovery");
    
    char buffer[1024];
    struct sockaddr_in senderAddr;
    socklen_t senderLen = sizeof(senderAddr);

    while (running_) {
        int len = recvfrom(discoverySocket_, buffer, sizeof(buffer) - 1, 0, 
                          (struct sockaddr*)&senderAddr, &senderLen);
        
        if (len > 0) {
            buffer[len] = '\0';
            std::string msg(buffer);
            
            char senderIpBuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(senderAddr.sin_addr), senderIpBuf, INET_ADDRSTRLEN);
            std::string senderIp(senderIpBuf);
            
            logger.log(LogLevel::DEBUG, "Received broadcast: " + msg + " from " + senderIp, "UDPDiscovery");
            metrics.incrementBytesReceived(len);
            
            handleDiscoveryMessage(msg, senderIp);
        } else if (len < 0 && running_) {
            logger.log(LogLevel::WARN, "Error receiving broadcast: " + std::string(strerror(errno)), "UDPDiscovery");
            metrics.incrementSyncErrors();
        }
    }
    
    logger.log(LogLevel::DEBUG, "UDP discovery loop ended", "UDPDiscovery");
}

void UDPDiscovery::handleDiscoveryMessage(const std::string& message, 
                                         const std::string& senderIp) {
    auto& logger = Logger::instance();
    
    // Expected format: SENTINEL_DISCOVERY|PEER_ID|TCP_PORT
    
    if (message.find("SENTINEL_DISCOVERY|") != 0) {
        logger.log(LogLevel::DEBUG, "Ignoring non-discovery message from " + senderIp, "UDPDiscovery");
        return;
    }
    
    // Parse message
    size_t firstPipe = message.find('|');
    size_t secondPipe = message.find('|', firstPipe + 1);
    
    if (secondPipe == std::string::npos) {
        logger.log(LogLevel::WARN, "Invalid discovery message format from " + senderIp + ": " + message, "UDPDiscovery");
        return;
    }
    
    std::string peerId = message.substr(firstPipe + 1, secondPipe - firstPipe - 1);
    std::string tcpPort = message.substr(secondPipe + 1);
    
    // Don't discover ourselves
    if (peerId == localPeerId_) {
        logger.log(LogLevel::DEBUG, "Ignoring self-discovery message", "UDPDiscovery");
        return;
    }
    
    logger.log(LogLevel::INFO, "Discovered peer " + peerId + " at " + senderIp + ":" + tcpPort, "UDPDiscovery");
    
    // Publish discovery event with sender IP included
    // Format: SENTINEL_DISCOVERY|PEER_ID|TCP_PORT|SENDER_IP
    if (eventBus_) {
        std::string enrichedMessage = message + "|" + senderIp;
        eventBus_->publish("PEER_DISCOVERED", enrichedMessage);
    }
}

} // namespace SentinelFS
