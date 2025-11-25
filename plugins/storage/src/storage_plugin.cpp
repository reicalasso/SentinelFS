#include "IStorageAPI.h"
#include "SQLiteHandler.h"
#include "FileMetadataManager.h"
#include "PeerManager.h"
#include "ConflictManager.h"
#include "DeviceManager.h"
#include "SessionManager.h"
#include "SyncQueueManager.h"
#include "FileAccessLogManager.h"
#include <iostream>
#include <memory>

namespace SentinelFS {

/**
 * @brief Storage plugin implementation using SQLite
 * 
 * Delegates responsibilities to specialized managers:
 * - SQLiteHandler: Database connection and schema
 * - FileMetadataManager: File CRUD operations
 * - PeerManager: Peer tracking and latency management
 * - ConflictManager: Conflict detection and resolution
 */
class StoragePlugin : public IStorageAPI {
public:
    StoragePlugin() 
        : sqliteHandler_(std::make_unique<SQLiteHandler>()),
          fileManager_(nullptr),
          peerManager_(nullptr),
          conflictManager_(nullptr),
          deviceManager_(nullptr),
          sessionManager_(nullptr),
          syncQueueManager_(nullptr),
          fileAccessLogManager_(nullptr) {}

    bool initialize(EventBus* eventBus) override {
        std::cout << "StoragePlugin initialized" << std::endl;
        
        if (!sqliteHandler_->initialize()) {
            return false;
        }

        // Initialize managers with database handle
        fileManager_ = std::make_unique<FileMetadataManager>(sqliteHandler_.get());
        peerManager_ = std::make_unique<PeerManager>(sqliteHandler_.get());
        conflictManager_ = std::make_unique<ConflictManager>(sqliteHandler_.get());
        deviceManager_ = std::make_unique<DeviceManager>(sqliteHandler_.get());
        sessionManager_ = std::make_unique<SessionManager>(sqliteHandler_.get());
        syncQueueManager_ = std::make_unique<SyncQueueManager>(sqliteHandler_.get());
        fileAccessLogManager_ = std::make_unique<FileAccessLogManager>(sqliteHandler_.get());
        
        return true;
    }

    void shutdown() override {
        std::cout << "StoragePlugin shutdown" << std::endl;
        sqliteHandler_->shutdown();
    }

    std::string getName() const override {
        return "StoragePlugin";
    }

    std::string getVersion() const override {
        return "1.0.0";
    }

    // --- File Operations (delegated to FileMetadataManager) ---

    bool addFile(const std::string& path, const std::string& hash, 
                 long long timestamp, long long size) override {
        return fileManager_->addFile(path, hash, timestamp, size);
    }

    std::optional<FileMetadata> getFile(const std::string& path) override {
        return fileManager_->getFile(path);
    }

    bool removeFile(const std::string& path) override {
        return fileManager_->removeFile(path);
    }

    // --- Peer Operations (delegated to PeerManager) ---

    bool addPeer(const PeerInfo& peer) override {
        return peerManager_->addPeer(peer);
    }

    std::optional<PeerInfo> getPeer(const std::string& peerId) override {
        return peerManager_->getPeer(peerId);
    }

    std::vector<PeerInfo> getAllPeers() override {
        return peerManager_->getAllPeers();
    }

    bool updatePeerLatency(const std::string& peerId, int latency) override {
        return peerManager_->updatePeerLatency(peerId, latency);
    }

    std::vector<PeerInfo> getPeersByLatency() override {
        return peerManager_->getPeersByLatency();
    }

    // --- Conflict Operations (delegated to ConflictManager) ---

    bool addConflict(const ConflictInfo& conflict) override {
        return conflictManager_->addConflict(conflict);
    }

    std::vector<ConflictInfo> getUnresolvedConflicts() override {
        return conflictManager_->getUnresolvedConflicts();
    }

    std::vector<ConflictInfo> getConflictsForFile(const std::string& path) override {
        return conflictManager_->getConflictsForFile(path);
    }

    bool markConflictResolved(int conflictId) override {
        return conflictManager_->markConflictResolved(conflictId);
    }

    std::pair<int, int> getConflictStats() override {
        return conflictManager_->getConflictStats();
    }

    // --- Sync Queue / Access Log helpers ---

    bool enqueueSyncOperation(const std::string& filePath,
                              const std::string& opType,
                              const std::string& status) override {
        return syncQueueManager_ ? syncQueueManager_->enqueue(filePath, opType, status) : false;
    }

    bool logFileAccess(const std::string& filePath,
                       const std::string& opType,
                       const std::string& deviceId,
                       long long timestamp) override {
        return fileAccessLogManager_ ? fileAccessLogManager_->logAccess(filePath, opType, deviceId, timestamp) : false;
    }
    
    void* getDB() override {
        return sqliteHandler_ ? sqliteHandler_->getDB() : nullptr;
    }

private:
    std::unique_ptr<SQLiteHandler> sqliteHandler_;
    std::unique_ptr<FileMetadataManager> fileManager_;
    std::unique_ptr<PeerManager> peerManager_;
    std::unique_ptr<ConflictManager> conflictManager_;
    std::unique_ptr<DeviceManager> deviceManager_;
    std::unique_ptr<SessionManager> sessionManager_;
    std::unique_ptr<SyncQueueManager> syncQueueManager_;
    std::unique_ptr<FileAccessLogManager> fileAccessLogManager_;
};

extern "C" {
    IPlugin* create_plugin() {
        return new StoragePlugin();
    }

    void destroy_plugin(IPlugin* plugin) {
        delete plugin;
    }
}

} // namespace SentinelFS
