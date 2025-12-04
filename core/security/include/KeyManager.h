#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <map>

namespace SentinelFS {

/**
 * @brief Key types in the SentinelFS security model
 * 
 * The key hierarchy follows a two-tier model:
 * 
 * 1. Identity Key (long-lived):
 *    - Ed25519 keypair for device identity
 *    - Used for signing and peer authentication
 *    - Stored securely on disk (encrypted at rest)
 *    - Lifespan: Years (until device is decommissioned)
 * 
 * 2. Session Key (short-lived):
 *    - X25519 ECDH for per-connection encryption
 *    - Derived during handshake using identity keys
 *    - Provides forward secrecy
 *    - Lifespan: Hours to days (configurable)
 */
enum class KeyType {
    IDENTITY_PUBLIC,     // Ed25519 public key (32 bytes)
    IDENTITY_PRIVATE,    // Ed25519 private key (64 bytes)
    SESSION,             // X25519/AES session key (32 bytes)
    SIGNING,             // Used for message authentication
    ENCRYPTION           // Used for data encryption
};

/**
 * @brief Key metadata and lifecycle information
 */
struct KeyInfo {
    std::string keyId;                              // Unique identifier (hash of public key)
    KeyType type;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point expires;  // 0 = no expiration (identity keys)
    std::string algorithm;                          // "Ed25519", "X25519", "AES-256-GCM"
    std::string peerId;                             // Associated peer (for session keys)
    bool compromised{false};                        // Flag for revoked keys
    uint32_t usageCount{0};                         // Number of times key was used
    
    bool isExpired() const;
    bool isValid() const;
    std::string toString() const;
};

/**
 * @brief Session key with automatic rotation
 */
struct SessionKey {
    std::string keyId;
    std::vector<uint8_t> key;                       // 32-byte symmetric key
    std::string peerId;                             // Remote peer
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point expires;
    std::chrono::system_clock::time_point lastUsed;
    uint64_t bytesEncrypted{0};                     // For key rotation threshold
    uint32_t messagesEncrypted{0};
    
    // Rotation thresholds
    static constexpr uint64_t MAX_BYTES = 1ULL << 30;      // 1 GB
    static constexpr uint32_t MAX_MESSAGES = 1000000;      // 1M messages
    
    bool needsRotation() const;
};

/**
 * @brief Device identity keypair
 */
struct IdentityKeyPair {
    std::string keyId;                              // SHA256 of public key
    std::vector<uint8_t> publicKey;                 // 32 bytes
    std::vector<uint8_t> privateKey;                // 64 bytes (includes public key)
    std::chrono::system_clock::time_point created;
    std::string deviceName;
    std::string fingerprint;                        // Human-readable fingerprint
    
    // Generate fingerprint for verification
    std::string computeFingerprint() const;
};

/**
 * @brief Key derivation parameters
 */
struct KeyDerivationParams {
    std::string context;                            // Domain separation string
    std::vector<uint8_t> salt;
    int iterations{100000};                         // For PBKDF2
    std::string info;                               // For HKDF
};

/**
 * @brief Key storage interface
 */
class IKeyStore {
public:
    virtual ~IKeyStore() = default;
    
    virtual bool store(const std::string& keyId, 
                       const std::vector<uint8_t>& keyData,
                       const KeyInfo& info) = 0;
    
    virtual std::vector<uint8_t> load(const std::string& keyId) = 0;
    
    virtual bool remove(const std::string& keyId) = 0;
    
    virtual std::vector<KeyInfo> list(KeyType type) = 0;
    
    virtual bool exists(const std::string& keyId) = 0;
};

/**
 * @brief Encrypted file-based key storage
 */
class FileKeyStore : public IKeyStore {
public:
    /**
     * @brief Constructor
     * @param storagePath Directory for key files
     * @param masterPassword Password for encrypting keys at rest
     */
    FileKeyStore(const std::string& storagePath, 
                 const std::string& masterPassword);
    
    ~FileKeyStore() override;
    
    bool store(const std::string& keyId,
               const std::vector<uint8_t>& keyData,
               const KeyInfo& info) override;
    
