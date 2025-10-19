#include "security_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <regex>

// Static members initialization
std::unique_ptr<SecurityManager> SecurityManager::instance = nullptr;
std::mutex SecurityManager::instanceMutex;

SecurityManager::SecurityManager() 
    : privateKey(nullptr), publicKey(nullptr), 
      maxDataPerSecond(10 * 1024 * 1024),  // 10 MB/s default limit
      timeWindow(std::chrono::seconds(1)) {
}

SecurityManager::~SecurityManager() {
    if (privateKey) {
        RSA_free(privateKey);
    }
    if (publicKey) {
        RSA_free(publicKey);
    }
}

SecurityManager& SecurityManager::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (!instance) {
        instance = std::make_unique<SecurityManager>();
        instance->initialize();
    }
    return *instance;
}

bool SecurityManager::initialize(const std::string& privateKeyPath, 
                                const std::string& publicKeyPath) {
    // If no keys provided, try to load existing ones or generate new ones
    if (privateKeyPath.empty() || publicKeyPath.empty()) {
        // In a real implementation, we'd check for existing key files
        // For now, we'll generate temporary keys
        return generateKeyPair("temp_private.pem", "temp_public.pem");
    }
    
    // Load private key
    FILE* privFile = fopen(privateKeyPath.c_str(), "r");
    if (privFile) {
        privateKey = PEM_read_RSAPrivateKey(privFile, NULL, NULL, NULL);
        fclose(privFile);
    }
    
    // Load public key
    FILE* pubFile = fopen(publicKeyPath.c_str(), "r");
    if (pubFile) {
        publicKey = PEM_read_RSAPublicKey(pubFile, NULL, NULL, NULL);
        fclose(pubFile);
    }
    
    return (privateKey != nullptr && publicKey != nullptr);
}

bool SecurityManager::generateKeyPair(const std::string& privateKeyPath, 
                                     const std::string& publicKeyPath) {
    // Create RSA key pair
    int bits = 2048;
    BIGNUM* e = BN_new();
    BN_set_word(e, 65537);
    
    RSA* rsaKey = RSA_new();
    if (!RSA_generate_key_ex(rsaKey, bits, e, NULL)) {
        BN_free(e);
        RSA_free(rsaKey);
        return false;
    }
    
    // Save private key
    FILE* privFile = fopen(privateKeyPath.c_str(), "w");
    if (privFile) {
        PEM_write_RSAPrivateKey(privFile, rsaKey, NULL, NULL, 0, NULL, NULL);
        fclose(privFile);
    }
    
    // Save public key
    FILE* pubFile = fopen(publicKeyPath.c_str(), "w");
    if (pubFile) {
        PEM_write_RSAPublicKey(pubFile, rsaKey);
        fclose(pubFile);
    }
    
    // Store keys in our instance
    if (privateKey) RSA_free(privateKey);
    if (publicKey) RSA_free(publicKey);
    
    privateKey = RSAPrivateKey_dup(rsaKey);
    publicKey = RSAPublicKey_dup(rsaKey);
    
    BN_free(e);
    RSA_free(rsaKey);
    
    return (privFile != nullptr && pubFile != nullptr);
}

bool SecurityManager::createCertificate(const PeerInfo& peer, const std::string& certificatePath) {
    // Create a basic certificate structure
    std::ofstream certFile(certificatePath);
    if (!certFile) {
        return false;
    }
    
    // Generate certificate data
    auto now = std::chrono::system_clock::now();
    auto validFrom = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    auto validUntil = validFrom + (365 * 24 * 60 * 60); // 1 year validity
    
    certFile << "PeerID:" << peer.id << std::endl;
    certFile << "PublicKey:" << peer.address << std::endl;  // In a real system this would be the actual public key
    certFile << "ValidFrom:" << validFrom << std::endl;
    certFile << "ValidUntil:" << validUntil << std::endl;
    certFile << "AccessLevel:READ_WRITE" << std::endl;
    
    // Sign the certificate
    std::string certData = "PeerID:" + peer.id + "PublicKey:" + peer.address + 
                          "ValidFrom:" + std::to_string(validFrom);
    std::vector<uint8_t> signature;
    if (signData(std::vector<uint8_t>(certData.begin(), certData.end()), signature)) {
        certFile << "Signature:" << std::string(signature.begin(), signature.end()) << std::endl;
    }
    
    certFile.close();
    return true;
}

bool SecurityManager::validateCertificate(const std::string& certificatePath) {
    // Read certificate file
    std::ifstream certFile(certificatePath);
    if (!certFile) {
        return false;
    }
    
    std::string line;
    std::string certData;
    std::vector<uint8_t> signature;
    
    while (std::getline(certFile, line)) {
        if (line.substr(0, 9) == "Signature") {
            // Extract signature - simplified for this example
            std::string sigStr = line.substr(10);
            signature = std::vector<uint8_t>(sigStr.begin(), sigStr.end());
        } else {
            certData += line;
        }
    }
    
    // Verify the signature against the certificate data
    std::vector<uint8_t> certDataBytes(certData.begin(), certData.end());
    return verifySignature(certDataBytes, signature, "");
}

