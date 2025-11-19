#pragma once

#include "EventBus.h"
#include <string>
#include <thread>
#include <atomic>

namespace SentinelFS {

/**
 * @brief UDP peer discovery manager
 * 
 * Handles:
 * - UDP broadcast listening
 * - Peer discovery via broadcasts
 * - Presence broadcasting
 */
class UDPDiscovery {
public:
    /**
     * @brief Constructor
     * @param eventBus Event bus for publishing discovered peers
     * @param localPeerId This peer's ID
     */
    UDPDiscovery(EventBus* eventBus, const std::string& localPeerId);
    
    ~UDPDiscovery();
    
    /**
     * @brief Start UDP discovery listener
     * @param port UDP port to listen on
     * @return true if started successfully
     */
    bool startDiscovery(int port);
    
    /**
     * @brief Stop UDP discovery
     */
    void stopDiscovery();
    
    /**
     * @brief Broadcast presence to network
     * @param discoveryPort UDP port to broadcast to
     * @param tcpPort Our TCP port for connections
     */
    void broadcastPresence(int discoveryPort, int tcpPort);
    
    /**
     * @brief Check if discovery is running
     */
    bool isRunning() const { return running_; }
    
private:
    void discoveryLoop();
    void handleDiscoveryMessage(const std::string& message, const std::string& senderIp);
    
    EventBus* eventBus_;
    std::string localPeerId_;
    
    int discoverySocket_{-1};
    std::atomic<bool> running_{false};
    std::thread discoveryThread_;
};

} // namespace SentinelFS