    std::vector<uint8_t> load(const std::string& keyId) override;
    
    bool remove(const std::string& keyId) override;
    
    std::vector<KeyInfo> list(KeyType type) override;
    
    bool exists(const std::string& keyId) override;
    
    /**
     * @brief Change master password
     * @param oldPassword Current password
     * @param newPassword New password
     * @return true if successful
     */
    bool changePassword(const std::string& oldPassword,
                        const std::string& newPassword);

private:
    std::string storagePath_;
    std::vector<uint8_t> masterKey_;
    std::mutex mutex_;
    
    std::string keyFilePath(const std::string& keyId) const;
    std::string metadataFilePath(const std::string& keyId) const;
    std::vector<uint8_t> deriveMasterKey(const std::string& password);
    std::vector<uint8_t> encryptKey(const std::vector<uint8_t>& plainKey);
    std::vector<uint8_t> decryptKey(const std::vector<uint8_t>& encryptedKey);
};

/**
 * @brief Key lifecycle manager
 * 
 * Manages the complete key hierarchy:
 * - Identity key generation and storage
 * - Session key derivation and rotation
 * - Key expiration and cleanup
 * - Peer key exchange
 */
class KeyManager {
public:
    /**
     * @brief Constructor
     * @param keyStore Key storage backend
     */
    explicit KeyManager(std::shared_ptr<IKeyStore> keyStore);
    
    ~KeyManager();
    
    // Disable copy
    KeyManager(const KeyManager&) = delete;
    KeyManager& operator=(const KeyManager&) = delete;
    
    //=====================================================
    // Identity Key Management
    //=====================================================
    
    /**
     * @brief Generate new identity keypair
     * @param deviceName Human-readable device name
     * @return true if successful
     */
    bool generateIdentityKey(const std::string& deviceName);
    
    /**
     * @brief Load existing identity key
     * @return true if identity key exists and was loaded
     */
    bool loadIdentityKey();
    
    /**
     * @brief Check if identity key is initialized
     */
    bool hasIdentityKey() const;
    
    /**
     * @brief Get local device's key ID
     */
    std::string getLocalKeyId() const;
    
    /**
     * @brief Get local public key for sharing with peers
     */
    std::vector<uint8_t> getPublicKey() const;
    
    /**
     * @brief Get human-readable key fingerprint for verification
     */
    std::string getFingerprint() const;
    
    /**
     * @brief Sign data with identity key
     * @param data Data to sign
     * @return Ed25519 signature (64 bytes)
     */
    std::vector<uint8_t> sign(const std::vector<uint8_t>& data);
    
    /**
     * @brief Verify signature from a peer
     * @param data Original data
     * @param signature Signature to verify
     * @param peerPublicKey Peer's public key
     * @return true if signature is valid
     */
    bool verify(const std::vector<uint8_t>& data,
                const std::vector<uint8_t>& signature,
                const std::vector<uint8_t>& peerPublicKey);
    
    //=====================================================
    // Peer Key Management
    //=====================================================
    
    /**
     * @brief Add a peer's public key (after verification)
     * @param peerId Peer identifier
     * @param publicKey Peer's Ed25519 public key
     * @param verified Whether key was verified out-of-band
     * @return true if successful
     */
    bool addPeerKey(const std::string& peerId,
                    const std::vector<uint8_t>& publicKey,
                    bool verified = false);
    
    /**
     * @brief Remove a peer's keys
     * @param peerId Peer to remove
     */
    void removePeerKeys(const std::string& peerId);
    
    /**
     * @brief Get peer's public key
     * @param peerId Peer identifier
     * @return Public key or empty vector if not found
     */
    std::vector<uint8_t> getPeerPublicKey(const std::string& peerId);
    
    /**
     * @brief Check if peer key is verified
     * @param peerId Peer identifier
     */
    bool isPeerKeyVerified(const std::string& peerId);
    
    /**
     * @brief Mark peer key as verified (after out-of-band verification)
     * @param peerId Peer identifier
     */
    void markPeerKeyVerified(const std::string& peerId);
    
    //=====================================================
    // Session Key Management
    //=====================================================
    
