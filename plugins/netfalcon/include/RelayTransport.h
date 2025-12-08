#pragma once

/**
 * @file RelayTransport.h
 * @brief Relay transport implementation for NetFalcon
 * 
 * Provides NAT traversal via relay server when direct connections fail.
 */

#include "ITransport.h"
#include "SessionManager.h"
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>

namespace SentinelFS {

class EventBus;

namespace NetFalcon {

/**
 * @brief Relay message types
 */
enum class RelayMessageType : uint8_t {
    REGISTER = 0x01,
    REGISTER_ACK = 0x02,
    PEER_LIST = 0x03,
    CONNECT = 0x04,
    CONNECT_ACK = 0x05,
    DATA = 0x06,
    HEARTBEAT = 0x07,
    DISCONNECT = 0x08,
    ERROR_MSG = 0xFF
};

/**
 * @brief Relay peer info
 */
struct RelayPeerInfo {
    std::string peerId;
    std::string publicIp;
    int publicPort{0};
    bool online{false};
    std::string natType;
    std::string connectedAt;
};

/**
 * @brief Relay transport implementation
 * 
 * Connects to a relay server for NAT traversal.
 * Used as fallback when direct TCP/QUIC connections fail.
 */
class RelayTransport : public ITransport {
public:
    RelayTransport(EventBus* eventBus, SessionManager* sessionManager);
    ~RelayTransport() override;

    // ITransport interface
    TransportType getType() const override { return TransportType::RELAY; }
    std::string getName() const override { return "Relay"; }
    
    bool startListening(int port) override;
    void stopListening() override;
    int getListeningPort() const override { return 0; } // Relay doesn't listen
    
    bool connect(const std::string& address, int port, const std::string& peerId = "") override;
    void disconnect(const std::string& peerId) override;
    bool send(const std::string& peerId, const std::vector<uint8_t>& data) override;
    
    bool isConnected(const std::string& peerId) const override;
    ConnectionState getConnectionState(const std::string& peerId) const override;
    ConnectionQuality getConnectionQuality(const std::string& peerId) const override;
    std::vector<std::string> getConnectedPeers() const override;
    
    void setEventCallback(TransportEventCallback callback) override;
    int measureRTT(const std::string& peerId) override;
    void shutdown() override;

    // Relay-specific methods
    
    /**
     * @brief Connect to relay server
     * @param host Relay server hostname
     * @param port Relay server port
     * @param sessionCode Session code for peer grouping
     * @return true if connection initiated
     */
    bool connectToServer(const std::string& host, int port, const std::string& sessionCode);
    
    /**
     * @brief Disconnect from relay server
     */
    void disconnectFromServer();
    
    /**
     * @brief Check if connected to relay server
     */
    bool isServerConnected() const { return serverConnected_; }
    
    /**
     * @brief Get relay server address
     */
    std::string getServerAddress() const;
    
    /**
     * @brief Get list of peers available via relay
     */
    std::vector<RelayPeerInfo> getRelayPeers() const;
    
    /**
     * @brief Request peer list from server
     */
    void requestPeerList();

private:
    void readLoop();
    void writeLoop();
    void heartbeatLoop();
    
    bool sendMessage(RelayMessageType type, const std::vector<uint8_t>& payload);
    void handleMessage(RelayMessageType type, const std::vector<uint8_t>& payload);
    void handlePeerList(const std::vector<uint8_t>& payload);
    void handleData(const std::vector<uint8_t>& payload);
    
    void emitEvent(TransportEvent event, const std::string& peerId,
                   const std::string& message = "", const std::vector<uint8_t>& data = {});

    EventBus* eventBus_;
    SessionManager* sessionManager_;
    TransportEventCallback eventCallback_;
    
    // Server connection
    std::string serverHost_;
    int serverPort_{9000};
    int serverSocket_{-1};
    std::atomic<bool> serverConnected_{false};
    std::atomic<bool> running_{false};
    
    std::string localPeerId_;
    std::string sessionCode_;
    
    // Threads
    std::thread readThread_;
    std::thread writeThread_;
    std::thread heartbeatThread_;
    
    // Write queue
    mutable std::mutex writeMutex_;
    std::queue<std::vector<uint8_t>> writeQueue_;
    
    // Peer tracking
    mutable std::mutex peerMutex_;
    std::map<std::string, RelayPeerInfo> relayPeers_;
    std::map<std::string, ConnectionState> peerStates_;
    std::map<std::string, ConnectionQuality> peerQuality_;
    
    static constexpr int HEARTBEAT_INTERVAL_SEC = 30;
    static constexpr int RECONNECT_DELAY_SEC = 5;
    static constexpr int CONNECT_TIMEOUT_SEC = 10;
};

} // namespace NetFalcon
} // namespace SentinelFS
