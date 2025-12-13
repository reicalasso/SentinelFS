#include "TransportRegistry.h"
#include <algorithm>
#include <limits>

namespace SentinelFS {
namespace NetFalcon {

TransportRegistry::TransportRegistry() = default;
TransportRegistry::~TransportRegistry() {
    shutdownAll();
}

void TransportRegistry::registerTransport(TransportType type, std::unique_ptr<ITransport> transport) {
    std::lock_guard<std::mutex> lock(mutex_);
    transports_[type] = std::move(transport);
}

void TransportRegistry::unregisterTransport(TransportType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = transports_.find(type);
    if (it != transports_.end()) {
        it->second->shutdown();
        transports_.erase(it);
    }
}

ITransport* TransportRegistry::getTransport(TransportType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = transports_.find(type);
    return it != transports_.end() ? it->second.get() : nullptr;
}

const ITransport* TransportRegistry::getTransport(TransportType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = transports_.find(type);
    return it != transports_.end() ? it->second.get() : nullptr;
}

std::vector<TransportType> TransportRegistry::getRegisteredTransports() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TransportType> types;
    types.reserve(transports_.size());
    for (const auto& [type, _] : transports_) {
        types.push_back(type);
    }
    return types;
}

bool TransportRegistry::hasTransport(TransportType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transports_.find(type) != transports_.end();
}

void TransportRegistry::setStrategy(TransportStrategy strategy) {
    std::lock_guard<std::mutex> lock(mutex_);
    strategy_ = strategy;
}

void TransportRegistry::setPreferredTransport(const std::string& peerId, TransportType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = bindings_.find(peerId);
    if (it != bindings_.end()) {
        it->second.preferredTransport = type;
    } else {
        PeerTransportBinding binding;
        binding.peerId = peerId;
        binding.preferredTransport = type;
        binding.activeTransport = type;
        binding.boundAt = std::chrono::steady_clock::now();
        bindings_[peerId] = binding;
    }
}

ITransport* TransportRegistry::selectTransport(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check existing binding
    auto bindingIt = bindings_.find(peerId);
    if (bindingIt != bindings_.end()) {
        auto transportIt = transports_.find(bindingIt->second.activeTransport);
        if (transportIt != transports_.end() && transportIt->second->isConnected(peerId)) {
            return transportIt->second.get();
        }
    }
    
    return selectByStrategy(peerId);
}

std::optional<PeerTransportBinding> TransportRegistry::getBinding(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = bindings_.find(peerId);
    if (it != bindings_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void TransportRegistry::bindPeer(const std::string& peerId, TransportType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    PeerTransportBinding binding;
    binding.peerId = peerId;
    binding.activeTransport = type;
    binding.preferredTransport = type;
    binding.boundAt = std::chrono::steady_clock::now();
    bindings_[peerId] = binding;
}

void TransportRegistry::unbindPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    bindings_.erase(peerId);
    qualityCache_.erase(peerId);
}

ITransport* TransportRegistry::handleFailover(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto bindingIt = bindings_.find(peerId);
    TransportType currentType = TransportType::TCP;
    
    if (bindingIt != bindings_.end()) {
        // Check circuit breaker
        if (bindingIt->second.circuitOpen) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - bindingIt->second.circuitOpenedAt);
            
            if (elapsed < failoverConfig_.circuitBreakerTimeout) {
                return nullptr;  // Circuit still open
            }
            // Half-open: allow one attempt
            bindingIt->second.circuitOpen = false;
        }
        
        // Check backoff
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - bindingIt->second.lastFailover);
        
        if (elapsed < bindingIt->second.currentBackoff) {
            return nullptr;  // Still in backoff period
        }
        
