#pragma once

/**
 * @file ITransport.h
 * @brief Transport layer abstraction for NetFalcon
 * 
 * Defines a common interface for different transport implementations
 * (TCP, QUIC, WebRTC, etc.) allowing seamless switching between them.
 */

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <memory>
#include <chrono>

namespace SentinelFS {
namespace NetFalcon {

/**
 * @brief Transport types supported by NetFalcon
 */
enum class TransportType {
    TCP,
    QUIC,
    WEBRTC,
    RELAY
};

/**
 * @brief Transport connection state
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    FAILED
};

/**
 * @brief Connection quality metrics
 */
struct ConnectionQuality {
    int rttMs{-1};
    double jitterMs{0.0};
    double packetLossPercent{0.0};
    std::size_t bytesInFlight{0};
    std::chrono::steady_clock::time_point lastUpdated;
};

/**
 * @brief Transport event types
 */
enum class TransportEvent {
    CONNECTED,
    DISCONNECTED,
    DATA_RECEIVED,
    ERROR,
    QUALITY_CHANGED
};

/**
 * @brief Transport event data
 */
struct TransportEventData {
    TransportEvent event;
    std::string peerId;
    std::string message;
    std::vector<uint8_t> data;
    ConnectionQuality quality;
};

/**
 * @brief Callback for transport events
 */
using TransportEventCallback = std::function<void(const TransportEventData&)>;

/**
 * @brief Abstract transport interface
 * 
 * All transport implementations (TCP, QUIC, etc.) must implement this interface.
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    /**
     * @brief Get transport type
     */
    virtual TransportType getType() const = 0;

    /**
     * @brief Get transport name for logging
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Start listening for incoming connections
     * @param port Port to listen on
     * @return true if successful
     */
    virtual bool startListening(int port) = 0;

    /**
     * @brief Stop listening
     */
    virtual void stopListening() = 0;

    /**
     * @brief Get listening port
     */
    virtual int getListeningPort() const = 0;

    /**
     * @brief Connect to a remote peer
     * @param address IP address or hostname
     * @param port Port number
     * @param peerId Expected peer ID (for verification)
     * @return true if connection initiated
     */
    virtual bool connect(const std::string& address, int port, const std::string& peerId = "") = 0;

    /**
     * @brief Disconnect from a peer
     * @param peerId Peer to disconnect
     */
    virtual void disconnect(const std::string& peerId) = 0;

    /**
     * @brief Send data to a peer
     * @param peerId Target peer
     * @param data Data to send
     * @return true if queued/sent successfully
     */
    virtual bool send(const std::string& peerId, const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Check if peer is connected
     * @param peerId Peer to check
     */
    virtual bool isConnected(const std::string& peerId) const = 0;

    /**
     * @brief Get connection state for a peer
     */
    virtual ConnectionState getConnectionState(const std::string& peerId) const = 0;

    /**
     * @brief Get connection quality metrics
     */
    virtual ConnectionQuality getConnectionQuality(const std::string& peerId) const = 0;

    /**
     * @brief Get list of connected peer IDs
     */
    virtual std::vector<std::string> getConnectedPeers() const = 0;

    /**
     * @brief Set event callback
     */
    virtual void setEventCallback(TransportEventCallback callback) = 0;

    /**
     * @brief Measure RTT to peer
     * @return RTT in milliseconds, -1 on error
     */
    virtual int measureRTT(const std::string& peerId) = 0;

    /**
     * @brief Shutdown transport
     */
    virtual void shutdown() = 0;
};

/**
 * @brief Transport factory function type
 */
using TransportFactory = std::function<std::unique_ptr<ITransport>()>;

} // namespace NetFalcon
} // namespace SentinelFS
