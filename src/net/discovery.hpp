#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../models.hpp"

class Discovery {
public:
    Discovery(const std::string& sessionCode);
    ~Discovery();

    void start();
    void stop();
    
    std::vector<PeerInfo> getPeers() const;
    void broadcastPresence();
    void listenForPeers();
    
    void setDiscoveryInterval(int milliseconds);
    
private:
    std::string sessionCode;
    std::vector<PeerInfo> peers;
    mutable std::mutex peersMutex;
    std::atomic<bool> running{false};
    
    std::thread discoveryThread;
    std::thread listenerThread;
    int discoveryIntervalMs;
    
    // Socket-related members
    int discoverySocket;
    struct sockaddr_in discoveryAddr;
    bool initializeSocket();
    
    void discoveryLoop();
    bool sendDiscoveryPacket();
    bool receiveDiscoveryPacket();
};