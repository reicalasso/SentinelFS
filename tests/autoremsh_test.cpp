#include "AutoRemeshManager.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace SentinelFS;

static bool containsId(const std::vector<std::string>& ids, const std::string& id) {
    for (const auto& v : ids) {
        if (v == id) {
            return true;
        }
    }
    return false;
}

void test_basic_selection() {
    AutoRemeshManager::Config cfg;
    cfg.maxActivePeers = 2;
    cfg.minSamplesForDecision = 3;
    AutoRemeshManager manager(cfg);

    // Good peers with low RTT and no loss
    for (int i = 0; i < 3; ++i) {
        manager.updateMeasurement("good1", 50, true);
        manager.updateMeasurement("good2", 60, true);
    }

    // Slow but stable peer
    for (int i = 0; i < 3; ++i) {
        manager.updateMeasurement("slow", 200, true);
    }

    // Unstable peer: some successes, many failures
    for (int i = 0; i < 3; ++i) {
        manager.updateMeasurement("unstable", 80, true);
    }
    for (int i = 0; i < 5; ++i) {
        manager.updateMeasurement("unstable", -1, false);
    }

    std::vector<AutoRemeshManager::PeerInfoSnapshot> peers = {
        {"good1", true},
        {"good2", true},
        {"slow",  true},
        {"unstable", true}
    };

    auto decision = manager.computeRemesh(peers);

    // We expect to keep good1 and good2 as active peers.
    assert(!containsId(decision.disconnectPeers, "good1"));
    assert(!containsId(decision.disconnectPeers, "good2"));
    assert(decision.disconnectPeers.size() == 2);
}

void test_insufficient_metrics_fallback() {
    AutoRemeshManager manager;

    // Fewer than minSamplesForDecision successful probes per peer
    manager.updateMeasurement("p1", 50, true);
    manager.updateMeasurement("p2", 70, true);

    std::vector<AutoRemeshManager::PeerInfoSnapshot> peers = {
        {"p1", true},
        {"p2", true}
    };

    auto decision = manager.computeRemesh(peers);

    // With insufficient metrics, manager should not decide to disconnect anyone.
    assert(decision.disconnectPeers.empty());
    assert(containsId(decision.connectPeers, "p1"));
    assert(containsId(decision.connectPeers, "p2"));
}

void test_packet_loss_estimation() {
    AutoRemeshManager manager;

    // One success, two failures -> ~66% loss
    manager.updateMeasurement("peer", 100, true);
    manager.updateMeasurement("peer", -1, false);
    manager.updateMeasurement("peer", -1, false);

    auto metrics = manager.snapshotMetrics();
    bool found = false;
    for (const auto& m : metrics) {
        if (m.peerId == "peer") {
            found = true;
            assert(m.totalProbes == 3);
            assert(m.successProbes == 1);
            assert(m.packetLossPercent > 60.0 && m.packetLossPercent < 70.0);
        }
    }
    assert(found);
}

int main() {
    test_basic_selection();
    test_insufficient_metrics_fallback();
    test_packet_loss_estimation();

    std::cout << "AutoRemeshManager tests passed" << std::endl;
    return 0;
}
