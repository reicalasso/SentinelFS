#pragma once

/**
 * @file SessionManager.h
 * @brief Session and security management for NetFalcon
 * 
 * Handles:
 * - Session code management
 * - Encryption key derivation and rotation
 * - Handshake orchestration
 * - Multi-session support
 */

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <chrono>
#include <functional>
#include <cstdint>

namespace SentinelFS {
namespace NetFalcon {

/**
 * @brief Session state
 */
enum class SessionState {
    INACTIVE,
    PENDING,
    ACTIVE,
    EXPIRED,
    REVOKED
};

/**
 * @brief Peer authentication state
 */
enum class AuthState {
    UNKNOWN,
    HANDSHAKE_PENDING,
    AUTHENTICATED,
    REJECTED,
    EXPIRED
};

/**
 * @brief Session info
 */
struct SessionInfo {
    std::string sessionId;
    std::string sessionCode;
    SessionState state{SessionState::INACTIVE};
    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point expiresAt;
    std::size_t peerCount{0};
    bool encryptionEnabled{false};
};

/**
 * @brief Peer session binding
 */
struct PeerSession {
    std::string peerId;
    std::string sessionId;
    AuthState authState{AuthState::UNKNOWN};
    std::chrono::steady_clock::time_point authenticatedAt;
    std::vector<uint8_t> sharedSecret;  // Per-peer derived key
    uint64_t messageCounter{0};         // For replay protection
};

/**
 * @brief Handshake result
 */
struct HandshakeResult {
    bool success{false};
    std::string peerId;
    std::string errorMessage;
    std::vector<uint8_t> sharedSecret;
};

/**
 * @brief Handshake callbacks
 */
using HandshakeCallback = std::function<void(const HandshakeResult&)>;

/**
 * @brief Session manager for NetFalcon
 * 
 * Provides:
 * - Multi-session support (different workspaces)
 * - Secure key derivation from session codes
 * - Automatic key rotation
 * - Replay attack protection
 */
class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    /**
     * @brief Set local peer ID
     */
    void setLocalPeerId(const std::string& peerId);
    std::string getLocalPeerId() const;

    /**
     * @brief Create or update primary session
     * @param sessionCode 6-character session code
     * @param enableEncryption Enable encryption for this session
     */
    void setSessionCode(const std::string& sessionCode, bool enableEncryption = true);
    std::string getSessionCode() const;

    /**
     * @brief Create a named session (for multi-workspace support)
     * @param name Session name/identifier
     * @param sessionCode Session code
     * @param enableEncryption Enable encryption
     * @return Session ID
     */
    std::string createSession(const std::string& name, const std::string& sessionCode, bool enableEncryption = true);

    /**
     * @brief Get session info
     */
    SessionInfo getSessionInfo(const std::string& sessionId = "") const;

    /**
     * @brief Check if encryption is enabled
     */
    bool isEncryptionEnabled() const;
    void setEncryptionEnabled(bool enable);

    /**
     * @brief Derive encryption key from session code
     * @param sessionCode Session code
     * @param salt Optional salt (uses default if empty)
     * @return Derived key (32 bytes for AES-256)
     */
    std::vector<uint8_t> deriveKey(const std::string& sessionCode, const std::vector<uint8_t>& salt = {}) const;

    /**
     * @brief Get current encryption key
     */
    std::vector<uint8_t> getEncryptionKey() const;

    /**
     * @brief Rotate encryption key
     * 
     * Derives new key with incremented rotation counter.
     * Old key remains valid for a grace period.
     */
    void rotateKey();

    /**
     * @brief Encrypt data
     * @param plaintext Data to encrypt
     * @param peerId Target peer (for per-peer keys)
     * @return Encrypted data with IV and HMAC
     */
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext, const std::string& peerId = "") const;

    /**
     * @brief Decrypt data
     * @param ciphertext Encrypted data
     * @param peerId Source peer
     * @return Decrypted data or empty on failure
     */
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext, const std::string& peerId = "") const;

    /**
     * @brief Verify session code matches
     */
    bool verifySessionCode(const std::string& code) const;

    /**
     * @brief Register authenticated peer
     */
    void registerPeer(const std::string& peerId, const std::string& sessionId = "");

    /**
     * @brief Unregister peer
     */
    void unregisterPeer(const std::string& peerId);

    /**
     * @brief Get peer session info
     */
    PeerSession getPeerSession(const std::string& peerId) const;

    /**
     * @brief Check if peer is authenticated
     */
    bool isPeerAuthenticated(const std::string& peerId) const;

    /**
     * @brief Update peer auth state
     */
    void updatePeerAuthState(const std::string& peerId, AuthState state);

    /**
     * @brief Get next message counter for replay protection
     */
    uint64_t getNextMessageCounter(const std::string& peerId);

    /**
     * @brief Verify message counter (replay protection)
     */
    bool verifyMessageCounter(const std::string& peerId, uint64_t counter);

    /**
     * @brief Create client handshake message
     */
    std::vector<uint8_t> createClientHello() const;

    /**
     * @brief Process server challenge
     */
    std::vector<uint8_t> processChallenge(const std::vector<uint8_t>& challenge) const;

    /**
     * @brief Create server challenge
     */
    std::vector<uint8_t> createServerChallenge(const std::string& clientPeerId, const std::vector<uint8_t>& clientHello) const;

    /**
     * @brief Verify client response
     */
    HandshakeResult verifyClientResponse(const std::string& clientPeerId, const std::vector<uint8_t>& response) const;

    /**
     * @brief Cleanup expired sessions and peers
     */
    void cleanup();

private:
    std::vector<uint8_t> computeAuthDigest(
        const std::vector<uint8_t>& clientNonce,
        const std::vector<uint8_t>& serverNonce,
        const std::string& clientPeerId,
        const std::string& serverPeerId
    ) const;

    mutable std::mutex mutex_;
    
    std::string localPeerId_;
    std::string primarySessionCode_;
    bool encryptionEnabled_{false};
    std::vector<uint8_t> encryptionKey_;
    uint32_t keyRotationCounter_{0};
    
    std::map<std::string, SessionInfo> sessions_;
    std::map<std::string, PeerSession> peerSessions_;
    std::map<std::string, uint64_t> lastSeenCounters_;  // For replay protection
    
    // Handshake state
    std::map<std::string, std::vector<uint8_t>> pendingChallenges_;
    
    static constexpr size_t KEY_SIZE = 32;  // AES-256
    static constexpr size_t NONCE_SIZE = 16;
    static const std::vector<uint8_t> DEFAULT_SALT;
};

} // namespace NetFalcon
} // namespace SentinelFS