        currentType = bindingIt->second.activeTransport;
        bindingIt->second.failoverCount++;
        bindingIt->second.lastFailover = now;
        updateBackoff(bindingIt->second);
    }
    
    // Get failover order and find next transport
    auto order = getFailoverOrder();
    bool foundCurrent = false;
    
    for (auto type : order) {
        if (type == currentType) {
            foundCurrent = true;
            continue;
        }
        
        if (foundCurrent) {
            auto it = transports_.find(type);
            if (it != transports_.end()) {
                // Update binding
                if (bindingIt != bindings_.end()) {
                    bindingIt->second.activeTransport = type;
                    bindingIt->second.transportAuthenticated = false;  // Require re-auth
                } else {
                    PeerTransportBinding binding;
                    binding.peerId = peerId;
                    binding.activeTransport = type;
                    binding.preferredTransport = type;
                    binding.boundAt = std::chrono::steady_clock::now();
                    binding.transportAuthenticated = false;
                    bindings_[peerId] = binding;
                }
                return it->second.get();
            }
        }
    }
    
    // Wrap around to beginning of order
    for (auto type : order) {
        if (type == currentType) break;
        
        auto it = transports_.find(type);
        if (it != transports_.end()) {
            if (bindingIt != bindings_.end()) {
                bindingIt->second.activeTransport = type;
                bindingIt->second.transportAuthenticated = false;
            }
            return it->second.get();
        }
    }
    
    return nullptr;
}

void TransportRegistry::reportFailure(const std::string& peerId, TransportType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto bindingIt = bindings_.find(peerId);
    if (bindingIt == bindings_.end()) return;
    
    bindingIt->second.consecutiveFailures++;
    
    // Check if circuit breaker should open
    if (failoverConfig_.enableCircuitBreaker &&
        bindingIt->second.consecutiveFailures >= failoverConfig_.circuitBreakerThreshold) {
        bindingIt->second.circuitOpen = true;
        bindingIt->second.circuitOpenedAt = std::chrono::steady_clock::now();
    }
}

void TransportRegistry::reportSuccess(const std::string& peerId, TransportType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto bindingIt = bindings_.find(peerId);
    if (bindingIt == bindings_.end()) return;
    
    // Reset failure state
    bindingIt->second.consecutiveFailures = 0;
    bindingIt->second.currentBackoff = failoverConfig_.initialBackoff;
    bindingIt->second.circuitOpen = false;
    bindingIt->second.transportAuthenticated = true;
    bindingIt->second.lastAuthTime = std::chrono::steady_clock::now();
}

bool TransportRegistry::isCircuitOpen(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto bindingIt = bindings_.find(peerId);
    if (bindingIt == bindings_.end()) return false;
    
    if (!bindingIt->second.circuitOpen) return false;
    
    // Check if timeout has elapsed
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - bindingIt->second.circuitOpenedAt);
    
    return elapsed < failoverConfig_.circuitBreakerTimeout;
}

void TransportRegistry::setLocalEnvironment(const NetworkEnvironment& env) {
    std::lock_guard<std::mutex> lock(mutex_);
    localEnv_ = env;
}

void TransportRegistry::setFailoverConfig(const FailoverConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    failoverConfig_ = config;
}

void TransportRegistry::updateBackoff(PeerTransportBinding& binding) {
    // Exponential backoff with cap
    auto newBackoff = std::chrono::milliseconds(
        static_cast<long long>(binding.currentBackoff.count() * failoverConfig_.backoffMultiplier));
    
    if (newBackoff > failoverConfig_.maxBackoff) {
        newBackoff = failoverConfig_.maxBackoff;
    }
    binding.currentBackoff = newBackoff;
}

bool TransportRegistry::shouldTriggerFailover(const ConnectionQuality& quality) const {
    // Use EWMA values for stable decisions
    return quality.isDegraded();
}

ITransport* TransportRegistry::selectTransport(const TransportSelectionContext& context) {
    std::lock_guard<std::mutex> lock(mutex_);
    return selectByEnvironment(context);
}

