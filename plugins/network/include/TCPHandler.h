#pragma once

#include "HandshakeProtocol.h"
#include "EventBus.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include "BandwidthLimiter.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

namespace SentinelFS {

/**
 * @brief TCP connection manager
 * 
 * Manages:
 * - TCP server socket and listening
 * - Active peer connections
 * - Data transmission/reception
 * - Connection lifecycle
 */
class TCPHandler {
public:
    using DataCallback = std::function<void(const std::string& peerId, 
                                           const std::vector<uint8_t>& data)>;
    
    /**
     * @brief Constructor
     * @param eventBus Event bus for publishing connection events
     * @param handshake Handshake protocol handler
     * @param bandwidthManager Optional bandwidth manager (nullptr = unlimited)
     */
    TCPHandler(EventBus* eventBus, HandshakeProtocol* handshake, BandwidthManager* bandwidthManager = nullptr);
    
    ~TCPHandler();
    
    /**
     * @brief Start TCP server on specified port
     * @param port Port to listen on
     * @return true if server started successfully
     */
    bool startListening(int port);
    
    /**
     * @brief Stop TCP server and close all connections
     */
    void stopListening();
    
    /**
     * @brief Connect to a remote peer
     * @param address IP address
     * @param port TCP port
     * @return true if connection successful
     */
    bool connectToPeer(const std::string& address, int port);
    
    /**
     * @brief Send data to a peer
     * @param peerId Peer identifier
     * @param data Data to send
     * @return true if sent successfully
     */
    bool sendData(const std::string& peerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Disconnect from a peer
     * @param peerId Peer to disconnect
     */
    void disconnectPeer(const std::string& peerId);
    
    /**
     * @brief Check if peer is connected
     * @param peerId Peer to check
     * @return true if connected
     */
    bool isPeerConnected(const std::string& peerId) const;
    
    /**
     * @brief Get list of connected peer IDs
     */
    std::vector<std::string> getConnectedPeers() const;
    
    /**
     * @brief Set callback for received data
     * 
     * Called when data is received from a peer
     */
    void setDataCallback(DataCallback callback) {
        dataCallback_ = callback;
    }
    
    /**
     * @brief Measure RTT to a peer
     * @param peerId Peer to measure
     * @return RTT in milliseconds, -1 on error
     */
    int measureRTT(const std::string& peerId);
    
    /**
     * @brief Get the port this handler is listening on
     * @return The listening port, or 0 if not listening
     */
    int getListeningPort() const { return listeningPort_; }
    
private:
    void listenLoop();
    void handleClient(int clientSocket);
    void readLoop(int socket, const std::string& peerId);
    
    EventBus* eventBus_;
    HandshakeProtocol* handshake_;
    BandwidthManager* bandwidthManager_;
    DataCallback dataCallback_;
    
    Logger& logger_{Logger::instance()};
    MetricsCollector& metrics_{MetricsCollector::instance()};
    
    int serverSocket_{-1};
    int listeningPort_{0};
    std::atomic<bool> listening_{false};
    std::atomic<bool> shuttingDown_{false};
    std::thread listenThread_;
    
    mutable std::mutex connectionMutex_;
    std::map<std::string, int> connections_;  // peerId -> socket
    
    // Track active read threads for graceful shutdown
    mutable std::mutex threadMutex_;
    std::map<std::string, std::thread> readThreads_;  // peerId -> thread
    
    void cleanupThread(const std::string& peerId);
};

} // namespace SentinelFS
