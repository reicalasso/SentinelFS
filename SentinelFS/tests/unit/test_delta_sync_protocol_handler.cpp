#include "DeltaSyncProtocolHandler.h"
#include "MockPlugins.h"
#include <iostream>
#include <cassert>

using namespace SentinelFS;

void test_delta_sync_protocol_handler_creation() {
    std::cout << "Running test_delta_sync_protocol_handler_creation..." << std::endl;
    
    MockNetwork network;
    MockStorage storage;
    MockFile file;
    std::string watchDir = "/tmp/test_watch";
    
    DeltaSyncProtocolHandler handler(&network, &storage, &file, watchDir);
    
    // Just testing instantiation for now as logic is complex to test without full protocol simulation
    // But we can test if it compiles and links.
    
    std::cout << "test_delta_sync_protocol_handler_creation passed." << std::endl;
}

int main() {
    try {
        test_delta_sync_protocol_handler_creation();
        std::cout << "All DeltaSyncProtocolHandler tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
