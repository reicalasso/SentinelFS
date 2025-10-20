#include "nat_traversal.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

NATTraversal::NATTraversal() : stunSocket(-1) {
    // Initialize with default values
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
}

NATTraversal::~NATTraversal() {
    if (stunSocket != -1) {
        close(stunSocket);
    }
}

bool NATTraversal::discoverExternalAddress(std::string& externalIP, int& externalPort, 
                                          const std::string& stunServer, int stunPort) {
    // Create UDP socket
    int sock = createUDPSocket();
    if (sock < 0) {
        std::cerr << "Failed to create socket for STUN" << std::endl;
        return false;
    }

    // Resolve STUN server using DNS to support hostnames (e.g. stun.l.google.com)
    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo* result = nullptr;
    int gaiStatus = getaddrinfo(stunServer.c_str(), nullptr, &hints, &result);
    if (gaiStatus != 0 || result == nullptr) {
        std::cerr << "Failed to resolve STUN server: " << stunServer
                  << " (" << gai_strerror(gaiStatus) << ")" << std::endl;
        if (result) {
            freeaddrinfo(result);
        }
        close(sock);
        return false;
    }

    // Setup STUN server address
    struct sockaddr_in stunAddr;
    memset(&stunAddr, 0, sizeof(stunAddr));
    stunAddr.sin_family = AF_INET;
    stunAddr.sin_port = htons(stunPort);
    stunAddr.sin_addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr)->sin_addr;
    freeaddrinfo(result);
    
    // Send STUN request
    if (!sendSTUNRequest(sock, stunAddr)) {
        std::cerr << "Failed to send STUN request" << std::endl;
        close(sock);
        return false;
    }
    
    // Receive STUN response
    bool success = receiveSTUNResponse(sock, externalIP, externalPort);
    close(sock);
    
    return success;
}

bool NATTraversal::punchHoleForPeer(const PeerInfo& peer) {
    // Create a UDP socket and send a packet to the peer
    // This creates a NAT mapping that allows the peer to respond
    int sock = createUDPSocket();
    if (sock < 0) {
        return false;
    }
    
    struct sockaddr_in peerAddr;
    memset(&peerAddr, 0, sizeof(peerAddr));
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(peer.port);
    
    if (inet_pton(AF_INET, peer.address.c_str(), &peerAddr.sin_addr) <= 0) {
        std::cerr << "Invalid peer address: " << peer.address << std::endl;
        close(sock);
        return false;
    }
    
    // Send a small "hole punch" packet
    const char* holePunchMsg = "HOLE_PUNCH";
    ssize_t sent = sendto(sock, holePunchMsg, strlen(holePunchMsg), 0, 
                         (struct sockaddr*)&peerAddr, sizeof(peerAddr));
    
    if (sent < 0) {
        std::cerr << "Failed to punch hole for peer " << peer.id << std::endl;
        close(sock);
        return false;
    }
    
    // Keep the socket open briefly to maintain the NAT mapping
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    close(sock);
    
    std::cout << "Hole punched for peer: " << peer.id << std::endl;
    return true;
}

std::string NATTraversal::getNATType() {
    // In a real implementation, you would test different STUN behaviors
    // to determine NAT type (full cone, restricted cone, port restricted cone, symmetric)
    // For now, return a default type
    return "Unknown";
}

// Private methods
bool NATTraversal::sendSTUNRequest(int sock, const struct sockaddr_in& stunAddr) {
    // Basic STUN message structure (Binding Request)
    // STUN header: 20 bytes
    unsigned char stunMsg[20];
    // Message type: 0x0001 = Binding Request
    stunMsg[0] = 0x00; 
    stunMsg[1] = 0x01;
    // Message length: 0 (no attributes)
    stunMsg[2] = 0x00;
    stunMsg[3] = 0x00;
    // Magic cookie: 0x2112A442
    stunMsg[4] = 0x21;
    stunMsg[5] = 0x12;
    stunMsg[6] = 0xA4;
    stunMsg[7] = 0x42;
    // Transaction ID (random)
    for (int i = 8; i < 20; i++) {
        stunMsg[i] = rand() % 256;
    }
    
    ssize_t sent = sendto(sock, stunMsg, 20, 0, 
                         (struct sockaddr*)&stunAddr, sizeof(stunAddr));
    
    return sent == 20;
}

