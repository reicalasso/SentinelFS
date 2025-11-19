#pragma once

#include "IPlugin.h"
#include <string>
#include <optional>
#include <vector>

namespace SentinelFS {

    struct FileMetadata {
        std::string path;
        std::string hash;
        long long timestamp;
        long long size;
    };

    struct PeerInfo {
        std::string id;
        std::string ip;
        int port;
        long long lastSeen;
        std::string status; // "active", "offline"
        int latency; // RTT in milliseconds, -1 if not measured
    };

    class IStorageAPI : public IPlugin {
    public:
        virtual ~IStorageAPI() = default;

        // --- File Operations ---

        /**
         * @brief Add or update file metadata in the storage.
         */
        virtual bool addFile(const std::string& path, const std::string& hash, long long timestamp, long long size) = 0;

        /**
         * @brief Retrieve file metadata by path.
         */
        virtual std::optional<FileMetadata> getFile(const std::string& path) = 0;

        /**
         * @brief Remove file metadata by path.
         */
        virtual bool removeFile(const std::string& path) = 0;

        // --- Peer Operations ---

        /**
         * @brief Add or update a peer in the storage.
         */
        virtual bool addPeer(const PeerInfo& peer) = 0;

        /**
         * @brief Get a peer by ID.
         */
        virtual std::optional<PeerInfo> getPeer(const std::string& peerId) = 0;

        /**
         * @brief Get all known peers.
         */
        virtual std::vector<PeerInfo> getAllPeers() = 0;

        /**
         * @brief Update peer latency.
         * @param peerId The ID of the peer.
         * @param latency RTT in milliseconds.
         */
        virtual bool updatePeerLatency(const std::string& peerId, int latency) = 0;

        /**
         * @brief Get all peers sorted by latency (lowest first).
         * @return Vector of peers sorted by latency, offline/unknown latency peers at end.
         */
        virtual std::vector<PeerInfo> getPeersByLatency() = 0;
    };
}


