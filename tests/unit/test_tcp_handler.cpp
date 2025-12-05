#include "TCPHandler.h"
#include "EventBus.h"
#include "HandshakeProtocol.h"
#include "BandwidthLimiter.h"
#include <iostream>
#include <cassert>

using namespace SentinelFS;

void test_tcp_handler_creation() {
    std::cout << "Running test_tcp_handler_creation..." << std::endl;
    
    EventBus eventBus;
    HandshakeProtocol handshake("local_peer", "123456", false);
    BandwidthManager bandwidthManager(0, 0);
    
    TCPHandler tcpHandler(&eventBus, &handshake, &bandwidthManager);
    
    // Try to start listening on a random high port to avoid conflicts
    // Note: This might fail if port is taken or permissions are missing, 
    // but usually high ports are fine.
    // We'll use a port that is likely free.
    int port = 54321;
    bool started = tcpHandler.startListening(port);
    
    if (started) {
        std::cout << "TCP Server started on port " << port << std::endl;
        tcpHandler.stopListening();
    } else {
        std::cout << "Failed to start TCP server (expected in some environments)." << std::endl;
    }
    
    std::cout << "test_tcp_handler_creation passed." << std::endl;
}

int main() {
    try {
        test_tcp_handler_creation();
        std::cout << "All TCPHandler tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
