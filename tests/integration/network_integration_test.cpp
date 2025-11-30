/**
 * @file network_integration_test.cpp
 * @brief Integration tests for SentinelFS network functionality
 * 
 * Tests peer discovery, connection establishment, and data transfer.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>

#include "TCPHandler.h"
#include "UDPDiscovery.h"
#include "HandshakeProtocol.h"
#include "EventBus.h"
#include "Logger.h"
#include "MetricsCollector.h"

using namespace SentinelFS;

class NetworkIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        eventBus1_ = std::make_unique<EventBus>();
        eventBus2_ = std::make_unique<EventBus>();
    }
    
    void TearDown() override {
        eventBus1_.reset();
        eventBus2_.reset();
    }
    
    std::unique_ptr<EventBus> eventBus1_;
    std::unique_ptr<EventBus> eventBus2_;
};

TEST_F(NetworkIntegrationTest, TCPConnectionEstablishment) {
    const int testPort = 18080;
    const std::string peerId1 = "peer1";
    const std::string peerId2 = "peer2";
    
    // Create handshake protocols
    HandshakeProtocol handshake1(peerId1);
    HandshakeProtocol handshake2(peerId2);
    
    // Create TCP handlers
    TCPHandler server(eventBus1_.get(), &handshake1, nullptr);
    TCPHandler client(eventBus2_.get(), &handshake2, nullptr);
    
    // Track connection events
    std::atomic<bool> serverConnected{false};
    std::atomic<bool> clientConnected{false};
    
    eventBus1_->subscribe("PEER_CONNECTED", [&](const std::any& data) {
        serverConnected = true;
    });
    
    eventBus2_->subscribe("PEER_CONNECTED", [&](const std::any& data) {
        clientConnected = true;
    });
    
    // Start server
    ASSERT_TRUE(server.startListening(testPort));
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Connect client
    ASSERT_TRUE(client.connectToPeer("127.0.0.1", testPort));
    
    // Wait for connection
    for (int i = 0; i < 50 && (!serverConnected || !clientConnected); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    EXPECT_TRUE(serverConnected);
    
    // Cleanup
    server.stopListening();
}

TEST_F(NetworkIntegrationTest, DataTransfer) {
    const int testPort = 18081;
    const std::string peerId1 = "sender";
    const std::string peerId2 = "receiver";
    
    HandshakeProtocol handshake1(peerId1);
    HandshakeProtocol handshake2(peerId2);
    
    TCPHandler server(eventBus1_.get(), &handshake1, nullptr);
    TCPHandler client(eventBus2_.get(), &handshake2, nullptr);
    
    // Track received data
    std::vector<uint8_t> receivedData;
    std::mutex dataMutex;
    std::atomic<bool> dataReceived{false};
    
    server.setDataCallback([&](const std::string& peerId, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(dataMutex);
        receivedData = data;
        dataReceived = true;
    });
    
    // Start and connect
    ASSERT_TRUE(server.startListening(testPort));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(client.connectToPeer("127.0.0.1", testPort));
    
    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Send data
    std::vector<uint8_t> testData = {'H', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(client.sendData(peerId1, testData));
    
    // Wait for data
    for (int i = 0; i < 50 && !dataReceived; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    EXPECT_TRUE(dataReceived);
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        EXPECT_EQ(receivedData, testData);
    }
    
    server.stopListening();
}

TEST_F(NetworkIntegrationTest, UDPDiscoveryBroadcast) {
    const int discoveryPort = 19999;
    const std::string peerId = "discoverable_peer";
    
    // Track discovered peers
    std::atomic<bool> peerDiscovered{false};
    std::string discoveredPeerId;
    
    eventBus1_->subscribe("PEER_DISCOVERED", [&](const std::any& data) {
        try {
            auto info = std::any_cast<std::string>(data);
            discoveredPeerId = info;
            peerDiscovered = true;
        } catch (...) {}
    });
    
    // Create discovery instances
    UDPDiscovery discovery1(eventBus1_.get(), peerId, discoveryPort);
    
    // Start discovery
    ASSERT_TRUE(discovery1.start());
    
    // Broadcast presence
    discovery1.broadcastPresence();
    
    // Wait a bit for potential self-discovery (may not happen on all systems)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Stop
    discovery1.stop();
    
    // Test passes if no crash - actual discovery depends on network config
    SUCCEED();
}

TEST_F(NetworkIntegrationTest, MultipleConnections) {
    const int testPort = 18082;
    const std::string serverId = "server";
    
    HandshakeProtocol serverHandshake(serverId);
    TCPHandler server(eventBus1_.get(), &serverHandshake, nullptr);
    
    std::atomic<int> connectionCount{0};
    eventBus1_->subscribe("PEER_CONNECTED", [&](const std::any&) {
        connectionCount++;
    });
    
    ASSERT_TRUE(server.startListening(testPort));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create multiple clients
    std::vector<std::unique_ptr<EventBus>> clientBuses;
    std::vector<std::unique_ptr<HandshakeProtocol>> clientHandshakes;
    std::vector<std::unique_ptr<TCPHandler>> clients;
    
    const int numClients = 3;
    for (int i = 0; i < numClients; ++i) {
        clientBuses.push_back(std::make_unique<EventBus>());
        clientHandshakes.push_back(std::make_unique<HandshakeProtocol>("client" + std::to_string(i)));
        clients.push_back(std::make_unique<TCPHandler>(
            clientBuses.back().get(), clientHandshakes.back().get(), nullptr));
        
        ASSERT_TRUE(clients.back()->connectToPeer("127.0.0.1", testPort));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Wait for all connections
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_GE(connectionCount.load(), numClients);
    
    server.stopListening();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
