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

// Forward declarations
enum class TransportType;
enum class NatType;

/**
 * @brief NAT traversal types
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
 * @brief Network environment information
 */
struct NetworkEnvironment {
    NatType natType{NatType::UNKNOWN};
    bool firewallDetected{false};
    bool udpBlocked{false};
    bool quicSupported{true};
    bool isLocal{false};
    bool isVPN{false};
    bool isRestricted{false};
    std::string networkType;
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
 * @brief Transport selection context
 */
struct TransportSelectionContext {
    std::string peerId;
    NetworkEnvironment localEnv;
    NetworkEnvironment remoteEnv;
    std::size_t dataSize{0};           // Hint for data size to send
    bool requiresReliability{true};     // Must guarantee delivery
    bool lowLatencyPreferred{false};    // Prioritize speed over reliability
    bool isInitialConnection{false};    // First connection attempt
};

/**
 * @brief Transport type enumeration
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
 * @brief Connection quality metrics with filtering
 */
struct ConnectionQuality {
    // Raw measurements
    int rttMs{-1};
    double jitterMs{0.0};
    double packetLossPercent{0.0};
    std::size_t bytesInFlight{0};
    std::chrono::steady_clock::time_point lastUpdated;
    
    // Bandwidth metrics
    double currentBandwidthBps{0.0};      // Current measured bandwidth
    double averageBandwidthBps{0.0};      // Average bandwidth over time window
    double maxBandwidthBps{0.0};          // Maximum observed bandwidth
    std::chrono::steady_clock::time_point lastBandwidthMeasure;
    
    // Congestion metrics
    double congestionWindow{0.0};         // Current congestion window size
    double queueDelay{0.0};               // Measured queue delay
    double retransmissionRate{0.0};       // Retransmission rate percentage
    bool isCongested{false};              // Congestion detected flag
    
    // Exponential weighted moving averages (EWMA)
    double ewmaRttMs{-1.0};
    double ewmaJitterMs{0.0};
    double ewmaLossPercent{0.0};
    double ewmaBandwidthBps{0.0};
    double ewmaCongestionLevel{0.0};
    
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
    
    // Bandwidth thresholds (bytes per second)
    static constexpr double BANDWIDTH_EXCELLENT = 10.0 * 1024 * 1024;  // > 10 MB/s
    static constexpr double BANDWIDTH_GOOD = 5.0 * 1024 * 1024;        // > 5 MB/s
    static constexpr double BANDWIDTH_FAIR = 1.0 * 1024 * 1024;        // > 1 MB/s
    
    // Congestion thresholds
    static constexpr double CONGESTION_EXCELLENT = 0.1;  // < 10% of max cwnd
    static constexpr double CONGESTION_GOOD = 0.3;      // < 30% of max cwnd
    static constexpr double CONGESTION_FAIR = 0.6;      // < 60% of max cwnd
    static constexpr double QUEUE_DELAY_EXCELLENT = 10.0; // < 10ms
    static constexpr double QUEUE_DELAY_GOOD = 50.0;     // < 50ms
    static constexpr double QUEUE_DELAY_FAIR = 100.0;    // < 100ms
    
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
    
    // Update bandwidth measurements
    void updateBandwidth(double currentBps) {
        currentBandwidthBps = currentBps;
        lastBandwidthMeasure = std::chrono::steady_clock::now();
        
        if (averageBandwidthBps == 0.0) {
            averageBandwidthBps = currentBps;
            ewmaBandwidthBps = currentBps;
        } else {
            averageBandwidthBps = 0.9 * averageBandwidthBps + 0.1 * currentBps;
            ewmaBandwidthBps = EWMA_ALPHA * currentBps + (1.0 - EWMA_ALPHA) * ewmaBandwidthBps;
        }
        
        if (currentBps > maxBandwidthBps) {
            maxBandwidthBps = currentBps;
        }
    }
    
    // Update congestion metrics
    void updateCongestion(double cwnd, double queueDel, double retransRate) {
        congestionWindow = cwnd;
        queueDelay = queueDel;
        retransmissionRate = retransRate;
        
        // Normalize congestion level (0-1, where 1 is highly congested)
        double congestionLevel = 0.0;
        if (cwnd > 0) {
            congestionLevel = std::min(1.0, queueDelay / 100.0 + retransRate / 10.0);
        }
        
        if (ewmaCongestionLevel == 0.0) {
            ewmaCongestionLevel = congestionLevel;
        } else {
            ewmaCongestionLevel = EWMA_ALPHA * congestionLevel + (1.0 - EWMA_ALPHA) * ewmaCongestionLevel;
        }
        
        isCongested = congestionLevel > 0.5;
    }
    
    // Check if quality degraded beyond threshold
    bool isDegraded() const {
        return ewmaRttMs > RTT_FAIR || 
               ewmaLossPercent > LOSS_FAIR || 
               ewmaJitterMs > JITTER_FAIR ||
               ewmaBandwidthBps < BANDWIDTH_FAIR ||
               ewmaCongestionLevel > CONGESTION_FAIR ||
               queueDelay > QUEUE_DELAY_FAIR;
    }
    
    // Check if quality is excellent (don't switch)
    bool isExcellent() const {
        return ewmaRttMs > 0 && ewmaRttMs < RTT_EXCELLENT &&
               ewmaLossPercent < LOSS_EXCELLENT &&
               ewmaJitterMs < JITTER_EXCELLENT &&
               ewmaBandwidthBps > BANDWIDTH_EXCELLENT &&
               ewmaCongestionLevel < CONGESTION_EXCELLENT &&
               queueDelay < QUEUE_DELAY_EXCELLENT;
    }
    
    // Compute composite score with adaptive weighting
    double computeScore(const TransportSelectionContext& context) const {
        if (ewmaRttMs < 0) return std::numeric_limits<double>::infinity();
        
        // Base score components
        double rttScore = normalizeScore(ewmaRttMs, 0, RTT_FAIR, true);        // Lower is better
        double lossScore = normalizeScore(ewmaLossPercent, 0, LOSS_FAIR, true); // Lower is better
        double jitterScore = normalizeScore(ewmaJitterMs, 0, JITTER_FAIR, true); // Lower is better
        double bandwidthScore = normalizeScore(ewmaBandwidthBps, BANDWIDTH_FAIR, BANDWIDTH_EXCELLENT, false); // Higher is better
        double congestionScore = normalizeScore(ewmaCongestionLevel, 0, 1.0, true); // Lower is better
        
        // Adaptive weights based on context
        double rttWeight = context.lowLatencyPreferred ? 0.4 : 0.2;
        double bandwidthWeight = context.dataSize > (1024 * 1024) ? 0.3 : 0.2; // Large files favor bandwidth
        double reliabilityWeight = context.requiresReliability ? 0.3 : 0.2;
        double congestionWeight = 0.2;
        
        // Calculate weighted score
        double totalScore = (rttWeight * rttScore) +
                           (bandwidthWeight * bandwidthScore) +
                           (reliabilityWeight * (lossScore + jitterScore)) +
                           (congestionWeight * congestionScore);
        
        // Apply penalty for congestion
        if (isCongested) {
            totalScore *= 1.5;
        }
        
        return totalScore;
    }
    
// Normalize value to 0-1 scale
    double normalizeScore(double value, double min, double max, bool lowerIsBetter) const {
        if (max <= min) return 0.0;
        
        double normalized = (value - min) / (max - min);
        if (normalized < 0) return 0.0;
        if (normalized > 1) return 1.0;
        
        return lowerIsBetter ? normalized : (1.0 - normalized);
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
