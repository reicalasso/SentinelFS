#pragma once

/**
 * @file NetFalconPlugin.h
 * @brief NetFalcon network plugin main header
 * 
 * NetFalcon is a modular, multi-transport network plugin for SentinelFS.
 * 
 * Features:
 * - Multi-transport support (TCP, QUIC, WebRTC, Relay)
 * - Intelligent transport selection
 * - Secure session management
 * - Multiple discovery mechanisms
 * - Bandwidth management
 * - Connection quality monitoring
 */

#include "INetworkAPI.h"
#include "TransportRegistry.h"
#include "SessionManager.h"
#include "DiscoveryService.h"
#include "RelayTransport.h"
#include "QUICTransport.h"
#include "WebRTCTransport.h"
#include "BandwidthLimiter.h"
#include <memory>

namespace SentinelFS {

class EventBus;

namespace NetFalcon {

/**
 * @brief NetFalcon plugin configuration
 */
struct NetFalconConfig {
    // Transport settings
    TransportStrategy transportStrategy{TransportStrategy::FALLBACK_CHAIN};
    bool enableTcp{true};
    bool enableQuic{false};  // Future
    bool enableWebRtc{false}; // Future
    bool enableRelay{true};
    
    // Discovery settings
    DiscoveryConfig discovery;
    
    // Security settings
    bool encryptionEnabled{false};
    bool requireAuthentication{true};
    
    // Connection settings
    std::size_t maxConnections{64};
    bool autoReconnect{true};
    int reconnectDelayMs{5000};
    
    // Bandwidth settings
    std::size_t globalUploadLimit{0};   // 0 = unlimited
    std::size_t globalDownloadLimit{0}; // 0 = unlimited
};

/**
 * @brief NetFalcon network plugin
 * 
 * Main plugin class implementing INetworkAPI.
 * Orchestrates all NetFalcon components.
 */
class NetFalconPlugin : public INetworkAPI {
public:
    NetFalconPlugin();
    ~NetFalconPlugin() override;

    // IPlugin interface
    bool initialize(EventBus* eventBus) override;
    void shutdown() override;
    std::string getName() const override { return "NetFalcon"; }
    std::string getVersion() const override { return "1.0.0"; }

    // INetworkAPI interface - Connection management
    bool connectToPeer(const std::string& address, int port) override;
    void disconnectPeer(const std::string& peerId) override;
    bool isPeerConnected(const std::string& peerId) override;
    
    // INetworkAPI interface - Data transfer
    bool sendData(const std::string& peerId, const std::vector<uint8_t>& data) override;
    
    // INetworkAPI interface - Listening
    void startListening(int port) override;
    
    // INetworkAPI interface - Discovery
    void startDiscovery(int port) override;
    void broadcastPresence(int discoveryPort, int tcpPort) override;
    
    // INetworkAPI interface - RTT
    int measureRTT(const std::string& peerId) override;
    int getPeerRTT(const std::string& peerId) override;
    
    // INetworkAPI interface - Session
    void setSessionCode(const std::string& code) override;
    std::string getSessionCode() const override;
    
    // INetworkAPI interface - Encryption
    void setEncryptionEnabled(bool enable) override;
    bool isEncryptionEnabled() const override;
    
    // INetworkAPI interface - Bandwidth
    void setGlobalUploadLimit(std::size_t bytesPerSecond) override;
    void setGlobalDownloadLimit(std::size_t bytesPerSecond) override;
    std::string getBandwidthStats() const override;
    
    // INetworkAPI interface - Relay
    void setRelayEnabled(bool enabled) override;
    bool isRelayEnabled() const override;
    bool isRelayConnected() const override;
    std::string getLocalPeerId() const override;
    int getLocalPort() const override;
    bool connectToRelay(const std::string& host, int port, const std::string& sessionCode) override;
    void disconnectFromRelay() override;
    std::vector<RelayPeerInfo> getRelayPeers() const override;
    
    // INetworkAPI interface - Transport Strategy (NetFalcon specific)
    void setTransportStrategy(TransportStrategy strategy) override;
    TransportStrategy getTransportStrategy() const override;
    void setTransportEnabled(const std::string& transport, bool enabled) override;
    bool isTransportEnabled(const std::string& transport) const override;
    std::vector<std::string> getAvailableTransports() const override;

    // NetFalcon-specific API
    
    /**
     * @brief Set plugin configuration
     */
    void setConfig(const NetFalconConfig& config);
    NetFalconConfig getConfig() const;

    /**
     * @brief Get transport registry for advanced control
     */
    TransportRegistry* getTransportRegistry() { return &transportRegistry_; }

    /**
     * @brief Get session manager
     */
    SessionManager* getSessionManager() { return &sessionManager_; }

    /**
     * @brief Get discovery service
     */
    DiscoveryService* getDiscoveryService() { return &discoveryService_; }

    /**
     * @brief Get connected peer count
     */
    std::size_t getConnectedPeerCount() const;

    /**
     * @brief Get all connected peer IDs
     */
    std::vector<std::string> getConnectedPeers() const;

    /**
     * @brief Get connection quality for peer
     */
    ConnectionQuality getConnectionQuality(const std::string& peerId) const;

    /**
     * @brief Force transport for peer (for testing/debugging)
     */
    void forceTransport(const std::string& peerId, TransportType type);

private:
    void setupTransports();
    void setupEventHandlers();
    void handleTransportEvent(const TransportEventData& event);
    void handleDiscoveredPeer(const DiscoveredPeer& peer);
    void handleReceivedData(const std::string& peerId, const std::vector<uint8_t>& data);

    EventBus* eventBus_{nullptr};
    NetFalconConfig config_;
    
    // Core components
    TransportRegistry transportRegistry_;
    SessionManager sessionManager_;
    DiscoveryService discoveryService_;
    BandwidthManager bandwidthManager_;
    
    // Relay transport (owned separately for direct access)
    std::unique_ptr<RelayTransport> relayTransport_;
    
    // WebRTC transport
    std::unique_ptr<WebRTCTransport> webrtcTransport_;
    
    // State
    int listeningPort_{0};
    int discoveryPort_{0};
    
    // RTT cache
    mutable std::mutex rttMutex_;
    std::map<std::string, int> rttCache_;
};

} // namespace NetFalcon
} // namespace SentinelFS
