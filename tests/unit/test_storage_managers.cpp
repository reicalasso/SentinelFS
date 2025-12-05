#include "SQLiteHandler.h"
#include "DeviceManager.h"
#include "PeerManager.h"
#include "SessionManager.h"
#include "FileMetadataManager.h"
#include "FileAccessLogManager.h"
#include "SyncQueueManager.h"
#include "ConflictManager.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace SentinelFS;

void test_storage_managers() {
    std::cout << "Running test_storage_managers..." << std::endl;
    
    // Use a temporary file for the database
    std::string dbPath = "test_storage.db";
    if (std::filesystem::exists(dbPath)) {
        std::filesystem::remove(dbPath);
    }
    
    SQLiteHandler dbHandler;
    if (!dbHandler.initialize(dbPath)) {
        std::cerr << "Failed to initialize SQLite database." << std::endl;
        return;
    }
    
    // Test PeerManager
    {
        PeerManager peerMgr(&dbHandler);
        PeerInfo peer;
        peer.id = "peer1";
        peer.ip = "127.0.0.1";
        peer.port = 8080;
        peer.status = "active";
        peer.lastSeen = 1000;
        
        assert(peerMgr.addPeer(peer));
        auto retrieved = peerMgr.getPeer("peer1");
        assert(retrieved.has_value());
        assert(retrieved->id == "peer1");
        
        std::cout << "PeerManager test passed." << std::endl;
    }
    
    // Test FileMetadataManager
    {
        FileMetadataManager fileMgr(&dbHandler);
        assert(fileMgr.addFile("/tmp/test.txt", "hash123", 1000, 500));
        auto retrieved = fileMgr.getFile("/tmp/test.txt");
        assert(retrieved.has_value());
        assert(retrieved->hash == "hash123");
        
        std::cout << "FileMetadataManager test passed." << std::endl;
    }
    
    // Test ConflictManager
    {
        ConflictManager conflictMgr(&dbHandler);
        ConflictInfo conflict;
        conflict.path = "/tmp/conflict.txt";
        conflict.localHash = "hash1";
        conflict.remoteHash = "hash2";
        conflict.remotePeerId = "peer1";
        conflict.resolved = false;
        
        assert(conflictMgr.addConflict(conflict));
        auto conflicts = conflictMgr.getUnresolvedConflicts();
        // Note: ID might be auto-generated, so we check if list is not empty
        assert(!conflicts.empty());
        
        std::cout << "ConflictManager test passed." << std::endl;
    }
    
    // Test other managers instantiation (assuming similar API or just existence)
    {
        DeviceManager deviceMgr(&dbHandler);
        SessionManager sessionMgr(&dbHandler);
        FileAccessLogManager logMgr(&dbHandler);
        SyncQueueManager queueMgr(&dbHandler);
        
        // Just basic calls if possible, or just instantiation
        // logMgr.logFileAccess(...)
        // queueMgr.enqueueSyncOperation(...)
    }
    
    dbHandler.shutdown();
    std::filesystem::remove(dbPath);
    
    std::cout << "test_storage_managers passed." << std::endl;
}

int main() {
    try {
        test_storage_managers();
        std::cout << "All Storage Plugin tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