bool SecurityManager::addPeerCertificate(const PeerCertificate& cert) {
    std::lock_guard<std::mutex> lock(securityMutex);
    peerCertificates[cert.peerId] = cert;
    return true;
}

PeerCertificate SecurityManager::getPeerCertificate(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(securityMutex));
    auto it = peerCertificates.find(peerId);
    if (it != peerCertificates.end()) {
        return it->second;
    }
    return PeerCertificate();
}

std::vector<uint8_t> SecurityManager::encryptData(const std::vector<uint8_t>& data, 
                                                 const std::string& peerId) {
    // For end-to-end encryption, we'll use AES with a session key
    std::string sessionKey = generateSessionKey();
    
    // In a real implementation, we would securely exchange the session key
    // For now, we'll just use AES encryption
    return aesEncrypt(data, sessionKey);
}

std::vector<uint8_t> SecurityManager::decryptData(const std::vector<uint8_t>& encryptedData, 
                                                 const std::string& peerId) {
    // Same approach as encryption - using AES
    std::string sessionKey = generateSessionKey();
    return aesDecrypt(encryptedData, sessionKey);
}

std::vector<uint8_t> SecurityManager::encryptFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return {};
    }
    
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> fileData(fileSize);
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    file.close();
    
    return aesEncrypt(fileData, generateSessionKey());
}

bool SecurityManager::decryptFile(const std::vector<uint8_t>& encryptedData, 
                                 const std::string& outputPath) {
    auto decryptedData = aesDecrypt(encryptedData, generateSessionKey());
    
    std::ofstream file(outputPath, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(decryptedData.data()), decryptedData.size());
    file.close();
    
    return true;
}

bool SecurityManager::authenticatePeer(const PeerInfo& peer) {
    // Check if peer has a valid certificate
    auto cert = getPeerCertificate(peer.id);
    if (cert.peerId.empty()) {
        return false; // No certificate for this peer
    }
    
    // Check certificate validity (expiration, etc.)
    // In a real system, we would check signatures and CA chains
    return true;
}

bool SecurityManager::signData(const std::vector<uint8_t>& data, std::vector<uint8_t>& signature) {
    if (!privateKey) {
        return false;
    }
    
    EVP_MD_CTX* mdctx = EVP_MD_CTX_create();
    if (!mdctx) {
        return false;
    }
    
    if (EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, EVP_PKEY_new()) <= 0) {
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }
    
    if (EVP_DigestSignUpdate(mdctx, data.data(), data.size()) <= 0) {
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }
    
    size_t siglen;
    if (EVP_DigestSignFinal(mdctx, NULL, &siglen) <= 0) {
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }
    
    signature.resize(siglen);
    if (EVP_DigestSignFinal(mdctx, signature.data(), &siglen) <= 0) {
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }
    
    // Resize to actual signature length
    signature.resize(siglen);
    EVP_MD_CTX_destroy(mdctx);
    
    return true;
}

bool SecurityManager::verifySignature(const std::vector<uint8_t>& data, 
                                     const std::vector<uint8_t>& signature, 
                                     const std::string& publicKeyStr) {
    // In a real implementation, we would use the provided public key
    // For now, we'll use our own public key as an example
    if (!publicKey) {
        return false;
    }
    
    EVP_MD_CTX* mdctx = EVP_MD_CTX_create();
    if (!mdctx) {
        return false;
    }
    
    if (EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, EVP_PKEY_new()) <= 0) {
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }
    
    if (EVP_DigestVerifyUpdate(mdctx, data.data(), data.size()) <= 0) {
        EVP_MD_CTX_destroy(mdctx);
        return false;
    }
    
    int result = EVP_DigestVerifyFinal(mdctx, signature.data(), signature.size());
    EVP_MD_CTX_destroy(mdctx);
    
    return (result == 1);
}

bool SecurityManager::checkAccess(const std::string& peerId, const std::string& filepath, 
                                 AccessLevel requiredLevel) {
    std::lock_guard<std::mutex> lock(securityMutex);
    
    // Check peer's certificate access level
    auto cert = getPeerCertificate(peerId);
    if (cert.accessLevel < requiredLevel) {
        return false;
    }
    
    // Check specific access rules
    for (const auto& rule : accessRules) {
        if (rule.peerId == peerId) {
            // Check if path matches pattern (simplified)
            if (filepath.find(rule.pathPattern) != std::string::npos) {
                if (!rule.allow) {
                    return false; // Explicitly denied
                }
                // Check if rule grants required access level
                if (rule.accessLevel >= requiredLevel) {
                    return true;
                }
            }
        }
    }
    
    // Default: allow if no specific rule exists and certificate level is sufficient
    return true;
}

void SecurityManager::addAccessRule(const FileAccessRule& rule) {
    std::lock_guard<std::mutex> lock(securityMutex);
    accessRules.push_back(rule);
}

void SecurityManager::removeAccessRule(const std::string& peerId, const std::string& pathPattern) {
    std::lock_guard<std::mutex> lock(securityMutex);
    accessRules.erase(
        std::remove_if(accessRules.begin(), accessRules.end(),
            [&](const FileAccessRule& rule) {
                return rule.peerId == peerId && rule.pathPattern == pathPattern;
            }),
        accessRules.end()
    );
}

