/**
 * @file BatchQueries.h
 * @brief Batch database operations for performance optimization
 * 
 * Provides utilities to reduce N+1 query problems:
 * - Batch peer insertions/updates
 * - Bulk file operations
 * - Transaction management
 */

#pragma once

#include "IStorageAPI.h"
#include <vector>
#include <string>
#include <map>
#include <functional>

namespace SentinelFS {

/**
 * @brief Batch query helper for storage operations
 */
class BatchQueries {
public:
    /**
     * @brief Batch insert/update peers
     * @param storage Storage plugin
     * @param peers List of peers to upsert
     * @return Number of successfully processed peers
     */
    static int batchUpsertPeers(IStorageAPI* storage, const std::vector<PeerInfo>& peers);
    
    /**
     * @brief Batch get peer info by IDs
     * @param storage Storage plugin
     * @param peerIds List of peer IDs
     * @return Map of peer ID to PeerInfo
     */
    static std::map<std::string, PeerInfo> batchGetPeers(
        IStorageAPI* storage, 
        const std::vector<std::string>& peerIds);
    
    /**
     * @brief Execute multiple operations in a transaction
     * @param storage Storage plugin
     * @param operations List of operations to execute
     * @return true if all operations succeeded
     */
    static bool executeInTransaction(
        IStorageAPI* storage,
        std::function<bool()> operations);
    
    /**
     * @brief Batch update peer latencies
     * @param storage Storage plugin
     * @param latencies Map of peer ID to latency value
     */
    static void batchUpdateLatencies(
        IStorageAPI* storage,
        const std::map<std::string, int>& latencies);
};

} // namespace SentinelFS
