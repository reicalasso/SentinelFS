#include "security_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <regex>
#include <cctype>
#include <iostream>
#include <exception>
#include <openssl/bio.h>
#include <openssl/err.h>

// Static members initialization
std::unique_ptr<SecurityManager> SecurityManager::instance = nullptr;
std::mutex SecurityManager::instanceMutex;

namespace {
std::string rsaPublicKeyToPem(RSA* rsa) {
    if (!rsa) {
        return {};
    }

    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) {
        return {};
    }

    if (PEM_write_bio_RSAPublicKey(bio, rsa) != 1) {
        BIO_free(bio);
        return {};
    }

    BUF_MEM* mem = nullptr;
    BIO_get_mem_ptr(bio, &mem);
    std::string pem;
    if (mem && mem->data && mem->length > 0) {
        pem.assign(mem->data, mem->length);
    }
    BIO_free(bio);
    return pem;
}

std::string trim(const std::string& value) {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string toUpper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

AccessLevel parseAccessLevel(const std::string& raw) {
    const auto upper = toUpper(trim(raw));
    if (upper == "READ_WRITE") {
        return AccessLevel::READ_WRITE;
    }
    if (upper == "ADMIN") {
        return AccessLevel::ADMIN;
    }
    if (upper == "NONE") {
        return AccessLevel::NONE;
    }
    return AccessLevel::READ_ONLY;
}

std::string accessLevelToString(AccessLevel level) {
    switch (level) {
        case AccessLevel::READ_WRITE:
            return "READ_WRITE";
        case AccessLevel::ADMIN:
            return "ADMIN";
        case AccessLevel::NONE:
            return "NONE";
        case AccessLevel::READ_ONLY:
        default:
            return "READ_ONLY";
    }
}

std::string bytesToHex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    if (hex.size() % 2 != 0) {
        return bytes;
    }

    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        const std::string byteString = hex.substr(i, 2);
        unsigned int value = 0;
        std::stringstream ss;
        ss << std::hex << byteString;
        ss >> value;
        if (ss.fail()) {
            bytes.clear();
            return bytes;
        }
        bytes.push_back(static_cast<uint8_t>(value));
    }
    return bytes;
}

bool looksLikePem(const std::string& text) {
    return text.find("-----BEGIN") != std::string::npos;
}
}

