#include "remesh.hpp"
#include <algorithm>
#include <random>

Remesh::Remesh() : remeshThresholdMs(100.0) {}

Remesh::~Remesh() {
    stop();
}

void Remesh::start() {
    running = true;
    remeshThread = std::thread(&Remesh::remeshLoop, this);
}

void Remesh::stop() {
    running = false;
    if (remeshThread.joinable()) {
        remeshThread.join();
    }
}

void Remesh::evaluateAndOptimize() {
    // For this basic implementation, just trigger the optimization
    std::lock_guard<std::mutex> lock(topologyMutex);
    
    // Calculate optimal topology based on current peer latencies
    auto optimalConnections = calculateOptimalTopology();
    
    // In a real implementation, we would reconfigure network connections here
    // based on the optimal topology
}

void Remesh::updatePeerLatency(const std::string& peerId, double latency) {
    std::lock_guard<std::mutex> lock(topologyMutex);
    peerLatencies[peerId] = latency;
}

std::vector<std::string> Remesh::getOptimalConnections() {
    std::lock_guard<std::mutex> lock(topologyMutex);
    return calculateOptimalTopology();
}

bool Remesh::needsRemesh() const {
    std::lock_guard<std::mutex> lock(topologyMutex);
    
    // Check if any peer has latency above the threshold
    for (const auto& pair : peerLatencies) {
        if (pair.second > remeshThresholdMs) {
            return true;
        }
    }
    return false;
}

void Remesh::setRemeshThreshold(double thresholdMs) {
    remeshThresholdMs = thresholdMs;
}

void Remesh::remeshLoop() {
    while (running) {
        // Wait for a while before checking again
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        if (needsRemesh()) {
            evaluateAndOptimize();
        }
    }
}

std::vector<std::string> Remesh::calculateOptimalTopology() {
    std::vector<std::string> optimalPeers;
    
    // Create a vector of pairs (latency, peerId) for sorting
    std::vector<std::pair<double, std::string>> latencyPairs;
    for (const auto& pair : peerLatencies) {
        latencyPairs.push_back({pair.second, pair.first});
    }
    
    // Sort by latency (lowest first)
    std::sort(latencyPairs.begin(), latencyPairs.end());
    
    // Select the peers with the lowest latency (up to a reasonable number)
    size_t numPeersToSelect = std::min(latencyPairs.size(), static_cast<size_t>(5)); // Max 5 peers
    for (size_t i = 0; i < numPeersToSelect && i < latencyPairs.size(); ++i) {
        optimalPeers.push_back(latencyPairs[i].second);
    }
    
    return optimalPeers;
}