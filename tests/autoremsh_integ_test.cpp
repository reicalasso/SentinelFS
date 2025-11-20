#include "AutoRemeshManager.h"

#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace SentinelFS;

struct FakePeerState {
    std::string id;
    bool connected{false};
};

struct RttSample {
    std::string id;
    int rttMs;
    bool success;
};

static std::vector<AutoRemeshManager::PeerInfoSnapshot> makeSnapshots(const std::map<std::string, FakePeerState>& peers) {
    std::vector<AutoRemeshManager::PeerInfoSnapshot> out;
    out.reserve(peers.size());
    for (const auto& kv : peers) {
        AutoRemeshManager::PeerInfoSnapshot s;
        s.peerId = kv.first;
        s.isConnected = kv.second.connected;
        out.push_back(s);
    }
    return out;
}

void test_remesh_with_changing_rtt() {
    AutoRemeshManager::Config cfg;
    cfg.maxActivePeers = 2;
    cfg.minSamplesForDecision = 3;
    AutoRemeshManager manager(cfg);

    // Four peers: two consistently fast, two consistently slow
    std::map<std::string, FakePeerState> peers;
    peers["fast1"] = {"fast1", true};
    peers["fast2"] = {"fast2", true};
    peers["slow1"] = {"slow1", true};
    peers["slow2"] = {"slow2", true};

    // Round-based RTT samples
    std::vector<std::vector<RttSample>> rounds = {
        {
            {"fast1", 40, true},
            {"fast2", 50, true},
            {"slow1", 200, true},
            {"slow2", 220, true},
        },
        {
            {"fast1", 45, true},
            {"fast2", 55, true},
            {"slow1", 210, true},
            {"slow2", 230, true},
        },
        {
            {"fast1", 50, true},
            {"fast2", 60, true},
            {"slow1", 220, true},
            {"slow2", 240, true},
        },
    };

    for (std::size_t round = 0; round < rounds.size(); ++round) {
        const auto& samples = rounds[round];
        for (const auto& s : samples) {
            manager.updateMeasurement(s.id, s.rttMs, s.success);
        }

        auto snapshots = makeSnapshots(peers);
        auto decision = manager.computeRemesh(snapshots);

        // Apply decisions to fake connection state
        for (const auto& id : decision.disconnectPeers) {
            auto it = peers.find(id);
            if (it != peers.end()) {
                it->second.connected = false;
            }
        }
        for (const auto& id : decision.connectPeers) {
            auto it = peers.find(id);
            if (it != peers.end()) {
                it->second.connected = true;
            }
        }
    }

    // After several rounds, only the two fast peers should remain connected
    int connectedCount = 0;
    for (const auto& kv : peers) {
        if (kv.second.connected) {
            ++connectedCount;
            if (kv.first == "slow1" || kv.first == "slow2") {
                assert(false && "Slow peers should have been disconnected by auto-remesh");
            }
        }
    }
    assert(connectedCount == 2);
}

int main() {
    test_remesh_with_changing_rtt();
    std::cout << "AutoRemesh integration test passed" << std::endl;
    return 0;
}
