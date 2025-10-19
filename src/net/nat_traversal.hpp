#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../models.hpp"

class NATTraversal {
public:
    NATTraversal();
    ~NATTraversal();

    // Use STUN to discover external IP and port
    bool discoverExternalAddress(std::string& externalIP, int& externalPort, 
                                 const std::string& stunServer = "stun.l.google.com", 
                                 int stunPort = 19302);
    
    // Punch through NAT to peer (simplified approach)
    bool punchHoleForPeer(const PeerInfo& peer);
    
    // Check NAT type (simplified)
    std::string getNATType();
    
private:
    int stunSocket;
    struct sockaddr_in localAddr;
    
    // STUN protocol methods
    bool sendSTUNRequest(int sock, const struct sockaddr_in& stunAddr);
    bool receiveSTUNResponse(int sock, std::string& externalIP, int& externalPort);
    
    // Helper to create socket
    int createUDPSocket(int localPort = 0);
};