    /**
     * @brief Derive a session key with a peer using ECDH
     * @param peerId Remote peer ID
     * @param peerPublicKey Peer's ephemeral public key
     * @param isInitiator Whether we initiated the connection
     * @param sessionDuration Session key validity duration
     * @return Session key ID
     */
    std::string deriveSessionKey(
        const std::string& peerId,
        const std::vector<uint8_t>& peerPublicKey,
        bool isInitiator,
        std::chrono::seconds sessionDuration = std::chrono::hours(24));
    
    /**
     * @brief Get session key for a peer
     * @param peerId Peer identifier
     * @return Session key or empty if not found/expired
     */
    std::vector<uint8_t> getSessionKey(const std::string& peerId);
    
    /**
     * @brief Check if session key needs rotation
     * @param peerId Peer identifier
     */
    bool sessionNeedsRotation(const std::string& peerId);
    
    /**
     * @brief Record key usage for rotation tracking
     * @param peerId Peer identifier
     * @param bytes Bytes encrypted
     */
    void recordKeyUsage(const std::string& peerId, uint64_t bytes);
    
    /**
     * @brief Invalidate session key (force re-negotiation)
     * @param peerId Peer identifier
     */
    void invalidateSession(const std::string& peerId);
    
    /**
     * @brief Generate ephemeral X25519 keypair for key exchange
     * @return Pair of (public key, private key)
     */
    std::pair<std::vector<uint8_t>, std::vector<uint8_t>> 
        generateEphemeralKeyPair();
    
    //=====================================================
    // Key Rotation and Cleanup
    //=====================================================
    
    /**
     * @brief Remove expired session keys
     * @return Number of keys removed
     */
    size_t cleanupExpiredKeys();
    
    /**
     * @brief Set callback for when session key is rotated
     */
    using KeyRotationCallback = std::function<void(const std::string& peerId)>;
    void setKeyRotationCallback(KeyRotationCallback callback);
    
    /**
     * @brief Export identity for backup (encrypted)
     * @param password Backup encryption password
     * @return Encrypted backup data
     */
    std::vector<uint8_t> exportIdentity(const std::string& password);
    
    /**
     * @brief Import identity from backup
     * @param backupData Encrypted backup data
     * @param password Backup password
     * @return true if successful
     */
    bool importIdentity(const std::vector<uint8_t>& backupData,
                        const std::string& password);
    
    //=====================================================
    // Session Code Integration
    //=====================================================
    
    /**
     * @brief Derive temporary key from session code (for bootstrap)
     * 
     * Session codes provide a simple way for devices to initially
     * connect and exchange identity keys. After key exchange,
     * communication switches to identity-based authentication.
     * 
     * @param sessionCode 6-character session code
     * @param salt Random salt
     * @return Derived key
     */
    std::vector<uint8_t> deriveKeyFromSessionCode(
        const std::string& sessionCode,
        const std::vector<uint8_t>& salt);
    
    /**
     * @brief Upgrade from session-code auth to identity-key auth
     * 
     * After initial pairing via session code, peers should verify
     * each other's identity keys and transition to key-based auth.
     * 
     * @param peerId Peer identifier
     * @param peerPublicKey Peer's identity public key
     * @return true if upgrade successful
     */
    bool upgradeToIdentityAuth(const std::string& peerId,
                                const std::vector<uint8_t>& peerPublicKey);

private:
    std::shared_ptr<IKeyStore> keyStore_;
    std::unique_ptr<IdentityKeyPair> identityKey_;
    std::map<std::string, std::vector<uint8_t>> peerPublicKeys_;
    std::map<std::string, bool> peerKeyVerified_;
    std::map<std::string, SessionKey> sessionKeys_;
    KeyRotationCallback rotationCallback_;
    mutable std::mutex mutex_;
    
    std::vector<uint8_t> performECDH(
        const std::vector<uint8_t>& privateKey,
        const std::vector<uint8_t>& peerPublicKey);
    
    std::vector<uint8_t> deriveSessionKeyFromShared(
        const std::vector<uint8_t>& sharedSecret,
        const std::string& context,
        bool isInitiator);
};

} // namespace SentinelFS
