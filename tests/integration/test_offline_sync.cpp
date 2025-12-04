#include "EventHandlers.h"
#include "EventBus.h"
#include "common_mocks.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace SentinelFS;

void test_offline_queue_flow() {
    std::cout << "Running test_offline_queue_flow..." << std::endl;
    
    // Setup test directory
    std::string testDir = "/tmp/test_offline_sync";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);
    
    EventBus eventBus;
    MockNetwork network;
    MockStorage storage;
    MockFilesystem filesystem;
    
    // Setup EventHandlers
    EventHandlers handlers(eventBus, &network, &storage, &filesystem, testDir);
    handlers.setupHandlers();
    
    // Add a peer
    PeerInfo peer;
    peer.id = "peer1";
    peer.status = "active";
    storage.addPeer(peer);
    
    // 1. Disable Sync
    handlers.setSyncEnabled(false);
    std::cout << "Sync disabled." << std::endl;
    
    // 2. Create a file and trigger modification
    std::string filePath = testDir + "/offline_file.txt";
    {
        std::ofstream ofs(filePath);
        ofs << "Offline Content";
        ofs.close();
    }
    // We need to add it to storage manually because FileSyncHandler::scanDirectory isn't running continuously
    // But FileSyncHandler::handleFileModified calls updateFileInDatabase which adds it.
    // So we just trigger the event.
    
    eventBus.publish("FILE_MODIFIED", filePath);
    
    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify NO network activity
    assert(network.sentData.empty());
    std::cout << "Verified no network activity while paused." << std::endl;
    
    // 3. Enable Sync
    handlers.setSyncEnabled(true);
    std::cout << "Sync enabled." << std::endl;
    
    // Wait for pending changes to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Verify network activity
    assert(network.sentData.count("peer1"));
    assert(!network.sentData["peer1"].empty());
    
    // Verify content
    bool foundUpdate = false;
    for (const auto& packet : network.sentData["peer1"]) {
        std::string msg(packet.begin(), packet.end());
        if (msg.find("UPDATE_AVAILABLE|") == 0) {
            foundUpdate = true;
            break;
        }
    }
    assert(foundUpdate);
    std::cout << "Verified pending changes broadcasted after resume." << std::endl;
    
    // Cleanup
    std::filesystem::remove_all(testDir);
    
    std::cout << "test_offline_queue_flow passed." << std::endl;
}

int main() {
    test_offline_queue_flow();
    std::cout << "All offline sync tests passed!" << std::endl;
    return 0;
}
