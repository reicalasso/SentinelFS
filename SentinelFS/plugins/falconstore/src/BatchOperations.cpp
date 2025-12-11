/**
 * @file BatchOperations.cpp
 * @brief Batch database operations for FalconStore
 */

#include "FalconStore.h"
#include "BatchQueries.h"
#include "Logger.h"
#include <sqlite3.h>

namespace SentinelFS {

/**
 * @brief Batch update peer latencies using BatchQueries utility
 */
bool FalconStore::batchUpdatePeerLatencies(const std::map<std::string, int>& latencies) {
    BatchQueries::batchUpdateLatencies(this, latencies);
    return true;
}

/**
 * @brief Batch insert/update peers using transaction
 */
int FalconStore::batchUpsertPeers(const std::vector<PeerInfo>& peers) {
    return BatchQueries::batchUpsertPeers(this, peers);
}

/**
 * @brief Get multiple peers efficiently using IN clause
 */
std::map<std::string, PeerInfo> FalconStore::batchGetPeers(const std::vector<std::string>& peerIds) {
    return BatchQueries::batchGetPeers(this, peerIds);
}

} // namespace SentinelFS
