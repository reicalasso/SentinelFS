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
    std::string createHelloMessage(const std::vector<uint8_t>& clientNonce) const;
    std::string createChallengeMessage(const std::string& remotePeerId,
                                       const std::vector<uint8_t>& clientNonce,
                                       const std::vector<uint8_t>& serverNonce) const;
    std::string createAuthMessage(const std::string& digest) const;
    std::string createWelcomeMessage() const;
    std::string createRejectMessage(const std::string& reason) const;
    
    bool sendMessage(int socket, const std::string& message);
    std::string receiveMessage(int socket, size_t maxSize = 1024);
    
    bool parseHelloMessage(const std::string& message,
                           std::string& remotePeerId,
                           std::vector<uint8_t>& clientNonce,
                           std::string& remoteSessionCode,
                           bool& legacyFormat);
    bool parseChallengeMessage(const std::string& message,
                               std::string& remotePeerId,
                               std::vector<uint8_t>& echoedClientNonce,
                               std::vector<uint8_t>& serverNonce);
    bool parseAuthMessage(const std::string& message,
                          std::string& remotePeerId,
                          std::string& digest);
    std::string computeAuthDigest(const std::vector<uint8_t>& clientNonce,
                                  const std::vector<uint8_t>& serverNonce,
                                  const std::string& remotePeerId,
                                  const std::string& purpose) const;
    std::vector<uint8_t> generateNonce() const;
    
    std::string localPeerId_;
    std::string sessionCode_;
    bool encryptionEnabled_;
    
    static constexpr const char* PROTOCOL_VERSION = "1.0";
    static constexpr int HANDSHAKE_TIMEOUT_SEC = 10;  // Timeout for handshake messages
};

} // namespace SentinelFS
