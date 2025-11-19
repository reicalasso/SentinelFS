#include "UDPDiscovery.h"
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
    std::cout << "Starting UDP discovery on port " << port << std::endl;
    
    discoverySocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (discoverySocket_ < 0) {
        std::cerr << "Failed to create discovery socket" << std::endl;
        return false;
    }

    // Enable broadcast
    int broadcast = 1;
    if (setsockopt(discoverySocket_, SOL_SOCKET, SO_BROADCAST, 
                   &broadcast, sizeof(broadcast)) < 0) {
        std::cerr << "Failed to set broadcast option" << std::endl;
        close(discoverySocket_);
        return false;
    }

    // Enable address reuse
    int reuse = 1;
    if (setsockopt(discoverySocket_, SOL_SOCKET, SO_REUSEADDR, 
                   &reuse, sizeof(reuse)) < 0) {
        std::cerr << "Failed to set reuse addr option" << std::endl;
    }

    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(discoverySocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind discovery socket" << std::endl;
        close(discoverySocket_);
        return false;
    }

    running_ = true;
    discoveryThread_ = std::thread(&UDPDiscovery::discoveryLoop, this);
    
    std::cout << "UDP discovery listening on port " << port << std::endl;
    return true;
}

void UDPDiscovery::stopDiscovery() {
    if (!running_) return;
    
    running_ = false;
    
    if (discoverySocket_ >= 0) {
        ::shutdown(discoverySocket_, SHUT_RDWR);
        close(discoverySocket_);
        discoverySocket_ = -1;
    }
    
    if (discoveryThread_.joinable()) {
        discoveryThread_.join();
    }
}

void UDPDiscovery::broadcastPresence(int discoveryPort, int tcpPort) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create broadcast socket" << std::endl;
        return;
    }

    // Enable broadcast
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

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
        std::cerr << "Failed to broadcast presence" << std::endl;
    } else {
        std::cout << "Broadcast sent: " << msg << " to port " << discoveryPort << std::endl;
    }
    
    close(sock);
}

void UDPDiscovery::discoveryLoop() {
    char buffer[1024];
    struct sockaddr_in senderAddr;
    socklen_t senderLen = sizeof(senderAddr);

    while (running_) {
        int len = recvfrom(discoverySocket_, buffer, sizeof(buffer) - 1, 0, 
                          (struct sockaddr*)&senderAddr, &senderLen);
        
        if (len > 0) {
            buffer[len] = '\0';
            std::string msg(buffer);
            std::string senderIp = inet_ntoa(senderAddr.sin_addr);
            
            std::cout << "Received broadcast: " << msg << " from " << senderIp << std::endl;
            
            handleDiscoveryMessage(msg, senderIp);
        }
    }
}

void UDPDiscovery::handleDiscoveryMessage(const std::string& message, 
                                         const std::string& senderIp) {
    // Expected format: SENTINEL_DISCOVERY|PEER_ID|TCP_PORT
    
    if (message.find("SENTINEL_DISCOVERY|") != 0) {
        std::cout << "Ignoring non-discovery message" << std::endl;
        return;
    }
    
    // Parse message
    size_t firstPipe = message.find('|');
    size_t secondPipe = message.find('|', firstPipe + 1);
    
    if (secondPipe == std::string::npos) {
        std::cerr << "Invalid discovery message format" << std::endl;
        return;
    }
    
    std::string peerId = message.substr(firstPipe + 1, secondPipe - firstPipe - 1);
    
    // Don't discover ourselves
    if (peerId == localPeerId_) {
        return;
    }
    
    // Publish discovery event
    if (eventBus_) {
        eventBus_->publish("PEER_DISCOVERED", message);
    }
}

} // namespace SentinelFS