ITransport* TransportRegistry::selectByEnvironment(const TransportSelectionContext& context) {
    // Priority 1: Check if RELAY is required due to NAT/firewall
    bool needsRelay = localEnv_.needsRelay() || context.remoteEnv.needsRelay();
    
    if (needsRelay) {
        // Try WebRTC first (has built-in NAT traversal via ICE)
        auto webrtcIt = transports_.find(TransportType::WEBRTC);
        if (webrtcIt != transports_.end()) {
            return webrtcIt->second.get();
        }
        // Fall back to RELAY
        auto relayIt = transports_.find(TransportType::RELAY);
        if (relayIt != transports_.end()) {
            return relayIt->second.get();
        }
    }
    
    // Priority 2: Default - QUIC for fast networks (0-RTT, low latency)
    if (localEnv_.quicSupported && !localEnv_.udpBlocked) {
        auto it = transports_.find(TransportType::QUIC);
        if (it != transports_.end()) {
            return it->second.get();
        }
    }
    
    // Priority 3: Large data transfer - prefer TCP for reliability
    constexpr std::size_t LARGE_DATA_THRESHOLD = 1024 * 1024;  // 1 MB
    if (context.dataSize > LARGE_DATA_THRESHOLD && context.requiresReliability) {
        auto it = transports_.find(TransportType::TCP);
        if (it != transports_.end()) {
            return it->second.get();
        }
    }
    
    // Priority 4: Check quality metrics if available (EWMA-based selection)
    auto qualityIt = qualityCache_.find(context.peerId);
    if (qualityIt != qualityCache_.end() && !qualityIt->second.empty()) {
        // Find best transport by EWMA score
        ITransport* best = nullptr;
        double bestScore = std::numeric_limits<double>::max();
        
        for (const auto& [type, quality] : qualityIt->second) {
            double score = quality.computeScore(context);
            if (score < bestScore) {
                auto transportIt = transports_.find(type);
                if (transportIt != transports_.end()) {
                    best = transportIt->second.get();
                    bestScore = score;
                }
            }
        }
        
        if (best) return best;
    }
    
    // Priority 5: Default fallback order - QUIC > TCP > WebRTC > RELAY
    for (auto type : priorityOrder_) {
        auto it = transports_.find(type);
        if (it != transports_.end()) {
            return it->second.get();
        }
    }
    
    return nullptr;
}

void TransportRegistry::updateQuality(const std::string& peerId, TransportType type, const ConnectionQuality& quality) {
    std::lock_guard<std::mutex> lock(mutex_);
    qualityCache_[peerId][type] = quality;
}

void TransportRegistry::shutdownAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [_, transport] : transports_) {
        transport->shutdown();
    }
    transports_.clear();
    bindings_.clear();
    qualityCache_.clear();
}

std::vector<std::string> TransportRegistry::getConnectedPeerIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> connectedPeers;
    
    // Check each transport for connected peers
    for (const auto& [type, transport] : transports_) {
        auto peers = transport->getConnectedPeers();
        for (const auto& peerId : peers) {
            // Avoid duplicates
            if (std::find(connectedPeers.begin(), connectedPeers.end(), peerId) == connectedPeers.end()) {
                connectedPeers.push_back(peerId);
            }
        }
    }
    
    return connectedPeers;
}

std::string TransportRegistry::transportTypeToString(TransportType type) {
    switch (type) {
        case TransportType::TCP: return "TCP";
        case TransportType::QUIC: return "QUIC";
        case TransportType::WEBRTC: return "WebRTC";
        case TransportType::RELAY: return "Relay";
        default: return "Unknown";
    }
}

std::optional<TransportType> TransportRegistry::parseTransportType(const std::string& str) {
    if (str == "TCP" || str == "tcp") return TransportType::TCP;
    if (str == "QUIC" || str == "quic") return TransportType::QUIC;
    if (str == "WebRTC" || str == "webrtc") return TransportType::WEBRTC;
    if (str == "Relay" || str == "relay") return TransportType::RELAY;
    return std::nullopt;
}

