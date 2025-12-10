#pragma once

#include "IStorageAPI.h"
#include "SQLiteHandler.h"
#include <vector>
#include <optional>

namespace SentinelFS {

/**
 * @brief Manages peer information and operations
 */
class PeerManager {
public:
    explicit PeerManager(SQLiteHandler* handler) : handler_(handler) {}

    /**
     * @brief Add or update peer information
     */
    bool addPeer(const PeerInfo& peer);

    /**
     * @brief Get peer information by ID
     */
    std::optional<PeerInfo> getPeer(const std::string& peerId);

    /**
     * @brief Get all known peers
     */
    std::vector<PeerInfo> getAllPeers();

    /**
     * @brief Update peer latency measurement
     */
    bool updatePeerLatency(const std::string& peerId, int latency);

    /**
     * @brief Get peers sorted by latency (fastest first)
     */
    std::vector<PeerInfo> getPeersByLatency();
    
    /**
     * @brief Remove a peer by ID
     */
    bool removePeer(const std::string& peerId);

private:
    SQLiteHandler* handler_;
};

} // namespace SentinelFS
