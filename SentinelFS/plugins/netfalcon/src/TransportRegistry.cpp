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
        currentType = bindingIt->second.activeTransport;
        bindingIt->second.failoverCount++;
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
                } else {
                    PeerTransportBinding binding;
                    binding.peerId = peerId;
                    binding.activeTransport = type;
                    binding.preferredTransport = type;
                    binding.boundAt = std::chrono::steady_clock::now();
                    bindings_[peerId] = binding;
                }
                return it->second.get();
            }
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
    // Adaptive selection: combine RTT, packet loss, and jitter
    ITransport* best = nullptr;
    double bestScore = std::numeric_limits<double>::max();
    
    auto qualityIt = qualityCache_.find(peerId);
    if (qualityIt != qualityCache_.end()) {
        for (const auto& [type, quality] : qualityIt->second) {
            // Score = RTT + (jitter * 2) + (packetLoss * 10)
            double score = quality.rttMs + (quality.jitterMs * 2.0) + (quality.packetLossPercent * 10.0);
            
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
    
    // Fallback to first available
    return selectByStrategy(peerId);
}

std::vector<TransportType> TransportRegistry::getFailoverOrder() const {
    return priorityOrder_;
}

} // namespace NetFalcon
} // namespace SentinelFS
