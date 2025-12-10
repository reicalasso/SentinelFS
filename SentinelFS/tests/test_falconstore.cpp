/**
 * @file test_falconstore.cpp
 * @brief FalconStore plugin integration tests
 */

#include <gtest/gtest.h>
#include "FalconStore.h"
#include "EventBus.h"
#include <filesystem>
#include <chrono>

using namespace SentinelFS;

class FalconStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use temp directory for test database
        testDbPath_ = "/tmp/falconstore_test_" + 
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".db";
        
        store_ = std::make_unique<FalconStore>();
        
        // Configure with test path
        Falcon::FalconConfig config;
        config.dbPath = testDbPath_;
        config.enableCache = true;
        config.cacheMaxSize = 100;
        store_->configure(config);
        
        // Initialize
        ASSERT_TRUE(store_->initialize(&eventBus_));
    }
    
    void TearDown() override {
        store_->shutdown();
        store_.reset();
        
        // Cleanup test database
        std::filesystem::remove(testDbPath_);
        std::filesystem::remove(testDbPath_ + "-wal");
        std::filesystem::remove(testDbPath_ + "-shm");
    }
    
    std::string testDbPath_;
    EventBus eventBus_;
    std::unique_ptr<FalconStore> store_;
};

// ============================================================================
// Basic Tests
// ============================================================================

TEST_F(FalconStoreTest, Initialize) {
    EXPECT_EQ(store_->getName(), "FalconStore");
    EXPECT_EQ(store_->getVersion(), "1.0.0");
    EXPECT_NE(store_->getDB(), nullptr);
}

TEST_F(FalconStoreTest, MigrationVersion) {
    auto* migrationManager = store_->getMigrationManager();
    ASSERT_NE(migrationManager, nullptr);
    
    // Should be at latest version after initialization
    int currentVersion = migrationManager->getCurrentVersion();
    int latestVersion = migrationManager->getLatestVersion();
    
    EXPECT_EQ(currentVersion, latestVersion);
    EXPECT_GE(currentVersion, 1);
    
    // No pending migrations
    auto pending = migrationManager->getPendingMigrations();
    EXPECT_TRUE(pending.empty());
}

// ============================================================================
// File Operations
// ============================================================================

TEST_F(FalconStoreTest, AddAndGetFile) {
    std::string path = "/test/file.txt";
    std::string hash = "abc123def456";
    long long timestamp = 1234567890;
    long long size = 1024;
    
    // Add file
    EXPECT_TRUE(store_->addFile(path, hash, timestamp, size));
    
    // Get file
    auto file = store_->getFile(path);
    ASSERT_TRUE(file.has_value());
    EXPECT_EQ(file->path, path);
    EXPECT_EQ(file->hash, hash);
    EXPECT_EQ(file->timestamp, timestamp);
    EXPECT_EQ(file->size, size);
}

TEST_F(FalconStoreTest, UpdateFile) {
    std::string path = "/test/file.txt";
    
    // Add initial
    EXPECT_TRUE(store_->addFile(path, "hash1", 1000, 100));
    
    // Update
    EXPECT_TRUE(store_->addFile(path, "hash2", 2000, 200));
    
    // Verify update
    auto file = store_->getFile(path);
    ASSERT_TRUE(file.has_value());
    EXPECT_EQ(file->hash, "hash2");
    EXPECT_EQ(file->timestamp, 2000);
    EXPECT_EQ(file->size, 200);
}

TEST_F(FalconStoreTest, RemoveFile) {
    std::string path = "/test/file.txt";
    
    // Add
    EXPECT_TRUE(store_->addFile(path, "hash", 1000, 100));
    EXPECT_TRUE(store_->getFile(path).has_value());
    
    // Remove
    EXPECT_TRUE(store_->removeFile(path));
    EXPECT_FALSE(store_->getFile(path).has_value());
}

TEST_F(FalconStoreTest, GetNonExistentFile) {
    auto file = store_->getFile("/nonexistent/path");
    EXPECT_FALSE(file.has_value());
}

// ============================================================================
// Peer Operations
// ============================================================================

TEST_F(FalconStoreTest, AddAndGetPeer) {
    PeerInfo peer;
    peer.id = "peer-123";
    peer.ip = "192.168.1.100";
    peer.port = 8080;
    peer.status = "active";
    peer.lastSeen = 1234567890;
    peer.latency = 50;
    
    // Add peer
    EXPECT_TRUE(store_->addPeer(peer));
    
    // Get peer
    auto retrieved = store_->getPeer("peer-123");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, "peer-123");
    EXPECT_EQ(retrieved->ip, "192.168.1.100");
    EXPECT_EQ(retrieved->port, 8080);
    EXPECT_EQ(retrieved->latency, 50);
}

TEST_F(FalconStoreTest, GetAllPeers) {
    // Add multiple peers
    for (int i = 0; i < 5; i++) {
        PeerInfo peer;
        peer.id = "peer-" + std::to_string(i);
        peer.ip = "192.168.1." + std::to_string(100 + i);
        peer.port = 8080 + i;
        peer.status = "active";
        peer.lastSeen = 1234567890;
        peer.latency = 10 * i;
        EXPECT_TRUE(store_->addPeer(peer));
    }
    
    auto peers = store_->getAllPeers();
    EXPECT_EQ(peers.size(), 5);
}