SecurityManager::SecurityManager() 
    : privateKey(nullptr), publicKey(nullptr), 
      maxDataPerSecond(10 * 1024 * 1024),  // 10 MB/s default limit
      timeWindow(std::chrono::seconds(1)) {
    localStorageKey = generateRandomKey();
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
    
    if (privateKey && publicKey) {
        localPublicKeyPem = rsaPublicKeyToPem(publicKey);
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
    localPublicKeyPem = rsaPublicKeyToPem(publicKey);
    
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

    std::string publicMaterial;
    if (publicKey) {
        publicMaterial = rsaPublicKeyToPem(publicKey);
    }

    if (publicMaterial.empty()) {
        publicMaterial = peer.address;
    }

    std::vector<uint8_t> publicBytes(publicMaterial.begin(), publicMaterial.end());
    std::string publicFingerprint = hashData(publicBytes);
    
    certFile << "PeerID:" << peer.id << std::endl;
    certFile << "PublicKey:" << publicFingerprint << std::endl;
    certFile << "ValidFrom:" << validFrom << std::endl;
    certFile << "ValidUntil:" << validUntil << std::endl;
    certFile << "AccessLevel:" << accessLevelToString(AccessLevel::READ_WRITE) << std::endl;
    
    // Sign the certificate
    std::string certData = "PeerID:" + peer.id +
                           "PublicKey:" + publicFingerprint +
                           "ValidFrom:" + std::to_string(validFrom) +
                           "ValidUntil:" + std::to_string(validUntil) +
                           "AccessLevel:" + accessLevelToString(AccessLevel::READ_WRITE);
    std::vector<uint8_t> signature;
    if (signData(std::vector<uint8_t>(certData.begin(), certData.end()), signature)) {
        certFile << "Signature:" << bytesToHex(signature) << std::endl;
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
    sessionKeys.erase(cert.peerId);
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

bool SecurityManager::loadPeerCertificates(const std::string& directory) {
    namespace fs = std::filesystem;
    fs::path dirPath(directory);
    std::error_code ec;

    if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) {
        return false;
    }

    size_t loaded = 0;
    for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
        if (ec) {
            std::cerr << "[Security] Failed to iterate certificates in " << dirPath << ": " << ec.message() << std::endl;
            break;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        std::ifstream certFile(entry.path());
        if (!certFile) {
            std::cerr << "[Security] Unable to open certificate file " << entry.path() << std::endl;
            continue;
        }

        PeerCertificate cert;
        std::string line;
        std::string certData;
        std::vector<uint8_t> signatureBytes;

        while (std::getline(certFile, line)) {
            if (line.rfind("PeerID:", 0) == 0) {
                cert.peerId = trim(line.substr(7));
            } else if (line.rfind("PublicKey:", 0) == 0) {
                cert.publicKey = trim(line.substr(10));
            } else if (line.rfind("Signature:", 0) == 0) {
                auto sigText = trim(line.substr(10));
                cert.signature = sigText;
                signatureBytes = hexToBytes(sigText);
                continue;
            } else if (line.rfind("ValidFrom:", 0) == 0) {
                cert.validFrom = trim(line.substr(10));
            } else if (line.rfind("ValidUntil:", 0) == 0) {
                cert.validUntil = trim(line.substr(11));
            } else if (line.rfind("AccessLevel:", 0) == 0) {
                cert.accessLevel = parseAccessLevel(line.substr(12));
            }

            if (line.rfind("Signature:", 0) != 0) {
                certData += line;
            }
        }

        if (cert.peerId.empty() || cert.publicKey.empty()) {
            std::cerr << "[Security] Certificate " << entry.path() << " missing required fields" << std::endl;
            continue;
        }

        if (!cert.signature.empty() && signatureBytes.empty()) {
            std::cerr << "[Security] Certificate " << entry.path() << " has malformed signature encoding" << std::endl;
        }

        addPeerCertificate(cert);
        ++loaded;

        if (!signatureBytes.empty() && looksLikePem(cert.publicKey)) {
            std::vector<uint8_t> dataBytes(certData.begin(), certData.end());
            if (!verifySignature(dataBytes, signatureBytes, cert.publicKey)) {
                std::cerr << "[Security] Warning: signature verification failed for " << entry.path() << std::endl;
            }
        }
    }

    return loaded > 0;
}

std::vector<uint8_t> SecurityManager::encryptData(const std::vector<uint8_t>& data, 
                                                 const std::string& peerId) {
    auto sessionKey = getSessionKeyForPeer(peerId);
    return aesEncrypt(data, sessionKey);
}

std::vector<uint8_t> SecurityManager::decryptData(const std::vector<uint8_t>& encryptedData, 
                                                 const std::string& peerId) {
    auto sessionKey = getSessionKeyForPeer(peerId);
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
    
    return aesEncrypt(fileData, getLocalStorageKey());
}

bool SecurityManager::decryptFile(const std::vector<uint8_t>& encryptedData, 
                                 const std::string& outputPath) {
    auto decryptedData = aesDecrypt(encryptedData, getLocalStorageKey());
    if (decryptedData.empty()) {
        return false;
    }
    
    std::ofstream file(outputPath, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(decryptedData.data()), decryptedData.size());
    file.close();
    
    return true;
}

std::vector<uint8_t> SecurityManager::getSessionKeyForPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(securityMutex);

    auto it = sessionKeys.find(peerId);
    if (it != sessionKeys.end()) {
        return it->second;
    }

    std::string peerPublicKey;
    auto certIt = peerCertificates.find(peerId);
    if (certIt != peerCertificates.end()) {
        peerPublicKey = certIt->second.publicKey;
    }

    std::string normalizedLocalKey = localPublicKeyPem;
    if (normalizedLocalKey.empty() && publicKey) {
        normalizedLocalKey = rsaPublicKeyToPem(publicKey);
        localPublicKeyPem = normalizedLocalKey;
    }

    std::string material;
    if (!peerPublicKey.empty() && !normalizedLocalKey.empty()) {
        if (peerPublicKey < normalizedLocalKey) {
            material = peerPublicKey + normalizedLocalKey;
        } else {
            material = normalizedLocalKey + peerPublicKey;
        }
    }

    if (material.empty()) {
        material = peerId + normalizedLocalKey;
        if (material.empty()) {
            material = std::to_string(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
        }
    }

    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(material.data()), material.size(), digest);

    std::vector<uint8_t> key(digest, digest + SHA256_DIGEST_LENGTH);
    sessionKeys[peerId] = key;
    return key;
}

std::vector<uint8_t> SecurityManager::getLocalStorageKey() {
    std::lock_guard<std::mutex> lock(securityMutex);
    if (localStorageKey.size() != 32) {
        localStorageKey = generateRandomKey();
    }
    return localStorageKey;
}

std::vector<uint8_t> SecurityManager::generateRandomKey(size_t bytes) {
    std::vector<uint8_t> key(bytes, 0);
    if (bytes == 0) {
        return key;
    }

    if (RAND_bytes(key.data(), static_cast<int>(bytes)) != 1) {
        key.clear();
    }
    return key;
}

bool SecurityManager::authenticatePeer(const PeerInfo& peer) {
    // Check if peer has a valid certificate
    auto cert = getPeerCertificate(peer.id);
    if (cert.peerId.empty()) {
        return false; // No certificate for this peer
    }
    
    // Check certificate validity period
    try {
        auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!cert.validFrom.empty()) {
            auto validFrom = std::stoll(cert.validFrom);
            if (nowSeconds < validFrom) {
                return false;
            }
        }

        if (!cert.validUntil.empty()) {
            auto validUntil = std::stoll(cert.validUntil);
            if (nowSeconds > validUntil) {
                return false;
            }
        }
    } catch (const std::exception&) {
        return false;
    }

    if (cert.accessLevel == AccessLevel::NONE) {
        return false;
    }
    
    return true;
}

