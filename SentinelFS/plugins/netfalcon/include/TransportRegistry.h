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
 * @brief Per-peer transport binding
 */
struct PeerTransportBinding {
    std::string peerId;
    TransportType activeTransport;
    TransportType preferredTransport;
    std::chrono::steady_clock::time_point boundAt;
    int failoverCount{0};
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
     * @brief Get the best transport for a peer
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
     * @brief Handle transport failure for peer
     * 
     * Attempts failover to next available transport.
     * @return New transport or nullptr if all failed
     */
    ITransport* handleFailover(const std::string& peerId);

    /**
     * @brief Update quality metrics for transport selection
     */
    void updateQuality(const std::string& peerId, TransportType type, const ConnectionQuality& quality);

    /**
     * @brief Shutdown all transports
     */
    void shutdownAll();

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
    std::vector<TransportType> getFailoverOrder() const;

    mutable std::mutex mutex_;
    std::map<TransportType, std::unique_ptr<ITransport>> transports_;
    std::map<std::string, PeerTransportBinding> bindings_;
    std::map<std::string, std::map<TransportType, ConnectionQuality>> qualityCache_;
    
    TransportStrategy strategy_{TransportStrategy::FALLBACK_CHAIN};
    std::vector<TransportType> priorityOrder_{TransportType::TCP, TransportType::QUIC, TransportType::RELAY};
};

} // namespace NetFalcon
} // namespace SentinelFS
