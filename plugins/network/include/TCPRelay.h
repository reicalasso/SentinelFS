#pragma once

/**
 * @file TCPRelay.h
 * @brief TCP Relay client for NAT traversal
 * 
 * Connects to a relay server to enable peer-to-peer connections
 * when direct connections fail (e.g., behind NAT/firewall).
 */

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <map>

namespace SentinelFS {

/**
 * @brief Relay message types
 */
enum class RelayMessageType : uint8_t {
    REGISTER = 0x01,      // Register with relay server
    REGISTER_ACK = 0x02,  // Registration acknowledged
    PEER_LIST = 0x03,     // List of available peers
    CONNECT = 0x04,       // Request connection to peer
    CONNECT_ACK = 0x05,   // Connection request acknowledged
    DATA = 0x06,          // Relayed data
    HEARTBEAT = 0x07,     // Keep-alive
    DISCONNECT = 0x08,    // Peer disconnected
    ERROR = 0xFF          // Error message
};

/**
 * @brief Relay peer info
 */
struct RelayPeer {
    std::string peerId;
    std::string publicIp;
    int publicPort;
    bool online;
    std::string natType;      // NAT type: symmetric, cone, open, unknown
    std::string connectedAt;  // ISO timestamp of connection
};

/**
 * @brief Callback for received data through relay
 */
using RelayDataCallback = std::function<void(const std::string& fromPeerId, const std::vector<uint8_t>& data)>;

/**
 * @brief Callback for peer discovery through relay
 */
using RelayPeerCallback = std::function<void(const RelayPeer& peer)>;

/**
 * @brief TCP Relay client for NAT traversal
 * 
 * Usage:
 * @code
 * TCPRelay relay("relay.sentinelfs.io", 9000);
 * relay.setDataCallback([](const std::string& from, const auto& data) {
 *     // Handle received data
 * });
 * relay.connect("my-peer-id", "session-code");
 * relay.sendToPeer("target-peer-id", data);
 * @endcode
 */
class TCPRelay {
public:
    /**
     * @brief Constructor
     * @param serverHost Relay server hostname
     * @param serverPort Relay server port
     */
    TCPRelay(const std::string& serverHost = "localhost", int serverPort = 9000);
    ~TCPRelay();
    
    // Non-copyable
    TCPRelay(const TCPRelay&) = delete;
    TCPRelay& operator=(const TCPRelay&) = delete;
    
    /**
     * @brief Connect to relay server
     * @param localPeerId Our peer ID
     * @param sessionCode Session code for authentication
     * @return true if connection initiated
     */
    bool connect(const std::string& localPeerId, const std::string& sessionCode);
    
    /**
     * @brief Disconnect from relay server
     */
    void disconnect();
    
    /**
     * @brief Check if connected to relay
     */
    bool isConnected() const { return connected_; }
    
    /**
     * @brief Check if relay is enabled
     */
    bool isEnabled() const { return enabled_; }
    
    /**
     * @brief Enable/disable relay
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Send data to peer through relay
     * @param peerId Target peer ID
     * @param data Data to send
     * @return true if queued for sending
     */
    bool sendToPeer(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Request peer list from relay
     */
    void requestPeerList();
    
    /**
     * @brief Set callback for received data
     */
    void setDataCallback(RelayDataCallback callback);
    
    /**
     * @brief Set callback for peer discovery
     */
    void setPeerCallback(RelayPeerCallback callback);
    
    /**
     * @brief Get list of known relay peers
     */
    std::vector<RelayPeer> getRelayPeers() const;
    
    /**
     * @brief Get list of connected relay peers (alias for getRelayPeers)
     */
    std::vector<RelayPeer> getConnectedPeers() const { return getRelayPeers(); }
    
    /**
     * @brief Get relay server address
     */
    std::string getServerAddress() const { return serverHost_ + ":" + std::to_string(serverPort_); }

private:
    void readLoop();
    void writeLoop();
    void heartbeatLoop();
    bool sendMessage(RelayMessageType type, const std::vector<uint8_t>& payload);
    void handleMessage(RelayMessageType type, const std::vector<uint8_t>& payload);
    void reconnect();
    
    std::string serverHost_;
    int serverPort_;
    int socket_{-1};
    
    std::string localPeerId_;
    std::string sessionCode_;
    
    std::atomic<bool> enabled_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    
    std::thread readThread_;
    std::thread writeThread_;
    std::thread heartbeatThread_;
    
    mutable std::mutex mutex_;
    std::mutex writeMutex_;
    std::queue<std::vector<uint8_t>> writeQueue_;
    
    std::map<std::string, RelayPeer> relayPeers_;
    
    RelayDataCallback dataCallback_;
    RelayPeerCallback peerCallback_;
    
    static constexpr int HEARTBEAT_INTERVAL_SEC = 30;
    static constexpr int RECONNECT_DELAY_SEC = 5;
    static constexpr int CONNECT_TIMEOUT_SEC = 10;
};

} // namespace SentinelFS