bool SecurityManager::signData(const std::vector<uint8_t>& data, std::vector<uint8_t>& signature) {
    if (!privateKey) {
        return false;
    }
    
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return false;
    }
    
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) {
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    if (EVP_PKEY_set1_RSA(pkey, privateKey) != 1) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    if (EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, pkey) <= 0) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(mdctx);
        return false;
    }

    if (EVP_DigestSignUpdate(mdctx, data.data(), data.size()) <= 0) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(mdctx);
        return false;
    }
    
    size_t siglen;
    if (EVP_DigestSignFinal(mdctx, nullptr, &siglen) <= 0) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(mdctx);
        return false;
    }
    
    signature.resize(siglen);
    if (EVP_DigestSignFinal(mdctx, signature.data(), &siglen) <= 0) {
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(mdctx);
        return false;
    }
    
    // Resize to actual signature length
    signature.resize(siglen);
    EVP_PKEY_free(pkey);
    EVP_MD_CTX_free(mdctx);
    
    return true;
}

bool SecurityManager::verifySignature(const std::vector<uint8_t>& data, 
                                     const std::vector<uint8_t>& signature, 
                                     const std::string& publicKeyStr) {
    // In a real implementation, we would use the provided public key
    // For now, we'll use our own public key as an example
    RSA* rsaKey = nullptr;
    if (!publicKeyStr.empty()) {
        BIO* bio = BIO_new_mem_buf(publicKeyStr.data(), static_cast<int>(publicKeyStr.size()));
        if (bio) {
            rsaKey = PEM_read_bio_RSAPublicKey(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
        }
    } else if (publicKey) {
        rsaKey = RSAPublicKey_dup(publicKey);
    }

    if (!rsaKey) {
        return false;
    }

    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) {
        RSA_free(rsaKey);
        return false;
    }

    if (EVP_PKEY_set1_RSA(pkey, rsaKey) != 1) {
        EVP_PKEY_free(pkey);
        RSA_free(rsaKey);
        return false;
    }

    RSA_free(rsaKey);

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        EVP_PKEY_free(pkey);
        return false;
    }

    if (EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, pkey) <= 0) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return false;
    }

    if (EVP_DigestVerifyUpdate(mdctx, data.data(), data.size()) <= 0) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return false;
    }

    int result = EVP_DigestVerifyFinal(mdctx, signature.data(), signature.size());
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    
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
            auto peerKey = it->first;
            it = lastActivity.erase(it);
            dataTransferred.erase(peerKey);
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
    if (padding == 0) {
        padding = blockSize;
    }

    std::vector<uint8_t> paddedData = data;
    paddedData.resize(data.size() + padding, static_cast<uint8_t>(padding));
    return paddedData;
}

