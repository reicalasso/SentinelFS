/**
 * @file FalconStore.h
 * @brief FalconStore - High-performance storage plugin for SentinelFS
 * 
 * Features:
 * - Schema migration system
 * - Connection pooling
 * - LRU cache layer
 * - Type-safe query builder
 * - Async operations
 * - Transaction support
 */

#pragma once

#include "IStorageAPI.h"
#include "IPlugin.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <chrono>
#include <optional>
#include <variant>
#include <future>

namespace SentinelFS {
namespace Falcon {

// ============================================================================
// Configuration
// ============================================================================

struct FalconConfig {
    // Database
    std::string dbPath;
    int schemaVersion{1};
    
    // Connection pool
    int poolSize{4};
    std::chrono::seconds connectionTimeout{30};
    
    // Cache
    bool enableCache{true};
    size_t cacheMaxSize{10000};      // Max entries
    size_t cacheMaxMemory{64 * 1024 * 1024};  // 64MB
    std::chrono::seconds cacheTTL{300};  // 5 minutes
    
    // Performance
    bool enableWAL{true};           // Write-Ahead Logging
    bool enableForeignKeys{true};
    int busyTimeout{5000};          // ms
    bool synchronous{false};        // NORMAL vs FULL
    
    // Maintenance
    bool autoVacuum{true};
    std::chrono::hours vacuumInterval{24};
};

// ============================================================================
// Query Builder Types
// ============================================================================

enum class OrderDirection { ASC, DESC };
enum class JoinType { INNER, LEFT, RIGHT, FULL };

/**
 * @brief Type-safe value for query parameters
 */
using QueryValue = std::variant<
    std::nullptr_t,
    bool,
    int,
    int64_t,
    double,
    std::string,
    std::vector<uint8_t>
>;

/**
 * @brief Query condition
 */
struct Condition {
    std::string column;
    std::string op;  // =, !=, <, >, <=, >=, LIKE, IN, IS NULL
    QueryValue value;
    std::string logic{"AND"};  // AND, OR
};

/**
 * @brief Order specification
 */
struct OrderSpec {
    std::string column;
    OrderDirection direction{OrderDirection::ASC};
};

/**
 * @brief Join specification
 */
struct JoinSpec {
    JoinType type{JoinType::INNER};
    std::string table;
    std::string onLeft;
    std::string onRight;
};

// ============================================================================
// Query Result
// ============================================================================

/**
 * @brief Single row from query result
 */
class Row {
public:
    virtual ~Row() = default;
    
    virtual bool isNull(const std::string& column) const = 0;
    virtual int getInt(const std::string& column) const = 0;
    virtual int64_t getInt64(const std::string& column) const = 0;
    virtual double getDouble(const std::string& column) const = 0;
    virtual std::string getString(const std::string& column) const = 0;
    virtual std::vector<uint8_t> getBlob(const std::string& column) const = 0;
    
    // Convenience with defaults
    template<typename T>
    T get(const std::string& column, T defaultValue = T{}) const;
};

/**
 * @brief Query result set
 */
class ResultSet {
public:
    virtual ~ResultSet() = default;
    
    virtual bool next() = 0;
    virtual const Row& current() const = 0;
    virtual size_t rowCount() const = 0;
    virtual bool empty() const = 0;
    
    // Iteration support
    virtual void reset() = 0;
};

// ============================================================================
// Query Builder
// ============================================================================

class QueryBuilder {
public:
    virtual ~QueryBuilder() = default;
    
    // SELECT
    virtual QueryBuilder& select(const std::vector<std::string>& columns = {"*"}) = 0;
    virtual QueryBuilder& selectDistinct(const std::vector<std::string>& columns) = 0;
    virtual QueryBuilder& from(const std::string& table) = 0;
    
    // JOIN
    virtual QueryBuilder& join(const JoinSpec& spec) = 0;
    virtual QueryBuilder& innerJoin(const std::string& table, const std::string& onLeft, const std::string& onRight) = 0;
    virtual QueryBuilder& leftJoin(const std::string& table, const std::string& onLeft, const std::string& onRight) = 0;
    
    // WHERE
    virtual QueryBuilder& where(const std::string& column, const std::string& op, const QueryValue& value) = 0;
    virtual QueryBuilder& whereNull(const std::string& column) = 0;
    virtual QueryBuilder& whereNotNull(const std::string& column) = 0;
    virtual QueryBuilder& whereIn(const std::string& column, const std::vector<QueryValue>& values) = 0;
    virtual QueryBuilder& whereBetween(const std::string& column, const QueryValue& low, const QueryValue& high) = 0;
    virtual QueryBuilder& orWhere(const std::string& column, const std::string& op, const QueryValue& value) = 0;
    