bool NATTraversal::receiveSTUNResponse(int sock, std::string& externalIP, int& externalPort) {
    unsigned char buffer[1024];
    socklen_t addrLen = sizeof(struct sockaddr_in);
    
    ssize_t receivedRaw = recvfrom(sock, buffer, sizeof(buffer), 0, nullptr, &addrLen);
    if (receivedRaw < 20) {
        return false; // Not enough data for STUN header
    }

    const size_t received = static_cast<size_t>(receivedRaw);
    
    // Parse STUN response
    // Byte 0-1: Message type
    uint16_t msgType = (buffer[0] << 8) | buffer[1];
    
    if (msgType != 0x0101) { // 0x0101 = Binding Response
        std::cerr << "STUN response has wrong type: " << std::hex << msgType << std::endl;
        return false;
    }
    
    // Parse attributes looking for XOR-MAPPED-ADDRESS
    size_t offset = 20; // Skip header

    while (offset + 4 <= received) {
        
        uint16_t attrType = (buffer[offset] << 8) | buffer[offset + 1];
        uint16_t attrLen = (buffer[offset + 2] << 8) | buffer[offset + 3];
        offset += 4;

        if (offset + attrLen > received) {
            break;
        }
        
        if (attrType == 0x0020) { // XOR-MAPPED-ADDRESS attribute
            if (attrLen >= 8 && buffer[offset] == 0x00) { // AF_INET (IPv4)
                // XOR-Port (XORed with magic cookie 0x2112A442)
                uint16_t portXORed = (buffer[offset + 2] << 8) | buffer[offset + 3];
                uint16_t magicCookiePort = 0x2112; // First 16 bits of magic cookie
                externalPort = portXORed ^ magicCookiePort;
                
                // XOR-IP (XORed with magic cookie)
                uint32_t ipXORed = (buffer[offset + 4] << 24) | (buffer[offset + 5] << 16) |
                                  (buffer[offset + 6] << 8) | buffer[offset + 7];
                uint32_t magicCookieIP = 0x2112A442; // Full magic cookie
                uint32_t externalIPInt = ipXORed ^ magicCookieIP;
                
                // Convert to string
                struct in_addr ipAddr;
                ipAddr.s_addr = htonl(externalIPInt);
                externalIP = inet_ntoa(ipAddr);
                
                return true;
            }
        }
        
        // Align to 4-byte boundary
        offset += attrLen;
        offset = (offset + 3) & ~static_cast<size_t>(3); // Align to 4-byte boundary
    }
    
    // If XOR-MAPPED-ADDRESS not found, try MAPPED-ADDRESS (deprecated but still used)
    offset = 20;
    while (offset + 4 <= received) {
        
        uint16_t attrType = (buffer[offset] << 8) | buffer[offset + 1];
        uint16_t attrLen = (buffer[offset + 2] << 8) | buffer[offset + 3];
        offset += 4;

        if (offset + attrLen > received) {
            break;
        }
        
        if (attrType == 0x0001) { // MAPPED-ADDRESS attribute
            if (attrLen >= 8 && buffer[offset] == 0x01) { // IPv4
                externalPort = (buffer[offset + 2] << 8) | buffer[offset + 3];
                
                struct in_addr ipAddr;
                ipAddr.s_addr = (buffer[offset + 4] << 24) | (buffer[offset + 5] << 16) |
                               (buffer[offset + 6] << 8) | buffer[offset + 7];
                externalIP = inet_ntoa(ipAddr);
                
                return true;
            }
        }
        
        // Align to 4-byte boundary
        offset += attrLen;
        offset = (offset + 3) & ~static_cast<size_t>(3);
    }
    
    return false;
}

int NATTraversal::createUDPSocket(int localPort) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sock);
        return -1;
    }
    
    // Bind to local address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(localPort);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}