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
#include <limits>

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
 * @brief NAT type detection results
 */
enum class NatType {
    UNKNOWN,
    OPEN,               // No NAT, direct connection possible
    FULL_CONE,          // NAT but easy traversal
    RESTRICTED_CONE,    // NAT with port restriction
    PORT_RESTRICTED,    // NAT with port + address restriction
    SYMMETRIC           // Hardest to traverse, needs RELAY
};

/**
 * @brief Network environment characteristics
 */
struct NetworkEnvironment {
    NatType natType{NatType::UNKNOWN};
    bool firewallDetected{false};
    bool udpBlocked{false};
    bool quicSupported{true};
    std::string publicIp;
    int publicPort{0};
    std::chrono::steady_clock::time_point lastProbed;
    
    // Returns true if direct P2P is likely to work
    bool canDirectConnect() const {
        return natType == NatType::OPEN || 
               natType == NatType::FULL_CONE ||
               natType == NatType::RESTRICTED_CONE;
    }
    
    // Returns true if RELAY is required
    bool needsRelay() const {
        return natType == NatType::SYMMETRIC || 
               firewallDetected || 
               udpBlocked;
    }
};

/**
 * @brief Connection quality metrics with filtering
 */
struct ConnectionQuality {
    // Raw measurements
    int rttMs{-1};
    double jitterMs{0.0};
    double packetLossPercent{0.0};
    std::size_t bytesInFlight{0};
    std::chrono::steady_clock::time_point lastUpdated;
    
    // Exponential weighted moving averages (EWMA)
    double ewmaRttMs{-1.0};
    double ewmaJitterMs{0.0};
    double ewmaLossPercent{0.0};
    
    // Quality thresholds
    static constexpr int RTT_EXCELLENT = 50;      // < 50ms
    static constexpr int RTT_GOOD = 150;          // < 150ms
    static constexpr int RTT_FAIR = 300;          // < 300ms
    static constexpr double LOSS_EXCELLENT = 0.1; // < 0.1%
    static constexpr double LOSS_GOOD = 1.0;      // < 1%
    static constexpr double LOSS_FAIR = 5.0;      // < 5%
    static constexpr double JITTER_EXCELLENT = 5.0;
    static constexpr double JITTER_GOOD = 20.0;
    static constexpr double JITTER_FAIR = 50.0;
    
    // EWMA alpha (0.1 = slow adaptation, 0.3 = faster)
    static constexpr double EWMA_ALPHA = 0.2;
    
    // Update EWMA values with new measurement
    void updateEwma(int newRtt, double newJitter, double newLoss) {
        if (ewmaRttMs < 0) {
            ewmaRttMs = newRtt;
            ewmaJitterMs = newJitter;
            ewmaLossPercent = newLoss;
        } else {
            ewmaRttMs = EWMA_ALPHA * newRtt + (1.0 - EWMA_ALPHA) * ewmaRttMs;
            ewmaJitterMs = EWMA_ALPHA * newJitter + (1.0 - EWMA_ALPHA) * ewmaJitterMs;
            ewmaLossPercent = EWMA_ALPHA * newLoss + (1.0 - EWMA_ALPHA) * ewmaLossPercent;
        }
        rttMs = newRtt;
        jitterMs = newJitter;
        packetLossPercent = newLoss;
        lastUpdated = std::chrono::steady_clock::now();
    }
    
    // Check if quality degraded beyond threshold
    bool isDegraded() const {
        return ewmaRttMs > RTT_FAIR || 
               ewmaLossPercent > LOSS_FAIR || 
               ewmaJitterMs > JITTER_FAIR;
    }
    
    // Check if quality is excellent (don't switch)
    bool isExcellent() const {
        return ewmaRttMs > 0 && ewmaRttMs < RTT_EXCELLENT &&
               ewmaLossPercent < LOSS_EXCELLENT &&
               ewmaJitterMs < JITTER_EXCELLENT;
    }
    
    // Compute composite score (lower is better)
    double computeScore(double lossWeight = 10.0, double jitterWeight = 2.0) const {
        if (ewmaRttMs < 0) return std::numeric_limits<double>::infinity();
        return ewmaRttMs + (jitterWeight * ewmaJitterMs) + (lossWeight * ewmaLossPercent);
    }
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