    // ORDER, GROUP, LIMIT
    virtual QueryBuilder& orderBy(const std::string& column, OrderDirection dir = OrderDirection::ASC) = 0;
    virtual QueryBuilder& groupBy(const std::vector<std::string>& columns) = 0;
    virtual QueryBuilder& having(const std::string& condition) = 0;
    virtual QueryBuilder& limit(int count) = 0;
    virtual QueryBuilder& offset(int count) = 0;
    
    // Execute
    virtual std::unique_ptr<ResultSet> execute() = 0;
    virtual std::future<std::unique_ptr<ResultSet>> executeAsync() = 0;
    
    // Get generated SQL (for debugging)
    virtual std::string toSql() const = 0;
};

// ============================================================================
// Transaction
// ============================================================================

class Transaction {
public:
    virtual ~Transaction() = default;
    
    virtual void commit() = 0;
    virtual void rollback() = 0;
    virtual bool isActive() const = 0;
    
    // Execute within transaction
    virtual bool execute(const std::string& sql) = 0;
    virtual std::unique_ptr<QueryBuilder> query() = 0;
};

// ============================================================================
// Migration System
// ============================================================================

/**
 * @brief Single migration step
 */
struct Migration {
    int version;
    std::string name;
    std::string upSql;
    std::string downSql;
    std::function<bool(void*)> upCallback;   // For complex migrations
    std::function<bool(void*)> downCallback;
};

/**
 * @brief Migration manager interface
 */
class IMigrationManager {
public:
    virtual ~IMigrationManager() = default;
    
    virtual void registerMigration(const Migration& migration) = 0;
    virtual int getCurrentVersion() const = 0;
    virtual int getLatestVersion() const = 0;
    virtual bool migrateUp(int targetVersion = -1) = 0;  // -1 = latest
    virtual bool migrateDown(int targetVersion) = 0;
    virtual std::vector<Migration> getPendingMigrations() const = 0;
};

// ============================================================================
// Cache Interface
// ============================================================================

/**
 * @brief Cache statistics
 */
struct CacheStats {
    size_t hits{0};
    size_t misses{0};
    size_t entries{0};
    size_t memoryUsed{0};
    double hitRate() const { return hits + misses > 0 ? double(hits) / (hits + misses) : 0; }
};

/**
 * @brief LRU Cache interface
 */
class ICache {
public:
    virtual ~ICache() = default;
    
    virtual void put(const std::string& key, const std::string& value, std::chrono::seconds ttl = std::chrono::seconds{0}) = 0;
    virtual std::optional<std::string> get(const std::string& key) = 0;
    virtual bool exists(const std::string& key) const = 0;
    virtual void invalidate(const std::string& key) = 0;
    virtual void invalidatePrefix(const std::string& prefix) = 0;
    virtual void clear() = 0;
    virtual CacheStats getStats() const = 0;
};

// ============================================================================
// Storage Statistics
// ============================================================================

struct FalconStats {
    // Queries
    uint64_t totalQueries{0};
    uint64_t selectQueries{0};
    uint64_t insertQueries{0};
    uint64_t updateQueries{0};
    uint64_t deleteQueries{0};
    
    // Performance
    double avgQueryTimeMs{0};
    double maxQueryTimeMs{0};
    uint64_t slowQueries{0};  // > 100ms
    
    // Cache
    CacheStats cache;
    
    // Connection pool
    int activeConnections{0};
    int idleConnections{0};
    uint64_t connectionWaits{0};
    
    // Database
    int64_t dbSizeBytes{0};
    int schemaVersion{0};
};

} // namespace Falcon

// ============================================================================
// Main Plugin Class
// ============================================================================

/**
 * @brief FalconStore - High-performance storage plugin
 */
class FalconStore : public IStorageAPI {
public:
    FalconStore();
    ~FalconStore() override;
    
    // IPlugin interface
    bool initialize(EventBus* eventBus) override;
    void shutdown() override;
    std::string getName() const override { return "FalconStore"; }
    std::string getVersion() const override { return "1.0.0"; }
    void setStoragePlugin(IStorageAPI* /*storage*/) override {}  // We ARE the storage
    
    // IStorageAPI interface
    // File operations
    bool addFile(const std::string& path, const std::string& hash, long long timestamp, long long size) override;
    std::optional<FileMetadata> getFile(const std::string& path) override;
    bool removeFile(const std::string& path) override;
    
    // Peer operations
    bool addPeer(const PeerInfo& peer) override;
    std::optional<PeerInfo> getPeer(const std::string& peerId) override;
    std::vector<PeerInfo> getAllPeers() override;
    bool updatePeerLatency(const std::string& peerId, int latency) override;
    std::vector<PeerInfo> getPeersByLatency() override;
    bool removePeer(const std::string& peerId) override;
    
