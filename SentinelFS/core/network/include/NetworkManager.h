#pragma once

#include "INetworkAPI.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <chrono>

namespace SentinelFS {

    /**
     * @brief Lightweight wrapper around INetworkAPI that adds connection pooling
     *        and transport preference scaffolding for future transports (QUIC, etc.).
     */
    class NetworkManager {
    public:
        enum class TransportType {
            TCP,
            QUIC,
            WEBRTC
        };

        struct ConnectionEntry {
            std::string peerId;
            std::string address;
            int port{0};
            TransportType transport{TransportType::TCP};
            std::chrono::steady_clock::time_point lastUsed;
        };

        explicit NetworkManager(INetworkAPI* plugin);

        void setPreferredTransport(TransportType type);
        void setPoolSize(std::size_t size);

        bool connect(const std::string& peerId,
                     const std::string& address,
                     int port,
                     TransportType transport = TransportType::TCP);

        bool send(const std::string& peerId, const std::vector<uint8_t>& data);
        void disconnect(const std::string& peerId);
        bool isConnected(const std::string& peerId) const;

        void pruneIdle(std::chrono::seconds idleThreshold);
        std::vector<ConnectionEntry> snapshot() const;

    private:
        bool ensureCapacityLocked();
        void touchConnection(const std::string& peerId);

        INetworkAPI* plugin_{nullptr}; // non-owning
        TransportType preferredTransport_{TransportType::TCP};
        std::size_t poolSize_{32};

        mutable std::mutex mutex_;
        std::unordered_map<std::string, ConnectionEntry> connections_;
    };

} // namespace SentinelFS
