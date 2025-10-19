#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <queue>
#include <limits>
#include "../models.hpp"

struct NetworkNode {
    std::string id;
    double latency;           // in ms
    double bandwidth;         // in Mbps
    bool active;
    std::vector<std::string> connections;  // connected peers
    
    NetworkNode() : latency(0.0), bandwidth(0.0), active(true) {}
    NetworkNode(const std::string& id) : id(id), latency(0.0), bandwidth(0.0), active(true) {}
};

struct NetworkEdge {
    std::string node1;
    std::string node2;
    double weight;  // Could be based on latency, bandwidth, or combined metric
    
    NetworkEdge(const std::string& n1, const std::string& n2, double w) 
        : node1(n1), node2(n2), weight(w) {}
};

class Remesh {
public:
    Remesh();
    ~Remesh();

    void start();
    void stop();
    
    // Evaluate current network topology and optimize
    void evaluateAndOptimize();
    
    // Update peer information
    void updatePeerLatency(const std::string& peerId, double latency);
    void updatePeerBandwidth(const std::string& peerId, double bandwidth);
    void addPeer(const std::string& peerId);
    void removePeer(const std::string& peerId);
    
    // Get optimal peer connections
    std::vector<std::string> getOptimalConnections();
    std::vector<std::pair<std::string, std::string>> getOptimalTopology();  // connections as pairs
    
    // Check if remesh is needed
    bool needsRemesh() const;
    
    void setRemeshThreshold(double thresholdMs);
    void setBandwidthWeight(double bwWeight) { bandwidthWeight = bwWeight; }
    void setLatencyWeight(double latWeight) { latencyWeight = latWeight; }
    
    // Topology algorithms
    std::vector<std::pair<std::string, std::string>> calculateMinimumSpanningTree();
    std::vector<std::string> calculateLoadBalancedConnections();
    
    // Network metrics
    double calculateNetworkEfficiency() const;
    std::string getNetworkDiameter() const;
    
private:
    mutable std::mutex topologyMutex;
    std::atomic<bool> running{false};
    std::thread remeshThread;
    
    std::map<std::string, NetworkNode> networkNodes;
    double remeshThresholdMs;
    double bandwidthWeight;  // Weight for bandwidth in combined metric
    double latencyWeight;    // Weight for latency in combined metric
    
    void remeshLoop();
    std::vector<std::string> calculateOptimalTopology();
    void measureLatencies();
    void measureBandwidth();
    
    // Topology calculation methods
    std::vector<std::pair<std::string, std::string>> calculateSimpleTopology();
};