ITransport* TransportRegistry::selectByStrategy(const std::string& peerId) {
    switch (strategy_) {
        case TransportStrategy::ADAPTIVE:
            return selectAdaptive(peerId);
            
        case TransportStrategy::PREFER_FAST: {
            // Select transport with lowest RTT
            ITransport* best = nullptr;
            int bestRtt = std::numeric_limits<int>::max();
            
            auto qualityIt = qualityCache_.find(peerId);
            if (qualityIt != qualityCache_.end()) {
                for (const auto& [type, quality] : qualityIt->second) {
                    if (quality.rttMs >= 0 && quality.rttMs < bestRtt) {
                        auto transportIt = transports_.find(type);
                        if (transportIt != transports_.end()) {
                            best = transportIt->second.get();
                            bestRtt = quality.rttMs;
                        }
                    }
                }
            }
            
            if (best) return best;
            // Fall through to fallback chain
            [[fallthrough]];
        }
        
        case TransportStrategy::PREFER_RELIABLE: {
            // Select transport with lowest packet loss
            ITransport* best = nullptr;
            double bestLoss = std::numeric_limits<double>::max();
            
            auto qualityIt = qualityCache_.find(peerId);
            if (qualityIt != qualityCache_.end()) {
                for (const auto& [type, quality] : qualityIt->second) {
                    if (quality.packetLossPercent < bestLoss) {
                        auto transportIt = transports_.find(type);
                        if (transportIt != transports_.end()) {
                            best = transportIt->second.get();
                            bestLoss = quality.packetLossPercent;
                        }
                    }
                }
            }
            
            if (best) return best;
            [[fallthrough]];
        }
        
        case TransportStrategy::PREFER_DIRECT:
        case TransportStrategy::FALLBACK_CHAIN:
        default: {
            // Try transports in priority order
            for (auto type : priorityOrder_) {
                auto it = transports_.find(type);
                if (it != transports_.end()) {
                    return it->second.get();
                }
            }
            
            // Return any available transport
            if (!transports_.empty()) {
                return transports_.begin()->second.get();
            }
            
            return nullptr;
        }
    }
}

ITransport* TransportRegistry::selectAdaptive(const std::string& peerId) {
    // Adaptive selection: use comprehensive scoring with bandwidth and congestion
    ITransport* best = nullptr;
    double bestScore = std::numeric_limits<double>::max();
    
    // Create selection context (could be enhanced with actual context)
    TransportSelectionContext context;
    context.peerId = peerId;
    context.dataSize = 1024 * 1024; // Default to 1MB as hint
    context.requiresReliability = true;
    context.lowLatencyPreferred = false;
    context.isInitialConnection = false;
    
    auto qualityIt = qualityCache_.find(peerId);
    if (qualityIt != qualityCache_.end()) {
        for (const auto& [type, quality] : qualityIt->second) {
            // Use new comprehensive scoring algorithm
            double score = quality.computeScore(context);
            
            // Apply transport-specific adjustments
            if (type == TransportType::QUIC) {
                // QUIC gets bonus for multiplexing capability
                score *= 0.9;
            } else if (type == TransportType::TCP) {
                // TCP gets bonus for reliability on congested networks
                if (quality.isCongested) {
                    score *= 0.85;
                }
            } else if (type == TransportType::RELAY) {
                // Relay gets penalty unless direct connections are failing
                score *= 1.3;
            }
            
            if (score < bestScore) {
                auto transportIt = transports_.find(type);
                if (transportIt != transports_.end()) {
                    best = transportIt->second.get();
                    bestScore = score;
                }
            }
        }
    }
    
    if (best) return best;
    
    // Fallback to priority order
    return selectByStrategy(peerId);
}

std::vector<TransportType> TransportRegistry::getFailoverOrder() const {
    return priorityOrder_;
}

} // namespace NetFalcon
} // namespace SentinelFS
