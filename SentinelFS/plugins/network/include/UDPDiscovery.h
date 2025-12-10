#pragma once

#include "EventBus.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <random>

namespace SentinelFS {

/**
 * @brief UDP peer discovery manager with rate limiting
 * 
 * Handles:
 * - UDP broadcast listening
 * - Peer discovery via broadcasts
 * - Presence broadcasting with exponential backoff
 * - Rate limiting to prevent broadcast amplification
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
     * @brief Broadcast presence to network (rate limited)
     * @param discoveryPort UDP port to broadcast to
     * @param tcpPort Our TCP port for connections
     * @return true if broadcast was sent, false if rate limited
     */
    bool broadcastPresence(int discoveryPort, int tcpPort);
    
    /**
     * @brief Check if discovery is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Reset broadcast backoff (call when new peer connects)
     */
    void resetBackoff();
    
private:
    void discoveryLoop();
    void handleDiscoveryMessage(const std::string& message, const std::string& senderIp);
    int calculateBackoffMs() const;
    
    EventBus* eventBus_;
    std::string localPeerId_;
    
    int discoverySocket_{-1};
    int currentPort_{-1};
    std::atomic<bool> running_{false};
    std::thread discoveryThread_;
    std::mutex discoveryMutex_;
    
    // Rate limiting for broadcast amplification prevention
    std::chrono::steady_clock::time_point lastBroadcast_;
    std::atomic<int> consecutiveBroadcasts_{0};
    mutable std::mt19937 rng_{std::random_device{}()};
    
    // Backoff configuration
    static constexpr int BASE_INTERVAL_MS = 5000;      // 5 seconds base
    static constexpr int MAX_INTERVAL_MS = 60000;      // 60 seconds max
    static constexpr int MAX_CONSECUTIVE = 10;         // Reset after this many
};

} // namespace SentinelFS