    // Conflict operations
    bool addConflict(const ConflictInfo& conflict) override;
    std::vector<ConflictInfo> getUnresolvedConflicts() override;
    std::vector<ConflictInfo> getConflictsForFile(const std::string& path) override;
    bool markConflictResolved(int conflictId, int strategy = 0) override;
    std::pair<int, int> getConflictStats() override;
    
    // Sync queue / Access log
    bool enqueueSyncOperation(const std::string& filePath, const std::string& opType, const std::string& status) override;
    bool logFileAccess(const std::string& filePath, const std::string& opType, const std::string& deviceId, long long timestamp) override;
    
    void* getDB() override;
    
    // DatabaseManager support
    DatabaseManager* getDatabaseManager() override;
    void setDatabaseManager(DatabaseManager* dbManager) override;
    
    // Watched folder operations
    bool addWatchedFolder(const std::string& path) override;
    bool removeWatchedFolder(const std::string& path) override;
    std::vector<WatchedFolder> getWatchedFolders() override;
    bool isWatchedFolder(const std::string& path) override;
    bool updateWatchedFolderStatus(const std::string& path, int statusId) override;
    
    // Bulk file operations
    std::vector<FileMetadata> getFilesInFolder(const std::string& folderPath) override;
    int removeFilesInFolder(const std::string& folderPath) override;
    int getFileCount() override;
    long long getTotalFileSize() override;
    bool markFileSynced(const std::string& path, bool synced = true) override;
    std::vector<FileMetadata> getPendingFiles() override;
    
    // Ignore patterns
    bool addIgnorePattern(const std::string& pattern) override;
    bool removeIgnorePattern(const std::string& pattern) override;
    std::vector<std::string> getIgnorePatterns() override;
    
    // Threat operations
    bool addThreat(const ThreatInfo& threat) override;
    std::vector<ThreatInfo> getThreats() override;
    bool removeThreat(int threatId) override;
    int removeThreatsInFolder(const std::string& folderPath) override;
    bool markThreatSafe(int threatId, bool safe = true) override;
    
    // Sync queue operations
    std::vector<SyncQueueItem> getSyncQueue() override;
    bool updateSyncQueueStatus(int itemId, const std::string& status) override;
    int clearCompletedSyncOperations() override;
    
    // Activity log
    std::vector<ActivityLogEntry> getRecentActivity(int limit = 50) override;
    
    // Peer extended operations
    bool removeAllPeers() override;
    bool updatePeerStatus(const std::string& peerId, const std::string& status) override;
    bool blockPeer(const std::string& peerId) override;
    bool unblockPeer(const std::string& peerId) override;
    bool isPeerBlocked(const std::string& peerId) override;
    
    // Batch operations for performance
    bool batchUpdatePeerLatencies(const std::map<std::string, int>& latencies);
    int batchUpsertPeers(const std::vector<PeerInfo>& peers);
    std::map<std::string, PeerInfo> batchGetPeers(const std::vector<std::string>& peerIds);
    
    // Config storage
    bool setConfig(const std::string& key, const std::string& value) override;
    std::optional<std::string> getConfig(const std::string& key) override;
    bool removeConfig(const std::string& key) override;
    
    // Transfer history
    bool logTransfer(const std::string& filePath, const std::string& peerId, 
                     const std::string& direction, long long bytes, bool success) override;
    std::vector<std::pair<std::string, long long>> getTransferHistory(int limit = 50) override;
    
    // FalconStore specific API
    
    /**
     * @brief Configure the storage plugin
     */
    void configure(const Falcon::FalconConfig& config);
    
    /**
     * @brief Get query builder for custom queries
     */
    std::unique_ptr<Falcon::QueryBuilder> query();
    
    /**
     * @brief Begin a transaction
     */
    std::unique_ptr<Falcon::Transaction> beginTransaction();
    
    /**
     * @brief Execute raw SQL (use with caution)
     */
    bool execute(const std::string& sql);
    
    /**
     * @brief Execute with parameters
     */
    bool execute(const std::string& sql, const std::vector<Falcon::QueryValue>& params);
    
    /**
     * @brief Get migration manager
     */
    Falcon::IMigrationManager* getMigrationManager();
    
    /**
     * @brief Get cache interface
     */
    Falcon::ICache* getCache();
    
    /**
     * @brief Get statistics
     */
    Falcon::FalconStats getStats() const;
    
    /**
     * @brief Optimize database (vacuum, analyze)
     */
    void optimize();
    
    /**
     * @brief Backup database to file
     */
    bool backup(const std::string& path);
    
    /**
     * @brief Restore database from backup
     */
    bool restore(const std::string& path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Plugin factory
extern "C" {
    IPlugin* create_plugin();
    void destroy_plugin(IPlugin* plugin);
}

} // namespace SentinelFS
