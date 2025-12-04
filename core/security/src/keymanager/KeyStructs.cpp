/**
 * @file KeyStructs.cpp
 * @brief Implementation of key-related data structures (KeyInfo, SessionKey, IdentityKeyPair)
 */

#include "KeyManager.h"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

namespace SentinelFS {

//=====================================================
// KeyInfo Implementation
//=====================================================

bool KeyInfo::isExpired() const {
    if (expires.time_since_epoch().count() == 0) {
        return false;  // No expiration
    }
    return std::chrono::system_clock::now() > expires;
}

bool KeyInfo::isValid() const {
    return !compromised && !isExpired();
}

std::string KeyInfo::toString() const {
    std::stringstream ss;
    ss << "KeyInfo{id=" << keyId.substr(0, 8) << "..., type=" 
       << static_cast<int>(type) << ", algorithm=" << algorithm << "}";
    return ss.str();
}

//=====================================================
// SessionKey Implementation
//=====================================================

bool SessionKey::needsRotation() const {
    // Check time-based expiration
    if (std::chrono::system_clock::now() > expires) {
        return true;
    }
    
    // Check usage-based thresholds
    if (bytesEncrypted > MAX_BYTES) {
        return true;
    }
    
    if (messagesEncrypted > MAX_MESSAGES) {
        return true;
    }
    
    return false;
}

//=====================================================
// IdentityKeyPair Implementation
//=====================================================

std::string IdentityKeyPair::computeFingerprint() const {
    // Compute SHA256 of public key
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(publicKey.data(), publicKey.size(), hash);
    
    // Format as human-readable fingerprint (like SSH)
    // Example: "SHA256:Ab3C:D4eF:5678:..."
    std::stringstream ss;
    ss << "SHA256:";
    for (int i = 0; i < 16; i += 2) {
        if (i > 0) ss << ":";
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i]);
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i + 1]);
    }
    return ss.str();
}

} // namespace SentinelFS
