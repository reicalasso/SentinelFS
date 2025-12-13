#pragma once

/**
 * @file TransportRegistry.h
 * @brief Transport registry and selector for NetFalcon
 * 
 * Manages multiple transport implementations and provides
 * intelligent transport selection based on connection quality.
 */

#include "ITransport.h"
#include <map>
#include <mutex>
#include <optional>

namespace SentinelFS {
namespace NetFalcon {

/**
 * @brief Transport selection strategy
 */
enum class TransportStrategy {
    PREFER_DIRECT,      // Prefer direct connections (TCP/QUIC)
    PREFER_RELIABLE,    // Prefer most reliable transport
    PREFER_FAST,        // Prefer lowest latency transport
    FALLBACK_CHAIN,     // Try transports in order until one works
    ADAPTIVE            // Dynamically select based on conditions
};

/**
 * @brief Failover configuration
 */
struct FailoverConfig {
    int maxFailoverAttempts{3};
    std::chrono::milliseconds initialBackoff{100};
    std::chrono::milliseconds maxBackoff{30000};
    double backoffMultiplier{2.0};
    bool enableCircuitBreaker{true};
    int circuitBreakerThreshold{5};     // Failures before circuit opens
    std::chrono::seconds circuitBreakerTimeout{60};
};

/**
 * @brief Per-peer transport binding with failover state
 */
struct PeerTransportBinding {
    std::string peerId;
    TransportType activeTransport;
    TransportType preferredTransport;
    std::chrono::steady_clock::time_point boundAt;
    int failoverCount{0};
    
    // Failover state
    std::chrono::steady_clock::time_point lastFailover;
    std::chrono::milliseconds currentBackoff{100};
    int consecutiveFailures{0};
    bool circuitOpen{false};
    std::chrono::steady_clock::time_point circuitOpenedAt;
    
    // Transport authentication state
    bool transportAuthenticated{false};
    std::chrono::steady_clock::time_point lastAuthTime;
};

/**
 * @brief Transport registry and intelligent selector
 * 
 * Manages transport lifecycle and provides smart routing:
 * - Registers multiple transport implementations
 * - Selects best transport per-peer based on strategy
 * - Handles automatic failover between transports
 */
class TransportRegistry {
public:
    TransportRegistry();
    ~TransportRegistry();

    /**
     * @brief Register a transport implementation
     * @param type Transport type identifier
     * @param transport Transport instance (takes ownership)
     */
    void registerTransport(TransportType type, std::unique_ptr<ITransport> transport);

    /**
     * @brief Unregister a transport
     */
    void unregisterTransport(TransportType type);

    /**
     * @brief Get transport by type
     * @return Transport pointer or nullptr if not registered
     */
    ITransport* getTransport(TransportType type);
    const ITransport* getTransport(TransportType type) const;

    /**
     * @brief Get all registered transports
     */
    std::vector<TransportType> getRegisteredTransports() const;

    /**
     * @brief Check if transport is registered
     */
    bool hasTransport(TransportType type) const;

    /**
     * @brief Set transport selection strategy
     */
    void setStrategy(TransportStrategy strategy);
    TransportStrategy getStrategy() const { return strategy_; }

    /**
     * @brief Set preferred transport for a peer
     */
    void setPreferredTransport(const std::string& peerId, TransportType type);

    /**
     * @brief Get the best transport for a peer (simple version)
     * 
     * Selection logic based on strategy:
     * - PREFER_DIRECT: TCP > QUIC > RELAY
     * - PREFER_RELIABLE: Lowest packet loss
     * - PREFER_FAST: Lowest RTT
     * - FALLBACK_CHAIN: First available in priority order
     * - ADAPTIVE: Dynamic based on current metrics
     */
    ITransport* selectTransport(const std::string& peerId);
    
    /**
     * @brief Context-aware transport selection
     * 
     * Considers NAT type, firewall, data size, reliability requirements.
     * Selection logic:
     * - if NAT traversal needed → RELAY
     * - else if low latency desired && QUIC available → QUIC
     * - else → TCP
     */
    ITransport* selectTransport(const TransportSelectionContext& context);

    /**
     * @brief Get current transport binding for peer
     */
    std::optional<PeerTransportBinding> getBinding(const std::string& peerId) const;

    /**
     * @brief Bind peer to specific transport
     */
    void bindPeer(const std::string& peerId, TransportType type);

    /**
     * @brief Unbind peer (clear transport preference)
     */
    void unbindPeer(const std::string& peerId);

    /**
     * @brief Handle transport failure for peer with exponential backoff
     * 
     * Attempts failover to next available transport.
     * Implements circuit breaker pattern.
     * @return New transport or nullptr if all failed or circuit open
     */
    ITransport* handleFailover(const std::string& peerId);
    
    /**
     * @brief Report transport failure (for circuit breaker)
     */
    void reportFailure(const std::string& peerId, TransportType type);
    
    /**
     * @brief Report transport success (resets failure count)
     */
    void reportSuccess(const std::string& peerId, TransportType type);
    
    /**
     * @brief Check if circuit breaker is open for peer
     */
    bool isCircuitOpen(const std::string& peerId) const;

    /**
     * @brief Update quality metrics for transport selection
     */
    void updateQuality(const std::string& peerId, TransportType type, const ConnectionQuality& quality);
    
    /**
     * @brief Set local network environment
     */
    void setLocalEnvironment(const NetworkEnvironment& env);
    NetworkEnvironment getLocalEnvironment() const { return localEnv_; }
    
    /**
     * @brief Set failover configuration
     */
    void setFailoverConfig(const FailoverConfig& config);
    FailoverConfig getFailoverConfig() const { return failoverConfig_; }

    /**
     * @brief Shutdown all transports
     */
    void shutdownAll();

    /**
     * @brief Get list of currently connected peer IDs
     */
    std::vector<std::string> getConnectedPeerIds() const;

    /**
     * @brief Convert transport type to string
     */
    static std::string transportTypeToString(TransportType type);

    /**
     * @brief Parse transport type from string
     */
    static std::optional<TransportType> parseTransportType(const std::string& str);

private:
    ITransport* selectByStrategy(const std::string& peerId);
    ITransport* selectAdaptive(const std::string& peerId);
    ITransport* selectByEnvironment(const TransportSelectionContext& context);
    std::vector<TransportType> getFailoverOrder() const;
    bool shouldTriggerFailover(const ConnectionQuality& quality) const;
    void updateBackoff(PeerTransportBinding& binding);

    mutable std::mutex mutex_;
    std::map<TransportType, std::unique_ptr<ITransport>> transports_;
    std::map<std::string, PeerTransportBinding> bindings_;
    std::map<std::string, std::map<TransportType, ConnectionQuality>> qualityCache_;
    
    TransportStrategy strategy_{TransportStrategy::ADAPTIVE};
    std::vector<TransportType> priorityOrder_{TransportType::QUIC, TransportType::TCP, TransportType::WEBRTC, TransportType::RELAY};
    NetworkEnvironment localEnv_;
    FailoverConfig failoverConfig_;
};

} // namespace NetFalcon
} // namespace SentinelFS
