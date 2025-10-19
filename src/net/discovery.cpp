#include "discovery.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

Discovery::Discovery(const std::string& sessionCode) 
    : sessionCode(sessionCode), discoveryIntervalMs(5000) {
    // Initialize the UDP socket for discovery
    discoverySocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (discoverySocket < 0) {
        std::cerr << "Error creating discovery socket" << std::endl;
    } else {
        // Enable broadcast
        int broadcastEnable = 1;
        if (setsockopt(discoverySocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
            std::cerr << "Error setting broadcast option" << std::endl;
        }
        
        // Bind to any address for listening
        memset(&discoveryAddr, 0, sizeof(discoveryAddr));
        discoveryAddr.sin_family = AF_INET;
        discoveryAddr.sin_addr.s_addr = INADDR_ANY;
        discoveryAddr.sin_port = htons(8081); // Discovery port
        
        if (bind(discoverySocket, (struct sockaddr*)&discoveryAddr, sizeof(discoveryAddr)) < 0) {
            std::cerr << "Error binding discovery socket" << std::endl;
        }
    }
}

Discovery::~Discovery() {
    stop();
    if (discoverySocket >= 0) {
        close(discoverySocket);
    }
}

void Discovery::start() {
    running = true;
    
    // Start discovery thread
    discoveryThread = std::thread(&Discovery::discoveryLoop, this);
    
    // Start listener thread
    listenerThread = std::thread(&Discovery::listenForPeers, this);
}

void Discovery::stop() {
    running = false;
    
    // Close the socket to unblock the listener
    if (discoverySocket >= 0) {
        close(discoverySocket);
        discoverySocket = socket(AF_INET, SOCK_DGRAM, 0); // Create a new socket if needed again
    }
    
    if (discoveryThread.joinable()) {
        discoveryThread.join();
    }
    
    if (listenerThread.joinable()) {
        listenerThread.join();
    }
}

std::vector<PeerInfo> Discovery::getPeers() const {
    std::lock_guard<std::mutex> lock(peersMutex);
    return peers;
}

void Discovery::broadcastPresence() {
    struct sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(8081);
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

    // Create discovery packet with session code and port
    std::string packetData = "DISCOVERY|" + sessionCode + "|" + std::to_string(8080) + "|MyNode";
    
    ssize_t sent = sendto(discoverySocket, 
                          packetData.c_str(), 
                          packetData.length(), 
                          0, 
                          (struct sockaddr*)&broadcastAddr, 
                          sizeof(broadcastAddr));
    
    if (sent < 0) {
        std::cerr << "Error sending discovery packet" << std::endl;
    }
}

void Discovery::listenForPeers() {
    char buffer[1024];
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    while (running) {
        ssize_t received = recvfrom(discoverySocket, 
                                   buffer, 
                                   sizeof(buffer) - 1, 
                                   MSG_DONTWAIT,  // Non-blocking receive
                                   (struct sockaddr*)&clientAddr, 
                                   &clientLen);
        
        if (received > 0) {
            buffer[received] = '\0';
            std::string msg(buffer);
            
            // Parse discovery packet: "DISCOVERY|sessionCode|port|nodeId"
            if (msg.substr(0, 9) == "DISCOVERY") {
                size_t pos1 = msg.find('|', 9);
                if (pos1 != std::string::npos) {
                    std::string packetSessionCode = msg.substr(10, pos1 - 10);
                    
                    if (packetSessionCode == sessionCode) { // Only process if session code matches
                        size_t pos2 = msg.find('|', pos1 + 1);
                        if (pos2 != std::string::npos) {
                            std::string portStr = msg.substr(pos1 + 1, pos2 - pos1 - 1);
                            std::string nodeId = msg.substr(pos2 + 1);
                            
                            int port = std::stoi(portStr);
                            std::string address = inet_ntoa(clientAddr.sin_addr);
                            
                            // Update or add peer
                            std::lock_guard<std::mutex> lock(peersMutex);
                            
                            bool found = false;
                            for (auto& peer : peers) {
                                if (peer.address == address && peer.port == port) {
                                    peer.lastSeen = std::to_string(std::time(nullptr));
                                    found = true;
                                    break;
                                }
                            }
                            
                            if (!found) {
                                PeerInfo newPeer;
                                newPeer.id = nodeId;
                                newPeer.address = address;
                                newPeer.port = port;
                                newPeer.latency = 0.0; // Will be updated by ping
                                newPeer.active = true;
                                newPeer.lastSeen = std::to_string(std::time(nullptr));
                                
                                peers.push_back(newPeer);
                                
                                std::cout << "Discovered new peer: " << newPeer.id 
                                         << " at " << newPeer.address << ":" << newPeer.port << std::endl;
                            }
                        }
                    }
                }
            }
        }
        
        // Sleep briefly to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Discovery::setDiscoveryInterval(int milliseconds) {
    discoveryIntervalMs = milliseconds;
}

void Discovery::discoveryLoop() {
    while (running) {
        broadcastPresence();
        std::this_thread::sleep_for(std::chrono::milliseconds(discoveryIntervalMs));
    }
}

bool Discovery::sendDiscoveryPacket() {
    // This method is now integrated into broadcastPresence
    broadcastPresence();
    return true;
}

bool Discovery::receiveDiscoveryPacket() {
    // This is handled in the listenForPeers method
    return true;
}

