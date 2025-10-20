#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../fs/delta_engine.hpp"  // For DeltaData
#include "../models.hpp"  // For PeerInfo
#include "nat_traversal.hpp"  // For NAT traversal

// Forward declarations
class SecureTransfer;
class SecurityManager;

struct Connection {
    int socket;
    struct sockaddr_in addr;
    std::chrono::steady_clock::time_point lastUsed;
    bool connected;
    void* ssl;  // For TLS connections (void* to avoid including openssl headers here)
    
    Connection() : socket(-1), lastUsed(std::chrono::steady_clock::now()), connected(false), ssl(nullptr) {}
};

class Transfer {
public:
    Transfer();
    ~Transfer();

    // Send delta data to a specific peer
    bool sendDelta(const DeltaData& delta, const PeerInfo& peer);
    
    // Receive delta data from a peer
    bool receiveDelta(DeltaData& delta, const PeerInfo& peer);
    
    // Send file to peer
    bool sendFile(const std::string& filePath, const PeerInfo& peer);
    
    // Receive file from peer
    bool receiveFile(const std::string& filePath, const PeerInfo& peer);
    
    // Broadcast delta to all peers
    bool broadcastDelta(const DeltaData& delta, const std::vector<PeerInfo>& peers);
    
    // Connection management
    bool establishConnection(const PeerInfo& peer);
    void closeConnection(const PeerInfo& peer);
    void cleanupConnections();
    
    // Enable/disable security
    void enableSecurity(bool enable) { securityEnabled = enable; }
    void setSecurityManager(SecurityManager* mgr) { securityManager = mgr; }
    bool isSecurityEnabled() const { return securityEnabled; }
    
    // Get secure transfer instance
    SecureTransfer* getSecureTransfer() { return secureTransfer.get(); }
    
private:
    std::atomic<bool> active{false};
    std::mutex transferMutex;
    
    // Connection pooling
    std::map<std::string, Connection> connectionPool;
    std::mutex connectionMutex;
    
    // Security
    bool securityEnabled{true};  // Security ON by default
    SecurityManager* securityManager{nullptr};
    std::unique_ptr<SecureTransfer> secureTransfer;
    
    // Internal transfer methods
    bool sendRawData(const std::vector<uint8_t>& data, const PeerInfo& peer);
    bool receiveRawData(std::vector<uint8_t>& data, const PeerInfo& peer);
    
    // Helper methods
    int getOrCreateSocket(const PeerInfo& peer);
    bool sendWithRetry(int sock, const void* data, size_t length);
    bool receiveWithRetry(int sock, void* data, size_t length);
    
    // TLS/SSL methods
    bool initializeTLS();
    bool setupSecureConnection(int sock);
    bool sendSecureData(int sock, const void* data, size_t length);
    bool receiveSecureData(int sock, void* data, size_t length);
};