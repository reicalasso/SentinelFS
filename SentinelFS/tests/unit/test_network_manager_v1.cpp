#include "NetworkManager.h"
#include "INetworkAPI.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <map>

using namespace SentinelFS;

// Mock implementation of INetworkAPI for testing
class MockNetworkPlugin : public INetworkAPI {
public:
    // IPlugin methods
    std::string getName() const override { return "MockNetwork"; }
    std::string getVersion() const override { return "1.0.0"; }
    void initialize(const std::unordered_map<std::string, std::string>& config) override {}
    void shutdown() override {}

    // INetworkAPI methods
    bool connectToPeer(const std::string& address, int port) override {
        // Simulate successful connection
        return true;
    }

    bool sendData(const std::string& peerId, const std::vector<uint8_t>& data) override {
        sentData[peerId] = data;
        return true;
    }

    void startListening(int port) override {}
    void startDiscovery(int port) override {}
    void broadcastPresence(int discoveryPort, int tcpPort) override {}
    
    // Missing methods from interface (assuming they exist based on usage)
    // The interface snippet was cut off, but usually there are more methods.
    // I'll add stubs for common ones if needed, or just implement pure virtuals.
    // Let's assume the ones above are enough for NetworkManager to work, 
    // or I'll get a compile error and fix it.
    
    // Based on NetworkManager.h, it calls connectToPeer and sendData.
    // It might call others.
    
    // Let's add a few more likely candidates just in case
    void disconnectPeer(const std::string& peerId) override {} // Hypothetical
    bool isPeerConnected(const std::string& peerId) override { return true; } // Hypothetical

    std::map<std::string, std::vector<uint8_t>> sentData;
};

// Since I don't know the exact full interface of INetworkAPI, 
// I'll try to compile this. If it fails due to missing pure virtuals, 
// I'll read the rest of INetworkAPI.h.

// Actually, to be safe and save turns, let's read the rest of INetworkAPI.h first.
// But I'll write the test logic assuming the mock works.

void test_connection_management() {
    std::cout << "Running test_connection_management..." << std::endl;
    
    MockNetworkPlugin mockPlugin;
    NetworkManager manager(&mockPlugin);
    
    bool connected = manager.connect("peer1", "127.0.0.1", 8080);
    assert(connected);
    assert(manager.isConnected("peer1"));
    
    auto snapshot = manager.snapshot();
    assert(snapshot.size() == 1);
    assert(snapshot[0].peerId == "peer1");
    assert(snapshot[0].address == "127.0.0.1");
    
    manager.disconnect("peer1");
    assert(!manager.isConnected("peer1"));
    
    std::cout << "test_connection_management passed." << std::endl;
}

void test_send_data() {
    std::cout << "Running test_send_data..." << std::endl;
    
    MockNetworkPlugin mockPlugin;
    NetworkManager manager(&mockPlugin);
    
    manager.connect("peer1", "127.0.0.1", 8080);
    
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    bool sent = manager.send("peer1", data);
    assert(sent);
    
    assert(mockPlugin.sentData["peer1"] == data);
    
    std::cout << "test_send_data passed." << std::endl;
}

int main() {
    // This test file relies on MockNetworkPlugin implementing all pure virtuals.
    // I'll need to ensure that.
    return 0;
}