std::vector<uint8_t> SecurityManager::unpadData(const std::vector<uint8_t>& data, bool& valid) {
    valid = false;
    if (data.empty()) {
        valid = true;
        return {};
    }

    uint8_t padding = data.back();
    if (padding == 0 || padding > AES_BLOCK_SIZE || padding > data.size()) {
        return {};
    }

    for (size_t i = data.size() - padding; i < data.size(); ++i) {
        if (data[i] != padding) {
            return {};
        }
    }

    valid = true;
    return std::vector<uint8_t>(data.begin(), data.end() - padding);
}

std::vector<uint8_t> SecurityManager::aesEncrypt(const std::vector<uint8_t>& data, 
                                                const std::vector<uint8_t>& key) {
    if (key.size() < 32) {
        return {};
    }

    auto paddedData = padData(data, AES_BLOCK_SIZE);

    unsigned char iv[AES_BLOCK_SIZE];
    if (RAND_bytes(iv, AES_BLOCK_SIZE) != 1) {
        return {};
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return {};
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    std::vector<uint8_t> cipherText(paddedData.size() + AES_BLOCK_SIZE);
    int len = 0;
    int totalLen = 0;

    if (EVP_EncryptUpdate(ctx, cipherText.data(), &len, paddedData.data(), static_cast<int>(paddedData.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen = len;

    if (EVP_EncryptFinal_ex(ctx, cipherText.data() + totalLen, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;

    cipherText.resize(totalLen);

    std::vector<uint8_t> finalResult;
    finalResult.reserve(AES_BLOCK_SIZE + cipherText.size());
    finalResult.insert(finalResult.end(), iv, iv + AES_BLOCK_SIZE);
    finalResult.insert(finalResult.end(), cipherText.begin(), cipherText.end());

    EVP_CIPHER_CTX_free(ctx);
    return finalResult;
}

std::vector<uint8_t> SecurityManager::aesDecrypt(const std::vector<uint8_t>& data, 
                                                const std::vector<uint8_t>& key) {
    if (data.size() < AES_BLOCK_SIZE || key.size() < 32) {
        return {};
    }
    
    // Extract IV from beginning
    std::vector<uint8_t> iv(data.begin(), data.begin() + AES_BLOCK_SIZE);
    std::vector<uint8_t> cipherText(data.begin() + AES_BLOCK_SIZE, data.end());
    
    // Create cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    // Initialize cipher
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                          key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    // Decrypt
    std::vector<uint8_t> result(cipherText.size() + AES_BLOCK_SIZE);
    int len = 0;
    int totalLen = 0;

    if (EVP_DecryptUpdate(ctx, result.data(), &len, cipherText.data(), static_cast<int>(cipherText.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen = len;
    
    if (EVP_DecryptFinal_ex(ctx, result.data() + totalLen, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += len;
    
    // Resize to actual result size
    result.resize(totalLen);
    
    // Unpad the result
    bool validPadding = false;
    auto unpadded = unpadData(result, validPadding);
    if (!validPadding) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    result = std::move(unpadded);
    
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