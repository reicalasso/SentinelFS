#include "EventHandlers.h"
#include "EventBus.h"
#include "common_mocks.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

using namespace SentinelFS;

void test_remote_update_flow() {
    std::cout << "Running test_remote_update_flow..." << std::endl;
    
    EventBus eventBus;
    MockNetwork network;
    MockStorage storage;
    MockFilesystem filesystem;
    
    // Setup EventHandlers
    EventHandlers handlers(eventBus, &network, &storage, &filesystem, "/tmp/test");
    handlers.setupHandlers();
    
    std::string peerId = "peer_remote";
    std::string filePath = "/tmp/test/remote_file.txt";
    std::string fileHash = "remote_hash";
    long long fileSize = 100;
    
    // 1. Simulate receiving UPDATE_AVAILABLE
    // Format: UPDATE_AVAILABLE|path|hash|size
    std::string updateMsg = "UPDATE_AVAILABLE|remote_file.txt|" + fileHash + "|" + std::to_string(fileSize);
    std::vector<uint8_t> updatePayload(updateMsg.begin(), updateMsg.end());
    
    eventBus.publish("DATA_RECEIVED", std::make_pair(peerId, updatePayload));
    
    // Verify that we sent a REQUEST_DELTA (even if file missing, we send empty signature)
    // DeltaSyncProtocolHandler sends REQUEST_DELTA|path|signature
    
    assert(network.sentData.count(peerId));
    bool sentRequest = false;
    for (const auto& packet : network.sentData[peerId]) {
        std::string msg(packet.begin(), packet.end());
        if (msg.find("REQUEST_DELTA|remote_file.txt") == 0) {
            sentRequest = true;
            break;
        }
    }
    assert(sentRequest);
    std::cout << "Update handling (REQUEST_DELTA) verified." << std::endl;
    
    // 2. Simulate receiving FILE_DATA
    // Format: FILE_DATA|relativePath|chunkId/total|data
    std::string content = "Remote Content";
    std::string dataMsg = "FILE_DATA|remote_file.txt|0/1|" + content;
    std::vector<uint8_t> dataPayload(dataMsg.begin(), dataMsg.end());
    
    // Clear previous sent data to avoid confusion
    network.sentData.clear();
    
    eventBus.publish("DATA_RECEIVED", std::make_pair(peerId, dataPayload));
    
    // Verify file was written to filesystem
    // Note: DeltaSyncProtocolHandler constructs full path using watchDirectory
    std::string fullPath = "/tmp/test/remote_file.txt";
    assert(filesystem.fileContents.count(fullPath));
    
    std::string writtenContent(filesystem.fileContents[fullPath].begin(), filesystem.fileContents[fullPath].end());
    assert(writtenContent == content);
    std::cout << "Data handling (FILE_DATA) verified." << std::endl;
    
    // Note: DeltaSyncProtocolHandler writes to filesystem but relies on EventHandlers/Watcher to update DB.
    // However, it also adds the file to an ignore list to prevent echo-broadcast.
    // In this test environment, we don't have a real watcher triggering FILE_MODIFIED,
    // and even if we did, EventHandlers might ignore it due to the patch list.
    // So we only verify the file was written to disk.
    
    std::cout << "test_remote_update_flow passed." << std::endl;
}

int main() {
    test_remote_update_flow();
    std::cout << "All remote sync tests passed!" << std::endl;
    return 0;
}
