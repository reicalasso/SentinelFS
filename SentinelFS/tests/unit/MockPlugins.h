#pragma once

#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "IFileAPI.h"
#include "EventBus.h"
#include <iostream>
#include <map>
#include <vector>
#include <string>

namespace SentinelFS {

class MockPlugin : public virtual IPlugin {
public:
    bool initialize(EventBus* eventBus) override { return true; }
    void shutdown() override {}
    std::string getName() const override { return "MockPlugin"; }
    std::string getVersion() const override { return "1.0.0"; }
};

class MockNetwork : public INetworkAPI {
public:
    bool initialize(EventBus* eventBus) override { return true; }
    void shutdown() override {}
    std::string getName() const override { return "MockNetwork"; }
    std::string getVersion() const override { return "1.0.0"; }

    bool connectToPeer(const std::string& address, int port) override { return true; }
    bool sendData(const std::string& peerId, const std::vector<uint8_t>& data) override { 
        sentData[peerId] = data;
        return true; 
    }
    void startListening(int port) override {}
    void startDiscovery(int port) override {}
    void broadcastPresence(int discoveryPort, int tcpPort) override {}
    int measureRTT(const std::string& peerId) override { return 10; }
    int getPeerRTT(const std::string& peerId) override { return 10; }
    void disconnectPeer(const std::string& peerId) override {}
    bool isPeerConnected(const std::string& peerId) override { return true; }
    void setSessionCode(const std::string& code) override {}
    std::string getSessionCode() const override { return "123456"; }
    void setEncryptionEnabled(bool enable) override {}
    bool isEncryptionEnabled() const override { return false; }
    void setGlobalUploadLimit(std::size_t bytesPerSecond) override {}
    void setGlobalDownloadLimit(std::size_t bytesPerSecond) override {}
    std::string getBandwidthStats() const override { return ""; }
    void setRelayEnabled(bool enabled) override {}
    bool isRelayEnabled() const override { return false; }
    bool isRelayConnected() const override { return false; }
    std::string getLocalPeerId() const override { return "local-peer"; }
    int getLocalPort() const override { return 8080; }
    bool connectToRelay(const std::string& host, int port, const std::string& sessionCode) override { return true; }
    void disconnectFromRelay() override {}
    std::vector<RelayPeerInfo> getRelayPeers() const override { return {}; }

    std::map<std::string, std::vector<uint8_t>> sentData;
};

class MockStorage : public IStorageAPI {
public:
    bool initialize(EventBus* eventBus) override { return true; }
    void shutdown() override {}
    std::string getName() const override { return "MockStorage"; }
    std::string getVersion() const override { return "1.0.0"; }

    bool addFile(const std::string& path, const std::string& hash, long long timestamp, long long size) override { return true; }
    std::optional<FileMetadata> getFile(const std::string& path) override { return std::nullopt; }
    bool removeFile(const std::string& path) override { return true; }
    bool addPeer(const PeerInfo& peer) override { return true; }
    std::optional<PeerInfo> getPeer(const std::string& peerId) override { return std::nullopt; }
    std::vector<PeerInfo> getAllPeers() override { return {}; }
    bool updatePeerLatency(const std::string& peerId, int latency) override { return true; }
    std::vector<PeerInfo> getPeersByLatency() override { return {}; }
    bool removePeer(const std::string& peerId) override { return true; }
    bool addConflict(const ConflictInfo& conflict) override { return true; }
    std::vector<ConflictInfo> getUnresolvedConflicts() override { return {}; }
    std::vector<ConflictInfo> getConflictsForFile(const std::string& path) override { return {}; }
    bool markConflictResolved(int conflictId, int strategy = 0) override { return true; }
    std::pair<int, int> getConflictStats() override { return {0, 0}; }
    bool enqueueSyncOperation(const std::string& filePath, const std::string& opType, const std::string& status) override { return true; }
    bool logFileAccess(const std::string& filePath, const std::string& opType, const std::string& deviceId, long long timestamp) override { return true; }
    void* getDB() override { return nullptr; }
    
    // DatabaseManager support
    DatabaseManager* getDatabaseManager() override { return nullptr; }
    void setDatabaseManager(DatabaseManager* dbManager) override { (void)dbManager; }
    
    // New IStorageAPI methods - stub implementations
    bool addWatchedFolder(const std::string& path) override { return true; }
    bool removeWatchedFolder(const std::string& path) override { return true; }
    std::vector<WatchedFolder> getWatchedFolders() override { return {}; }
    bool isWatchedFolder(const std::string& path) override { return false; }
    bool updateWatchedFolderStatus(const std::string& path, int statusId) override { return true; }
    std::vector<FileMetadata> getFilesInFolder(const std::string& folderPath) override { return {}; }
    int removeFilesInFolder(const std::string& folderPath) override { return 0; }
    int getFileCount() override { return 0; }
    long long getTotalFileSize() override { return 0; }
    bool markFileSynced(const std::string& path, bool synced = true) override { return true; }
    std::vector<FileMetadata> getPendingFiles() override { return {}; }
    bool addIgnorePattern(const std::string& pattern) override { return true; }
    bool removeIgnorePattern(const std::string& pattern) override { return true; }
    std::vector<std::string> getIgnorePatterns() override { return {}; }
    bool addThreat(const ThreatInfo& threat) override { return true; }
    std::vector<ThreatInfo> getThreats() override { return {}; }
    bool removeThreat(int threatId) override { return true; }
    int removeThreatsInFolder(const std::string& folderPath) override { return 0; }
    bool markThreatSafe(int threatId, bool safe = true) override { return true; }
    std::vector<SyncQueueItem> getSyncQueue() override { return {}; }
    bool updateSyncQueueStatus(int itemId, const std::string& status) override { return true; }
    int clearCompletedSyncOperations() override { return 0; }
    std::vector<ActivityLogEntry> getRecentActivity(int limit = 50) override { return {}; }
    bool removeAllPeers() override { return true; }
    bool updatePeerStatus(const std::string& peerId, const std::string& status) override { return true; }
    bool blockPeer(const std::string& peerId) override { return true; }
    bool unblockPeer(const std::string& peerId) override { return true; }
    bool isPeerBlocked(const std::string& peerId) override { return false; }
    bool setConfig(const std::string& key, const std::string& value) override { return true; }
    std::optional<std::string> getConfig(const std::string& key) override { return std::nullopt; }
    bool removeConfig(const std::string& key) override { return true; }
    bool logTransfer(const std::string& filePath, const std::string& peerId, const std::string& direction, long long bytes, bool success) override { return true; }
    std::vector<std::pair<std::string, long long>> getTransferHistory(int limit = 50) override { return {}; }
};

class MockFile : public IFileAPI {
public:
    bool initialize(EventBus* eventBus) override { return true; }
    void shutdown() override {}
    std::string getName() const override { return "MockFile"; }
    std::string getVersion() const override { return "1.0.0"; }

    std::vector<uint8_t> readFile(const std::string& path) override { return {}; }
    void startWatching(const std::string& path) override {}
    void stopWatching(const std::string& path) override {}
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data) override { return true; }
};

}
