#include "HandshakeProtocol.h"
#include <iostream>
#include <cassert>

using namespace SentinelFS;

void test_handshake_protocol_creation() {
    std::cout << "Running test_handshake_protocol_creation..." << std::endl;
    
    std::string localPeerId = "test_peer";
    std::string sessionCode = "123456";
    bool encryptionEnabled = false;
    
    HandshakeProtocol handshake(localPeerId, sessionCode, encryptionEnabled);
    
    // We can't easily test performClientHandshake without a socket.
    // But we verified the constructor works.
    
    std::cout << "test_handshake_protocol_creation passed." << std::endl;
}

int main() {
    try {
        test_handshake_protocol_creation();
        std::cout << "All HandshakeProtocol tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
