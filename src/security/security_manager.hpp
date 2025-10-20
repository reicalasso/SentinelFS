#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include "../models.hpp"
#include "../net/transfer.hpp"

// Access control levels
enum class AccessLevel {
    READ_ONLY,
    READ_WRITE,
    ADMIN,
    NONE
};

struct PeerCertificate {
    std::string peerId;
    std::string publicKey;
    std::string signature;
    std::string validFrom;
    std::string validUntil;
    AccessLevel accessLevel;
    
    PeerCertificate() : accessLevel(AccessLevel::READ_ONLY) {}
};

struct FileAccessRule {
    std::string peerId;
    std::string pathPattern;  // Regex or glob pattern
    AccessLevel accessLevel;
    bool allow;
    
    FileAccessRule() : accessLevel(AccessLevel::READ_ONLY), allow(true) {}
};

class SecurityManager {
public:
    SecurityManager();
    ~SecurityManager();
    
    // Initialize security components
    bool initialize(const std::string& privateKeyPath = "", 
                   const std::string& publicKeyPath = "");
    
    // Generate key pair for this node
    bool generateKeyPair(const std::string& privateKeyPath, 
                        const std::string& publicKeyPath);
    
    // Certificate management
    bool createCertificate(const PeerInfo& peer, const std::string& certificatePath);
    bool validateCertificate(const std::string& certificatePath);
    bool addPeerCertificate(const PeerCertificate& cert);
    PeerCertificate getPeerCertificate(const std::string& peerId) const;
    bool loadPeerCertificates(const std::string& directory);
    
    // Encryption methods
    std::vector<uint8_t> encryptData(const std::vector<uint8_t>& data, 
                                    const std::string& peerId);
    std::vector<uint8_t> decryptData(const std::vector<uint8_t>& encryptedData, 
                                    const std::string& peerId);
    
    // File encryption before transmission
    std::vector<uint8_t> encryptFile(const std::string& filepath);
    bool decryptFile(const std::vector<uint8_t>& encryptedData, 
                    const std::string& outputPath);
    
    // Authentication
    bool authenticatePeer(const PeerInfo& peer);
    bool signData(const std::vector<uint8_t>& data, std::vector<uint8_t>& signature);
    bool verifySignature(const std::vector<uint8_t>& data, 
                        const std::vector<uint8_t>& signature, 
                        const std::string& publicKey);
    
    // Access control
    bool checkAccess(const std::string& peerId, const std::string& filepath, 
                    AccessLevel requiredLevel);
    void addAccessRule(const FileAccessRule& rule);
    void removeAccessRule(const std::string& peerId, const std::string& pathPattern);
    
    // Rate limiting
    bool isRateLimited(const std::string& peerId);
    void recordPeerActivity(const std::string& peerId, size_t dataBytes = 0);
    
    // Traffic protection
    std::vector<uint8_t> padData(const std::vector<uint8_t>& data, size_t blockSize = 16);
    std::vector<uint8_t> unpadData(const std::vector<uint8_t>& data, bool& valid);
    
    // Get instance
    static SecurityManager& getInstance();
    
private:
    static std::unique_ptr<SecurityManager> instance;
    static std::mutex instanceMutex;
    
    RSA* privateKey;
    RSA* publicKey;
    std::map<std::string, PeerCertificate> peerCertificates;
    std::vector<FileAccessRule> accessRules;
    std::map<std::string, std::chrono::steady_clock::time_point> lastActivity;
    std::map<std::string, size_t> dataTransferred;
    std::map<std::string, std::vector<uint8_t>> sessionKeys;
    std::mutex securityMutex;
    
    // Rate limiting configuration
    size_t maxDataPerSecond;  // in bytes
    std::chrono::seconds timeWindow;

    // Local secrets
    std::vector<uint8_t> localStorageKey;
    std::string localPublicKeyPem;
    
    // Internal helper methods
    std::vector<uint8_t> getSessionKeyForPeer(const std::string& peerId);
    std::vector<uint8_t> getLocalStorageKey();
    std::vector<uint8_t> generateRandomKey(size_t bytes = 32);
    std::vector<uint8_t> aesEncrypt(const std::vector<uint8_t>& data, 
                                   const std::vector<uint8_t>& key);
    std::vector<uint8_t> aesDecrypt(const std::vector<uint8_t>& data, 
                                   const std::vector<uint8_t>& key);
    std::string hashData(const std::vector<uint8_t>& data);
};