TEST_F(FalconStoreTest, UpdatePeerLatency) {
    PeerInfo peer;
    peer.id = "peer-latency";
    peer.ip = "192.168.1.1";
    peer.port = 8080;
    peer.status = "active";
    peer.lastSeen = 1234567890;
    peer.latency = 100;
    
    EXPECT_TRUE(store_->addPeer(peer));
    EXPECT_TRUE(store_->updatePeerLatency("peer-latency", 25));
    
    auto retrieved = store_->getPeer("peer-latency");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->latency, 25);
}

TEST_F(FalconStoreTest, GetPeersByLatency) {
    // Add peers with different latencies
    for (int i = 0; i < 5; i++) {
        PeerInfo peer;
        peer.id = "peer-" + std::to_string(i);
        peer.ip = "192.168.1." + std::to_string(i);
        peer.port = 8080;
        peer.status = "active";
        peer.lastSeen = 1234567890;
        peer.latency = (5 - i) * 10;  // 50, 40, 30, 20, 10
        EXPECT_TRUE(store_->addPeer(peer));
    }
    
    auto peers = store_->getPeersByLatency();
    ASSERT_EQ(peers.size(), 5);
    
    // Should be sorted by latency (ascending)
    EXPECT_EQ(peers[0].latency, 10);
    EXPECT_EQ(peers[4].latency, 50);
}

TEST_F(FalconStoreTest, RemovePeer) {
    PeerInfo peer;
    peer.id = "peer-remove";
    peer.ip = "192.168.1.1";
    peer.port = 8080;
    peer.status = "active";
    peer.lastSeen = 1234567890;
    peer.latency = 50;
    
    EXPECT_TRUE(store_->addPeer(peer));
    EXPECT_TRUE(store_->getPeer("peer-remove").has_value());
    
    EXPECT_TRUE(store_->removePeer("peer-remove"));
    EXPECT_FALSE(store_->getPeer("peer-remove").has_value());
}

// ============================================================================
// Conflict Operations
// ============================================================================

TEST_F(FalconStoreTest, ConflictStats) {
    auto [total, unresolved] = store_->getConflictStats();
    EXPECT_EQ(total, 0);
    EXPECT_EQ(unresolved, 0);
}

// ============================================================================
// Cache Tests
// ============================================================================

TEST_F(FalconStoreTest, CacheExists) {
    auto* cache = store_->getCache();
    ASSERT_NE(cache, nullptr);
    
    // Initial stats
    auto stats = cache->getStats();
    EXPECT_EQ(stats.entries, 0);
    EXPECT_EQ(stats.hits, 0);
    EXPECT_EQ(stats.misses, 0);
}

TEST_F(FalconStoreTest, CachePutGet) {
    auto* cache = store_->getCache();
    ASSERT_NE(cache, nullptr);
    
    cache->put("key1", "value1");
    
    auto value = cache->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "value1");
    
    auto stats = cache->getStats();
    EXPECT_EQ(stats.hits, 1);
}

TEST_F(FalconStoreTest, CacheInvalidate) {
    auto* cache = store_->getCache();
    ASSERT_NE(cache, nullptr);
    
    cache->put("key1", "value1");
    EXPECT_TRUE(cache->exists("key1"));
    
    cache->invalidate("key1");
    EXPECT_FALSE(cache->exists("key1"));
}

// ============================================================================
// Query Builder Tests
// ============================================================================

TEST_F(FalconStoreTest, QueryBuilder) {
    // Add some test data
    for (int i = 0; i < 10; i++) {
        store_->addFile("/test/file" + std::to_string(i) + ".txt", 
                        "hash" + std::to_string(i), 
                        1000 + i, 
                        100 * (i + 1));
    }
    
    // Query with builder
    auto query = store_->query();
    ASSERT_NE(query, nullptr);
    
    query->select({"path", "size"});
    query->from("files");
    query->where("size", ">", 500);
    query->orderBy("size", Falcon::OrderDirection::ASC);
    query->limit(5);
    
    auto results = query->execute();
    ASSERT_NE(results, nullptr);
    
    int count = 0;
    while (results->next()) {
        count++;
    }
    EXPECT_EQ(count, 5);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(FalconStoreTest, Statistics) {
    // Perform some operations
    store_->addFile("/test/file1.txt", "hash1", 1000, 100);
    store_->addFile("/test/file2.txt", "hash2", 2000, 200);
    store_->getFile("/test/file1.txt");
    store_->removeFile("/test/file2.txt");
    
    auto stats = store_->getStats();
    
    EXPECT_GT(stats.totalQueries, 0);
    EXPECT_GT(stats.insertQueries, 0);
    EXPECT_GT(stats.selectQueries, 0);
    EXPECT_GT(stats.deleteQueries, 0);
    EXPECT_GE(stats.avgQueryTimeMs, 0);
}

// ============================================================================
// Backup/Restore Tests
// ============================================================================

TEST_F(FalconStoreTest, Backup) {
    // Add some data
    store_->addFile("/test/file.txt", "hash", 1000, 100);
    
    std::string backupPath = testDbPath_ + ".backup";
    
    // Backup
    EXPECT_TRUE(store_->backup(backupPath));
    EXPECT_TRUE(std::filesystem::exists(backupPath));
    
    // Cleanup
    std::filesystem::remove(backupPath);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
