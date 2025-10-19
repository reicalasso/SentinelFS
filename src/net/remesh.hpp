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
#include "../models.hpp"

class Remesh {
public:
    Remesh();
    ~Remesh();

    void start();
    void stop();
    
    // Evaluate current network topology and optimize
    void evaluateAndOptimize();
    
    // Update peer latency information
    void updatePeerLatency(const std::string& peerId, double latency);
    
    // Get optimal peer connections
    std::vector<std::string> getOptimalConnections();
    
    // Check if remesh is needed
    bool needsRemesh() const;
    
    void setRemeshThreshold(double thresholdMs);
    
private:
    mutable std::mutex topologyMutex;
    std::atomic<bool> running{false};
    std::thread remeshThread;
    
    std::map<std::string, double> peerLatencies;
    double remeshThresholdMs;
    
    void remeshLoop();
    std::vector<std::string> calculateOptimalTopology();
    void measureLatencies();
};