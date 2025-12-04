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

void test_peer_discovery_and_connection() {
    std::cout << "Running test_peer_discovery_and_connection..." << std::endl;
    
    // Setup test directory
    std::string testDir = "/tmp/test_peer_lifecycle";
    std::filesystem::remove_all(testDir);
    std::filesystem::create_directories(testDir);
    
    EventBus eventBus;
    MockNetwork network;
    MockStorage storage;
    MockFilesystem filesystem;
    
    // Setup EventHandlers
    EventHandlers handlers(eventBus, &network, &storage, &filesystem, testDir);
    handlers.setupHandlers();
    
    // 1. Simulate PEER_DISCOVERED
    // Format: SENTINEL_DISCOVERY|PEER_ID|TCP_PORT|SENDER_IP
    std::string discoveryMsg = "SENTINEL_DISCOVERY|peer_remote|8080|192.168.1.100";
    eventBus.publish("PEER_DISCOVERED", discoveryMsg);
    
    // Verify peer added to storage
    auto peer = storage.getPeer("peer_remote");
    assert(peer.has_value());
    assert(peer->id == "peer_remote");
    assert(peer->ip == "192.168.1.100");
    assert(peer->port == 8080);
    std::cout << "Peer discovery verified." << std::endl;
    
    // 2. Simulate PEER_CONNECTED
    // This should trigger a broadcast of all files to the new peer
    
    // First, add a file to storage AND create it on disk so FileSyncHandler finds it
    std::string filePath = testDir + "/file1.txt";
    {
        std::ofstream ofs(filePath);
        ofs << "content";
        ofs.close();
    }
    storage.addFile(filePath, "hash1", 1000, 7);
    
    eventBus.publish("PEER_CONNECTED", std::string("peer_remote"));
    
    // Wait for async broadcast (EventHandlers spawns a thread with 500ms delay)
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Verify network sent data
    assert(network.sentData.count("peer_remote"));
    assert(!network.sentData["peer_remote"].empty());
    
    // Verify the content of the sent data (should be UPDATE_AVAILABLE)
    bool foundUpdate = false;
    for (const auto& packet : network.sentData["peer_remote"]) {
        std::string msg(packet.begin(), packet.end());
        if (msg.find("UPDATE_AVAILABLE|") == 0) {
            foundUpdate = true;
            break;
        }
    }
    assert(foundUpdate);
    std::cout << "Peer connection sync trigger verified." << std::endl;
    
    // Cleanup
    std::filesystem::remove_all(testDir);
    
    std::cout << "test_peer_discovery_and_connection passed." << std::endl;
}

int main() {
    test_peer_discovery_and_connection();
    std::cout << "All peer lifecycle tests passed!" << std::endl;
    return 0;
}
