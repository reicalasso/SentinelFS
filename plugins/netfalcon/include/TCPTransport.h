#pragma once

/**
 * @file TCPTransport.h
 * @brief TCP transport implementation for NetFalcon
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
class BandwidthManager;

namespace NetFalcon {

/**
 * @brief TCP connection info
 */
struct TCPConnection {
    std::string peerId;
    std::string address;
    int port{0};
    int socket{-1};
    ConnectionState state{ConnectionState::DISCONNECTED};
    ConnectionQuality quality;
    std::chrono::steady_clock::time_point connectedAt;
    std::chrono::steady_clock::time_point lastActivity;
    std::thread readThread;
    bool isIncoming{false};
};

/**
 * @brief TCP transport implementation
 * 
 * Features:
 * - Connection pooling
 * - Automatic reconnection
 * - Bandwidth limiting integration
 * - Quality monitoring
 */
class TCPTransport : public ITransport {
public:
    TCPTransport(EventBus* eventBus, SessionManager* sessionManager, BandwidthManager* bandwidthManager = nullptr);
    ~TCPTransport() override;

    // ITransport interface
    TransportType getType() const override { return TransportType::TCP; }
    std::string getName() const override { return "TCP"; }
    
    bool startListening(int port) override;
    void stopListening() override;
    int getListeningPort() const override { return listeningPort_; }
    
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

    /**
     * @brief Set maximum connection pool size
     */
    void setMaxConnections(std::size_t max) { maxConnections_ = max; }

    /**
     * @brief Enable/disable auto-reconnect
     */
    void setAutoReconnect(bool enable) { autoReconnect_ = enable; }

    /**
     * @brief Get connection count
     */
    std::size_t getConnectionCount() const;

private:
    // Server operations
    void listenLoop();
    void handleIncomingConnection(int clientSocket, const std::string& clientIp, int clientPort);
    
    // Client operations
    bool performHandshake(int socket, bool isServer, std::string& remotePeerId);
    
    // Connection management
    void readLoop(const std::string& peerId);
    void cleanupConnection(const std::string& peerId);
    bool ensureCapacity();
    void pruneOldestConnection();
    
    // Event emission
    void emitEvent(TransportEvent event, const std::string& peerId, 
                   const std::string& message = "", const std::vector<uint8_t>& data = {});

    EventBus* eventBus_;
    SessionManager* sessionManager_;
    BandwidthManager* bandwidthManager_;
    TransportEventCallback eventCallback_;
    
    // Server state
    int serverSocket_{-1};
    int listeningPort_{0};
    std::atomic<bool> listening_{false};
    std::thread listenThread_;
    
    // Connections
    mutable std::mutex connectionMutex_;
    std::map<std::string, std::unique_ptr<TCPConnection>> connections_;
    std::size_t maxConnections_{64};
    
    // Reconnection
    std::atomic<bool> autoReconnect_{true};
    std::map<std::string, std::pair<std::string, int>> reconnectTargets_;  // peerId -> (address, port)
    
    // Shutdown flag
    std::atomic<bool> shuttingDown_{false};
    
    // Thread management
    mutable std::mutex threadMutex_;
    std::map<std::string, std::thread> readThreads_;
};

} // namespace NetFalcon
} // namespace SentinelFS
