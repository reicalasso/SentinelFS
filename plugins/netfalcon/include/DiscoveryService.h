#pragma once

/**
 * @file DiscoveryService.h
 * @brief Peer discovery service for NetFalcon
 * 
 * Supports multiple discovery mechanisms:
 * - UDP broadcast (LAN)
 * - mDNS/Bonjour
 * - Relay server discovery
 */

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <optional>

namespace SentinelFS {
namespace NetFalcon {

/**
 * @brief Discovery method
 */
enum class DiscoveryMethod {
    UDP_BROADCAST,
    MDNS,
    RELAY,
    MANUAL
};

/**
 * @brief Discovered peer info
 */
struct DiscoveredPeer {
    std::string peerId;
    std::string address;
    int port{0};
    DiscoveryMethod method{DiscoveryMethod::MANUAL};
    std::string sessionCode;
    std::chrono::steady_clock::time_point discoveredAt;
    std::chrono::steady_clock::time_point lastSeen;
    int signalStrength{0};  // For proximity estimation
    std::map<std::string, std::string> metadata;  // Platform, version, etc.
};

/**
 * @brief Discovery event callback
 */
using DiscoveryCallback = std::function<void(const DiscoveredPeer&)>;

/**
 * @brief Discovery service configuration
 */
struct DiscoveryConfig {
    int udpPort{9999};
    int broadcastIntervalMs{5000};
    int peerTimeoutSec{60};
    bool enableUdpBroadcast{true};
    bool enableMdns{false};
    bool enableRelay{false};
    std::string serviceName{"_sentinelfs._tcp"};
};

/**
 * @brief Peer discovery service
 * 
 * Manages peer discovery through multiple mechanisms:
 * - UDP broadcast for LAN discovery
 * - mDNS for zero-config networking
 * - Relay server for NAT traversal
 */
class DiscoveryService {
public:
    DiscoveryService();
    explicit DiscoveryService(const DiscoveryConfig& config);
    ~DiscoveryService();

    /**
     * @brief Set local peer info for announcements
     */
    void setLocalPeer(const std::string& peerId, int tcpPort, const std::string& sessionCode);

    /**
     * @brief Start discovery services
     */
    bool start();

    /**
     * @brief Stop discovery services
     */
    void stop();

    /**
     * @brief Check if discovery is running
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Broadcast presence immediately
     */
    void broadcastPresence();

    /**
     * @brief Get all discovered peers
     */
    std::vector<DiscoveredPeer> getDiscoveredPeers() const;

    /**
     * @brief Get peer by ID
     */
    std::optional<DiscoveredPeer> getPeer(const std::string& peerId) const;

    /**
     * @brief Filter peers by session code
     */
    std::vector<DiscoveredPeer> getPeersBySession(const std::string& sessionCode) const;

    /**
     * @brief Manually add a peer
     */
    void addPeer(const DiscoveredPeer& peer);

    /**
     * @brief Remove a peer
     */
    void removePeer(const std::string& peerId);

    /**
     * @brief Set discovery callback
     */
    void setDiscoveryCallback(DiscoveryCallback callback);

    /**
     * @brief Update configuration
     */
    void setConfig(const DiscoveryConfig& config);
    DiscoveryConfig getConfig() const;

    /**
     * @brief Cleanup stale peers
     */
    void cleanupStalePeers();

    /**
     * @brief Get discovery statistics
     */
    struct DiscoveryStats {
        std::size_t totalDiscovered{0};
        std::size_t activePeers{0};
        std::size_t broadcastsSent{0};
        std::size_t broadcastsReceived{0};
        std::chrono::steady_clock::time_point lastBroadcast;
    };
    DiscoveryStats getStats() const;

private:
    // UDP broadcast
    bool startUdpDiscovery();
    void stopUdpDiscovery();
    void udpListenLoop();
    void udpBroadcastLoop();
    bool sendUdpBroadcast();
    void handleUdpMessage(const std::string& message, const std::string& senderIp);

    // mDNS (placeholder for future)
    bool startMdns();
    void stopMdns();

    // Peer management
    void notifyPeerDiscovered(const DiscoveredPeer& peer);
    void updatePeerLastSeen(const std::string& peerId);

    mutable std::mutex mutex_;
    DiscoveryConfig config_;
    
    std::string localPeerId_;
    int localTcpPort_{0};
    std::string localSessionCode_;
    
    std::atomic<bool> running_{false};
    
    // UDP discovery
    int udpSocket_{-1};
    std::thread udpListenThread_;
    std::thread udpBroadcastThread_;
    
    // Discovered peers
    std::map<std::string, DiscoveredPeer> peers_;
    
    // Callbacks
    DiscoveryCallback discoveryCallback_;
    
    // Statistics
    DiscoveryStats stats_;
    
    // Rate limiting
    std::chrono::steady_clock::time_point lastBroadcast_;
    int consecutiveBroadcasts_{0};
    
    static constexpr int BASE_INTERVAL_MS = 1000;
    static constexpr int MAX_INTERVAL_MS = 30000;
    static constexpr int MAX_CONSECUTIVE = 10;
};

} // namespace NetFalcon
} // namespace SentinelFS
