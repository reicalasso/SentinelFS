#include "remesh.hpp"
#include <algorithm>
#include <random>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

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
    // For this implementation, we'll measure actual latencies to peers
    // and recalculate the optimal topology
    std::lock_guard<std::mutex> lock(topologyMutex);
    
    // Calculate optimal topology based on current peer latencies
    auto optimalConnections = calculateOptimalTopology();
    
    // In a real implementation, we would reconfigure network connections here
    // based on the optimal topology
    std::cout << "Remesh evaluation complete. Optimal connections: " << optimalConnections.size() << " peers" << std::endl;
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
    
    // Check if we have at least some peer information
    if (peerLatencies.empty()) {
        return false; // No peers to remesh for
    }
    
    // Check if any peer has latency above the threshold
    for (const auto& pair : peerLatencies) {
        if (pair.second > remeshThresholdMs) {
            return true;
        }
    }
    
    // Check if we have peers but some are disconnected
    return false; // For now, just return false - in a real system we'd need more complex logic
}

void Remesh::setRemeshThreshold(double thresholdMs) {
    remeshThresholdMs = thresholdMs;
}

void Remesh::remeshLoop() {
    while (running) {
        // Ping known peers to measure current latency
        measureLatencies();
        
        if (needsRemesh()) {
            evaluateAndOptimize();
        }
        
        // Wait for a while before checking again
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

std::vector<std::string> Remesh::calculateOptimalTopology() {
    std::vector<std::string> optimalPeers;
    
    // Create a vector of pairs (latency, peerId) for sorting
    std::vector<std::pair<double, std::string>> latencyPairs;
    for (const auto& pair : peerLatencies) {
        if (pair.second > 0 && pair.second < 10000) { // Filter out invalid measurements (0) and extremely high ones
            latencyPairs.push_back({pair.second, pair.first});
        }
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

void Remesh::measureLatencies() {
    // This would ping each peer to measure actual network latency
    // For now, we'll implement a basic ping mechanism using UDP
    
    // Create a socket for ping operations
    int pingSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (pingSocket < 0) {
        std::cerr << "Error creating ping socket" << std::endl;
        return;
    }
    
    // This is a simplified latency measurement approach
    // In a real implementation, we might use ICMP ping or send timestamped packets
    
    // For now, just update the latency measurements with dummy data, 
    // but in a real system, you would ping each peer and measure the round-trip time
    std::lock_guard<std::mutex> lock(topologyMutex);
    
    // Simulate measuring latencies by adding some random variation
    for (auto& pair : peerLatencies) {
        // Add some random fluctuation to simulate real network behavior
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(1.0, 10.0); // 1-10ms random fluctuation
        
        // Keep the latency measurement within reasonable bounds
        if (pair.second > 0) {
            pair.second += dis(gen);
            if (pair.second > 1000) pair.second = 1000; // Cap at 1000ms
        } else {
            pair.second = 50.0; // Default to 50ms if not set
        }
    }
    
    close(pingSocket);
}