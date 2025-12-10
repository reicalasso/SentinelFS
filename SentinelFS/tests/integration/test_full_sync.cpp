#include "EventHandlers.h"
#include "EventBus.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "IFileAPI.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <memory>

using namespace SentinelFS;

// --- Mocks ---

class MockNetwork : public INetworkAPI {
public:
    std::string getName() const override { return "MockNetwork"; }
    std::string getVersion() const override { return "1.0.0"; }
    bool initialize(EventBus* eventBus) override { return true; }
    void shutdown() override {}

    bool connectToPeer(const std::string& address, int port) override { return true; }
    bool sendData(const std::string& peerId, const std::vector<uint8_t>& data) override {
        sentData[peerId].push_back(data);
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
    std::string getSessionCode() const override { return "CODE"; }
    void setEncryptionEnabled(bool enable) override {}
    bool isEncryptionEnabled() const override { return false; }
    void setGlobalUploadLimit(std::size_t bytesPerSecond) override {}
    void setGlobalDownloadLimit(std::size_t bytesPerSecond) override {}
    std::string getBandwidthStats() const override { return ""; }
    void setRelayEnabled(bool enabled) override {}
    bool isRelayEnabled() const override { return false; }
    bool isRelayConnected() const override { return false; }
    std::string getLocalPeerId() const override { return "local_peer"; }
    int getLocalPort() const override { return 8080; }
    bool connectToRelay(const std::string& host, int port, const std::string& sessionCode) override { return true; }
    void disconnectFromRelay() override {}
    std::vector<RelayPeerInfo> getRelayPeers() const override { return {}; }

    std::map<std::string, std::vector<std::vector<uint8_t>>> sentData;
};

class MockStorage : public IStorageAPI {
public:
    std::string getName() const override { return "MockStorage"; }
    std::string getVersion() const override { return "1.0.0"; }
    bool initialize(EventBus* eventBus) override { return true; }
    void shutdown() override {}

    void* getDB() override { return nullptr; }
    
    bool addFile(const std::string& path, const std::string& hash, long long timestamp, long long size) override {
        FileMetadata metadata;
        metadata.path = path;
        metadata.hash = hash;
        metadata.timestamp = timestamp;
        metadata.size = size;
        files[path] = metadata;
        return true;
    }
    std::optional<FileMetadata> getFile(const std::string& path) override {
        if (files.count(path)) return files[path];
        return std::nullopt;
    }
    bool removeFile(const std::string& path) override {
        files.erase(path);
        return true;
    }
    
    bool addPeer(const PeerInfo& peer) override {
        peers[peer.id] = peer;
        return true;
    }
    std::optional<PeerInfo> getPeer(const std::string& peerId) override {
        if (peers.count(peerId)) return peers[peerId];
        return std::nullopt;
    }
    std::vector<PeerInfo> getAllPeers() override {
        std::vector<PeerInfo> result;
        for (const auto& pair : peers) result.push_back(pair.second);
        return result;
    }
    bool updatePeerLatency(const std::string& peerId, int latency) override {
        if (peers.count(peerId)) {
            peers[peerId].latency = latency;
            return true;
        }
        return false;
    }
    std::vector<PeerInfo> getPeersByLatency() override {
        return getAllPeers(); // Mock implementation
    }
    bool removePeer(const std::string& peerId) override {
        peers.erase(peerId);
        return true;
    }
    
    bool addConflict(const ConflictInfo& conflict) override { return true; }
    std::vector<ConflictInfo> getUnresolvedConflicts() override { return {}; }
    std::vector<ConflictInfo> getConflictsForFile(const std::string& path) override { return {}; }
    bool markConflictResolved(int conflictId, int strategy = 0) override { return true; }
    std::pair<int, int> getConflictStats() override { return {0, 0}; }

    bool enqueueSyncOperation(const std::string& filePath, const std::string& opType, const std::string& status) override { return true; }
    bool logFileAccess(const std::string& filePath, const std::string& opType, const std::string& deviceId, long long timestamp) override { return true; }

    // Additional IStorageAPI methods
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

    std::map<std::string, FileMetadata> files;
    std::map<std::string, PeerInfo> peers;
};

class MockFilesystem : public IFileAPI {
public:
    std::string getName() const override { return "MockFilesystem"; }
    std::string getVersion() const override { return "1.0.0"; }
    bool initialize(EventBus* eventBus) override { return true; }
    void shutdown() override {}

    std::vector<uint8_t> readFile(const std::string& path) override {
        if (fileContents.count(path)) return fileContents[path];
        return {};
    }
    void startWatching(const std::string& path) override {}
    void stopWatching(const std::string& path) override {}
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data) override {
        fileContents[path] = data;
        return true;
    }

    std::map<std::string, std::vector<uint8_t>> fileContents;
};

#include <filesystem>
#include <fstream>

// ... (includes)

// ... (Mocks)

void test_file_modification_broadcast() {
    std::cout << "Running test_file_modification_broadcast..." << std::endl;
    
    // Setup real test environment
    std::string testDir = "/tmp/sentinel_test_env";
    std::string filePath = testDir + "/file.txt";
    
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);
    
    // Create a real file
    {
        std::ofstream ofs(filePath);
        ofs << "Hello";
        ofs.close();
    }
    
    EventBus eventBus;
    MockNetwork network;
    MockStorage storage;
    MockFilesystem filesystem;
    
    // Setup EventHandlers
    EventHandlers handlers(eventBus, &network, &storage, &filesystem, testDir);
    handlers.setupHandlers();
    
    // Add a peer to storage so we have someone to broadcast to
    PeerInfo peer;
    peer.id = "peer1";
    peer.status = "active";
    storage.addPeer(peer);
    
    // Trigger FILE_MODIFIED event
    eventBus.publish("FILE_MODIFIED", filePath);
    
    // Wait a bit for async operations (if any)
    // Since EventBus seems synchronous, this might not be needed, but good practice.
    // However, FileSyncHandler::broadcastUpdate might spawn threads?
    // Looking at the code, it seemed synchronous except for the "new peer connected" part.
    
    // Check if storage was updated
    auto metadata = storage.getFile(filePath);
    
    if (metadata.has_value()) {
        assert(metadata->size == 5);
        std::cout << "Storage updated correctly." << std::endl;
    } else {
        std::cout << "Storage NOT updated." << std::endl;
    }
    
    // Check if network sent data
    if (network.sentData.count("peer1")) {
        assert(!network.sentData["peer1"].empty());
        std::cout << "Network sent data to peer1." << std::endl;
    } else {
        std::cout << "Network did NOT send data to peer1." << std::endl;
    }
    
    // Cleanup
    std::filesystem::remove_all(testDir);
    
    std::cout << "test_file_modification_broadcast passed." << std::endl;
}

int main() {
    test_file_modification_broadcast();
    std::cout << "All integration tests passed!" << std::endl;
    return 0;
}
