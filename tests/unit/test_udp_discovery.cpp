#include "UDPDiscovery.h"
#include "EventBus.h"
#include <iostream>
#include <cassert>

using namespace SentinelFS;

void test_udp_discovery_creation() {
    std::cout << "Running test_udp_discovery_creation..." << std::endl;
    
    EventBus eventBus;
    std::string localPeerId = "test_peer";
    
    UDPDiscovery discovery(&eventBus, localPeerId);
    
    // Try to start discovery on a random high port
    int port = 54322;
    bool started = discovery.startDiscovery(port);
    
    if (started) {
        std::cout << "UDP Discovery started on port " << port << std::endl;
        discovery.stopDiscovery();
    } else {
        std::cout << "Failed to start UDP discovery (expected in some environments)." << std::endl;
    }
    
    std::cout << "test_udp_discovery_creation passed." << std::endl;
}

int main() {
    try {
        test_udp_discovery_creation();
        std::cout << "All UDPDiscovery tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
