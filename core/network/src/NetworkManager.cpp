#include "NetworkManager.h"

#include <algorithm>

namespace SentinelFS {

    NetworkManager::NetworkManager(INetworkAPI* plugin)
        : plugin_(plugin) {
    }

    void NetworkManager::setPreferredTransport(TransportType type) {
        preferredTransport_ = type;
    }

    void NetworkManager::setPoolSize(std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        poolSize_ = std::max<std::size_t>(1, size);
        ensureCapacityLocked();
    }

    bool NetworkManager::connect(const std::string& peerId,
                                 const std::string& address,
                                 int port,
                                 TransportType transport) {
        if (!plugin_) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto existing = connections_.find(peerId);
        if (existing != connections_.end()) {
            touchConnection(peerId);
            return true;
        }

        if (!ensureCapacityLocked()) {
            return false;
        }

        TransportType resolvedTransport = (transport == TransportType::TCP)
                                              ? TransportType::TCP
                                              : preferredTransport_;

        bool connected = false;
        switch (resolvedTransport) {
            case TransportType::TCP:
                connected = plugin_->connectToPeer(address, port);
                break;
            case TransportType::QUIC:
            case TransportType::WEBRTC:
                // Placeholder for future transport implementations
                connected = plugin_->connectToPeer(address, port);
                break;
        }

        if (connected) {
            ConnectionEntry entry;
            entry.peerId = peerId;
            entry.address = address;
            entry.port = port;
            entry.transport = resolvedTransport;
            entry.lastUsed = std::chrono::steady_clock::now();
            connections_[peerId] = std::move(entry);
        }

        return connected;
    }

    bool NetworkManager::send(const std::string& peerId, const std::vector<uint8_t>& data) {
        if (!plugin_) {
            return false;
        }

        bool result = plugin_->sendData(peerId, data);
        if (result) {
            std::lock_guard<std::mutex> lock(mutex_);
            touchConnection(peerId);
        }
        return result;
    }

    void NetworkManager::disconnect(const std::string& peerId) {
        if (!plugin_) {
            return;
        }

        plugin_->disconnectPeer(peerId);
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.erase(peerId);
    }

    bool NetworkManager::isConnected(const std::string& peerId) const {
        if (!plugin_) {
            return false;
        }
        return plugin_->isPeerConnected(peerId);
    }

    void NetworkManager::pruneIdle(std::chrono::seconds idleThreshold) {
        if (idleThreshold.count() <= 0) {
            return;
        }

        std::vector<std::string> toRemove;
        auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& pair : connections_) {
                if ((now - pair.second.lastUsed) > idleThreshold) {
                    toRemove.push_back(pair.first);
                }
            }
        }

        for (const auto& peerId : toRemove) {
            disconnect(peerId);
        }
    }

    std::vector<NetworkManager::ConnectionEntry> NetworkManager::snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ConnectionEntry> entries;
        entries.reserve(connections_.size());
        for (const auto& pair : connections_) {
            entries.push_back(pair.second);
        }
        return entries;
    }

    bool NetworkManager::ensureCapacityLocked() {
        if (connections_.size() < poolSize_) {
            return true;
        }

        auto oldest = std::min_element(
            connections_.begin(),
            connections_.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.second.lastUsed < rhs.second.lastUsed;
            });

        if (oldest == connections_.end()) {
            return false;
        }

        plugin_->disconnectPeer(oldest->first);
        connections_.erase(oldest);
        return true;
    }

    void NetworkManager::touchConnection(const std::string& peerId) {
        auto it = connections_.find(peerId);
        if (it != connections_.end()) {
            it->second.lastUsed = std::chrono::steady_clock::now();
        }
    }

} // namespace SentinelFS
