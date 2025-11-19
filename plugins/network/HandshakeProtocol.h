#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace SentinelFS {

/**
 * @brief Handshake protocol handler for peer connections
 * 
 * Manages the initial connection handshake:
 * 1. SENTINEL_HELLO|VERSION|PEER_ID|SESSION_CODE
 * 2. Validate session code
 * 3. SENTINEL_WELCOME|VERSION|PEER_ID or SENTINEL_REJECT|REASON
 * 4. Setup encryption if enabled
 */
class HandshakeProtocol {
public:
    struct HandshakeResult {
        bool success;
        std::string peerId;
        std::string errorMessage;
    };
    
    /**
     * @brief Constructor
     * @param localPeerId This peer's ID
     * @param sessionCode Expected session code (empty = accept all)
     * @param encryptionEnabled Whether encryption should be negotiated
     */
    HandshakeProtocol(const std::string& localPeerId,
                      const std::string& sessionCode,
                      bool encryptionEnabled);
    
    /**
     * @brief Perform handshake as client (initiator)
     * @param socket Connected socket
     * @return Handshake result with remote peer ID
     */
    HandshakeResult performClientHandshake(int socket);
    
    /**
     * @brief Perform handshake as server (receiver)
     * @param socket Accepted client socket
     * @return Handshake result with remote peer ID
     */
    HandshakeResult performServerHandshake(int socket);
    
    /**
     * @brief Update session code
     */
    void setSessionCode(const std::string& code) { sessionCode_ = code; }
    
    /**
     * @brief Update encryption setting
     */
    void setEncryptionEnabled(bool enabled) { encryptionEnabled_ = enabled; }
    
private:
    std::string createHelloMessage() const;
    std::string createWelcomeMessage() const;
    std::string createRejectMessage(const std::string& reason) const;
    
    bool sendMessage(int socket, const std::string& message);
    std::string receiveMessage(int socket, size_t maxSize = 1024);
    
    bool validateHelloMessage(const std::string& message, std::string& remotePeerId);
    
    std::string localPeerId_;
    std::string sessionCode_;
    bool encryptionEnabled_;
    
    static constexpr const char* PROTOCOL_VERSION = "1.0";
};

} // namespace SentinelFS