bool SecurityManager::isRateLimited(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(securityMutex);
    
    auto now = std::chrono::steady_clock::now();
    
    // Clean up old records
    for (auto it = lastActivity.begin(); it != lastActivity.end();) {
        if (now - it->second > timeWindow * 2) {
            it = lastActivity.erase(it);
            dataTransferred.erase(it->first);
        } else {
            ++it;
        }
    }
    
    // Check if peer has exceeded rate limit
    auto activityIt = lastActivity.find(peerId);
    if (activityIt != lastActivity.end()) {
        auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(
            now - activityIt->second);
        
        if (timeDiff < timeWindow) {
            auto dataIt = dataTransferred.find(peerId);
            if (dataIt != dataTransferred.end() && dataIt->second > maxDataPerSecond) {
                return true; // Rate limited
            }
        } else {
            // Reset counter for this period
            lastActivity[peerId] = now;
            dataTransferred[peerId] = 0;
        }
    } else {
        // First activity for this peer
        lastActivity[peerId] = now;
        dataTransferred[peerId] = 0;
    }
    
    return false;
}

void SecurityManager::recordPeerActivity(const std::string& peerId, size_t dataBytes) {
    std::lock_guard<std::mutex> lock(securityMutex);
    dataTransferred[peerId] += dataBytes;
    lastActivity[peerId] = std::chrono::steady_clock::now();
}

std::vector<uint8_t> SecurityManager::padData(const std::vector<uint8_t>& data, size_t blockSize) {
    size_t padding = blockSize - (data.size() % blockSize);
    if (padding == blockSize) padding = 0;  // No padding needed if already aligned
    
    std::vector<uint8_t> paddedData = data;
    paddedData.resize(data.size() + padding, padding);
    
    return paddedData;
}

std::vector<uint8_t> SecurityManager::unpadData(const std::vector<uint8_t>& data) {
    if (data.empty()) return data;
    
    uint8_t padding = data.back();
    if (padding == 0 || padding > 16) return data; // Invalid padding
    
    // Verify padding is consistent
    for (size_t i = data.size() - padding; i < data.size(); ++i) {
        if (data[i] != padding) return data; // Invalid padding
    }
    
    return std::vector<uint8_t>(data.begin(), data.end() - padding);
}

std::string SecurityManager::generateSessionKey() {
    unsigned char key[32]; // 256-bit key
    RAND_bytes(key, 32);
    
    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << (int)key[i];
    }
    
    return ss.str();
}

std::vector<uint8_t> SecurityManager::aesEncrypt(const std::vector<uint8_t>& data, 
                                                const std::string& key) {
    // Pad data to AES block size
    auto paddedData = padData(data, AES_BLOCK_SIZE);
    
    // Generate random IV
    unsigned char iv[AES_BLOCK_SIZE];
    RAND_bytes(iv, AES_BLOCK_SIZE);
    
    // Create cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    // Initialize cipher
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, 
                          reinterpret_cast<const unsigned char*>(key.substr(0, 32).c_str()), iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    // Encrypt
    std::vector<uint8_t> result(paddedData.size() + AES_BLOCK_SIZE);
    int len;
    int totalLen = 0;
    
    if (EVP_EncryptUpdate(ctx, result.data(), &len, paddedData.data(), paddedData.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen = len;
    
    if (EVP_EncryptFinal_ex(ctx, result.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;
    
    // Prepend IV to result
    std::vector<uint8_t> finalResult;
    finalResult.insert(finalResult.end(), iv, iv + AES_BLOCK_SIZE);
    finalResult.insert(finalResult.end(), result.begin(), result.begin() + totalLen);
    
    EVP_CIPHER_CTX_free(ctx);
    return finalResult;
}

std::vector<uint8_t> SecurityManager::aesDecrypt(const std::vector<uint8_t>& data, 
                                                const std::string& key) {
    if (data.size() < AES_BLOCK_SIZE) {
        return {};
    }
    
    // Extract IV from beginning
    std::vector<uint8_t> iv(data.begin(), data.begin() + AES_BLOCK_SIZE);
    std::vector<uint8_t> cipherText(data.begin() + AES_BLOCK_SIZE, data.end());
    
    // Create cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    // Initialize cipher
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                          reinterpret_cast<const unsigned char*>(key.substr(0, 32).c_str()), 
                          iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    // Decrypt
    std::vector<uint8_t> result(cipherText.size() + AES_BLOCK_SIZE);
    int len;
    int totalLen = 0;
    
    if (EVP_DecryptUpdate(ctx, result.data(), &len, cipherText.data(), cipherText.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen = len;
    
    if (EVP_DecryptFinal_ex(ctx, result.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;
    
    // Resize to actual result size
    result.resize(totalLen);
    
    // Unpad the result
    result = unpadData(result);
    
    EVP_CIPHER_CTX_free(ctx);
    return result;
}

std::string SecurityManager::hashData(const std::vector<uint8_t>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}