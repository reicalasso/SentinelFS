#include "AutoRemeshManager.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>

using namespace SentinelFS;

void test_metrics_update() {
    std::cout << "Running test_metrics_update..." << std::endl;
    
    AutoRemeshManager manager;
    std::string peerId = "peer1";
    
    // 1. Success probe
    manager.updateMeasurement(peerId, 100, true);
    auto metrics = manager.snapshotMetrics();
    assert(metrics.size() == 1);
    assert(metrics[0].peerId == peerId);
    assert(metrics[0].lastRttMs == 100);
    assert(metrics[0].successProbes == 1);
    assert(metrics[0].totalProbes == 1);
    
    // 2. Fail probe
    manager.updateMeasurement(peerId, 0, false);
    metrics = manager.snapshotMetrics();
    assert(metrics[0].successProbes == 1);
    assert(metrics[0].totalProbes == 2);
    assert(metrics[0].packetLossPercent == 50.0);
    
    std::cout << "test_metrics_update passed." << std::endl;
}

void test_remesh_decision() {
    std::cout << "Running test_remesh_decision..." << std::endl;
    
    AutoRemeshManager::Config config;
    config.maxActivePeers = 2;
    config.minSamplesForDecision = 1;
    AutoRemeshManager manager(config);
    
    // Peer 1: Good connection
    manager.updateMeasurement("peer1", 10, true);
    manager.updateMeasurement("peer1", 10, true);
    
    // Peer 2: Bad connection (high RTT)
    manager.updateMeasurement("peer2", 500, true);
    manager.updateMeasurement("peer2", 500, true);
    
    // Peer 3: Good connection
    manager.updateMeasurement("peer3", 20, true);
    manager.updateMeasurement("peer3", 20, true);
    
    // Current state: peer2 is connected, peer1 and peer3 are not.
    // We expect peer2 to be disconnected, and peer1 and peer3 to be connected.
    std::vector<AutoRemeshManager::PeerInfoSnapshot> peers = {
        {"peer1", false},
        {"peer2", true},
        {"peer3", false}
    };
    
    auto decision = manager.computeRemesh(peers);
    
    // Check connectPeers
    // Should contain peer1 and peer3
    bool hasPeer1 = false;
    bool hasPeer3 = false;
    for (const auto& p : decision.connectPeers) {
        if (p == "peer1") hasPeer1 = true;
        if (p == "peer3") hasPeer3 = true;
    }
    assert(hasPeer1);
    assert(hasPeer3);
    
    // Check disconnectPeers
    // Should contain peer2 because maxActivePeers is 2, and we have 2 better peers.
    bool hasPeer2 = false;
    for (const auto& p : decision.disconnectPeers) {
        if (p == "peer2") hasPeer2 = true;
    }
    assert(hasPeer2);
    
    std::cout << "test_remesh_decision passed." << std::endl;
}

int main() {
    try {
        test_metrics_update();
        test_remesh_decision();
        std::cout << "All AutoRemeshManager tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
