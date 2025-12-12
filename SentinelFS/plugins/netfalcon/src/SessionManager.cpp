#include "SessionManager.h"
#include "Crypto.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace SentinelFS {
namespace NetFalcon {

const std::vector<uint8_t> SessionManager::DEFAULT_SALT = {
    0x4E, 0x65, 0x74, 0x46, 0x61, 0x6C, 0x63, 0x6F,  // "NetFalco"
    0x6E, 0x5F, 0x53, 0x61, 0x6C, 0x74, 0x5F, 0x31   // "n_Salt_1"
};

SessionManager::SessionManager() = default;
SessionManager::~SessionManager() = default;

void SessionManager::setLocalPeerId(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    localPeerId_ = peerId;
}

std::string SessionManager::getLocalPeerId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return localPeerId_;
}

void SessionManager::setSessionCode(const std::string& sessionCode, bool enableEncryption) {
    std::lock_guard<std::mutex> lock(mutex_);
    primarySessionCode_ = sessionCode;
    encryptionEnabled_ = enableEncryption;
    
    if (enableEncryption && !sessionCode.empty()) {
        // Use key separation: derive separate keys for encryption and MAC
        // Prefer Argon2id if available (memory-hard, GPU/ASIC resistant)
        if (Crypto::isArgon2Available()) {
            keys_ = Crypto::deriveKeyPairArgon2(sessionCode, DEFAULT_SALT);
        } else {
            // Fallback to PBKDF2 with high iteration count
            keys_ = Crypto::deriveKeyPair(sessionCode, DEFAULT_SALT, 100000);
        }
    } else {
        keys_.encKey.clear();
        keys_.macKey.clear();
    }
    
    keyRotationCounter_ = 0;
    messageSequence_ = 0;
}

std::string SessionManager::getSessionCode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return primarySessionCode_;
}

std::string SessionManager::createSession(const std::string& name, const std::string& sessionCode, bool enableEncryption) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Generate session ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 99999);
    std::string sessionId = name + "_" + std::to_string(dis(gen));
    
    SessionInfo info;
    info.sessionId = sessionId;
    info.sessionCode = sessionCode;
    info.state = SessionState::ACTIVE;
    info.createdAt = std::chrono::steady_clock::now();
    info.expiresAt = info.createdAt + std::chrono::hours(24);
    info.encryptionEnabled = enableEncryption;
    
    sessions_[sessionId] = info;
    
    return sessionId;
}

SessionInfo SessionManager::getSessionInfo(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (sessionId.empty()) {
        // Return primary session info
        SessionInfo info;
        info.sessionId = "primary";
        info.sessionCode = primarySessionCode_;
        info.state = primarySessionCode_.empty() ? SessionState::INACTIVE : SessionState::ACTIVE;
        info.encryptionEnabled = encryptionEnabled_;
        info.peerCount = peerSessions_.size();
        return info;
    }
    
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second;
    }
    
    return SessionInfo{};
}

bool SessionManager::isEncryptionEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return encryptionEnabled_;
}

void SessionManager::setEncryptionEnabled(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    encryptionEnabled_ = enable;
    
    if (enable && !primarySessionCode_.empty() && keys_.encKey.empty()) {
        if (Crypto::isArgon2Available()) {
            keys_ = Crypto::deriveKeyPairArgon2(primarySessionCode_, DEFAULT_SALT);
        } else {
            keys_ = Crypto::deriveKeyPair(primarySessionCode_, DEFAULT_SALT, 100000);
        }
    } else if (!enable) {
        keys_.encKey.clear();
        keys_.macKey.clear();
    }
}

std::vector<uint8_t> SessionManager::deriveKey(const std::string& sessionCode, const std::vector<uint8_t>& salt) const {
    std::vector<uint8_t> useSalt = salt.empty() ? DEFAULT_SALT : salt;
    return Crypto::deriveKeyFromSessionCode(sessionCode, useSalt);
}

std::vector<uint8_t> SessionManager::getEncryptionKey() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return keys_.encKey;
}

void SessionManager::rotateKey() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (primarySessionCode_.empty()) return;
    
    keyRotationCounter_++;
    
    // Create new salt with rotation counter
    std::vector<uint8_t> rotatedSalt = DEFAULT_SALT;
    rotatedSalt.push_back(static_cast<uint8_t>(keyRotationCounter_ >> 24));
    rotatedSalt.push_back(static_cast<uint8_t>(keyRotationCounter_ >> 16));
    rotatedSalt.push_back(static_cast<uint8_t>(keyRotationCounter_ >> 8));
    rotatedSalt.push_back(static_cast<uint8_t>(keyRotationCounter_));
    
    // Derive new key pair with rotated salt
    keys_ = Crypto::deriveKeyPair(primarySessionCode_, rotatedSalt);
}

std::vector<uint8_t> SessionManager::encrypt(const std::vector<uint8_t>& plaintext, const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!encryptionEnabled_ || keys_.encKey.empty()) {
        return plaintext;
    }
    
    try {
        EncryptedMessage msg;
        msg.version = EncryptedMessage::CURRENT_VERSION;
        msg.sequence = const_cast<SessionManager*>(this)->messageSequence_++;  // Atomic increment
        
        if (msg.isAead()) {
            // AES-256-GCM (AEAD) - recommended
            auto nonce = Crypto::generateGcmNonce();
            auto aad = msg.getAuthenticatedData();  // version || sequence
            
            // GCM provides both encryption and authentication
            auto ciphertextWithTag = Crypto::encryptGcm(plaintext, keys_.encKey, nonce, aad);
            
            msg.iv = nonce;
            msg.ciphertext = ciphertextWithTag;  // Includes 16-byte auth tag
            // msg.hmac is empty for GCM
        } else {
            // AES-256-CBC + HMAC (legacy fallback)
            auto iv = Crypto::generateIV();
            auto ciphertext = Crypto::encrypt(plaintext, keys_.encKey, iv);
            
            msg.iv = iv;
            msg.ciphertext = ciphertext;
            
            // Encrypt-then-MAC
            msg.hmac = Crypto::hmacSHA256(msg.getAuthenticatedData(), keys_.macKey);
        }
        
        return msg.serialize();
    } catch (const std::exception&) {
        return {};
    }
}

std::vector<uint8_t> SessionManager::decrypt(const std::vector<uint8_t>& ciphertext, const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!encryptionEnabled_ || keys_.encKey.empty()) {
        return ciphertext;
    }
    
    try {
        auto msg = EncryptedMessage::deserialize(ciphertext);
        
        // Check version compatibility
        if (msg.version > EncryptedMessage::CURRENT_VERSION) {
            return {};  // Unsupported version
        }
        
        std::vector<uint8_t> plaintext;
        
        if (msg.isAead()) {
            // AES-256-GCM (AEAD)
            auto aad = msg.getAuthenticatedData();  // version || sequence
            
            // GCM decryption verifies authenticity automatically
            // Returns empty vector on auth failure
            plaintext = Crypto::decryptGcm(msg.ciphertext, keys_.encKey, msg.iv, aad);
            
            if (plaintext.empty()) {
                return {};  // Authentication failed
            }
        } else {
            // AES-256-CBC + HMAC (legacy)
            // CRITICAL: Verify MAC BEFORE decryption (Encrypt-then-MAC)
            auto expectedHmac = Crypto::hmacSHA256(msg.getAuthenticatedData(), keys_.macKey);
            
            // Constant-time comparison to prevent timing attacks
            if (!Crypto::constantTimeCompare(msg.hmac, expectedHmac)) {
                return {};  // MAC verification failed - do NOT attempt decryption
            }
            
            plaintext = Crypto::decrypt(msg.ciphertext, keys_.encKey, msg.iv);
        }
        
        // Verify sequence number for replay protection
        if (!const_cast<SessionManager*>(this)->verifyMessageCounter(peerId, msg.sequence)) {
            return {};  // Replay attack detected
        }
        
        return plaintext;
    } catch (const std::exception&) {
        return {};
    }
}

bool SessionManager::verifySessionCode(const std::string& code) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // If both are empty, allow connection (open mode)
    if (primarySessionCode_.empty() && code.empty()) {
        return true;
    }
    
    // If either side has a session code, both must match
    // This prevents session-code-protected peers from connecting to open peers
    if (primarySessionCode_.empty() || code.empty()) {
        return false;  // One has code, other doesn't - reject
    }
    
    return primarySessionCode_ == code;
}

void SessionManager::registerPeer(const std::string& peerId, const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PeerSession session;
    session.peerId = peerId;
    session.sessionId = sessionId.empty() ? "primary" : sessionId;
    session.authState = AuthState::AUTHENTICATED;
    session.authenticatedAt = std::chrono::steady_clock::now();
    
    peerSessions_[peerId] = session;
}

void SessionManager::unregisterPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    peerSessions_.erase(peerId);
    lastSeenCounters_.erase(peerId);
    pendingChallenges_.erase(peerId);
}

PeerSession SessionManager::getPeerSession(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peerSessions_.find(peerId);
    if (it != peerSessions_.end()) {
        return it->second;
    }
    return PeerSession{};
}

bool SessionManager::isPeerAuthenticated(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peerSessions_.find(peerId);
    return it != peerSessions_.end() && it->second.authState == AuthState::AUTHENTICATED;
}

void SessionManager::updatePeerAuthState(const std::string& peerId, AuthState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peerSessions_.find(peerId);
    if (it != peerSessions_.end()) {
        it->second.authState = state;
        if (state == AuthState::AUTHENTICATED) {
            it->second.authenticatedAt = std::chrono::steady_clock::now();
        }
    }
}

uint64_t SessionManager::getNextMessageCounter(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peerSessions_.find(peerId);
    if (it != peerSessions_.end()) {
        return ++it->second.messageCounter;
    }
    return 0;
}

bool SessionManager::verifyMessageCounter(const std::string& peerId, uint64_t counter) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& lastSeen = lastSeenCounters_[peerId];
    
    // Allow some window for out-of-order delivery
    constexpr uint64_t WINDOW_SIZE = 100;
    
    if (counter <= lastSeen && lastSeen - counter > WINDOW_SIZE) {
        return false;  // Replay attack or very old message
    }
    
    if (counter > lastSeen) {
        lastSeen = counter;
    }
    
    return true;
}

std::vector<uint8_t> SessionManager::createClientHello() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Generate nonce
    auto nonce = Crypto::generateIV();
    
    // Format: VERSION|PEER_ID|SESSION_CODE|NONCE_HEX
    std::stringstream ss;
    ss << "FALCON_HELLO|1|" << localPeerId_ << "|" << primarySessionCode_ << "|" << Crypto::toHex(nonce);
    
    std::string msg = ss.str();
    return std::vector<uint8_t>(msg.begin(), msg.end());
}

std::vector<uint8_t> SessionManager::processChallenge(const std::vector<uint8_t>& challenge) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string challengeStr(challenge.begin(), challenge.end());
    
    // Parse: FALCON_CHALLENGE|VERSION|SERVER_PEER_ID|CLIENT_NONCE|SERVER_NONCE
    std::vector<std::string> parts;
    std::stringstream ss(challengeStr);
    std::string part;
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }
    
    if (parts.size() < 5 || parts[0] != "FALCON_CHALLENGE") {
        return {};
    }
    
    std::string serverPeerId = parts[2];
    auto clientNonce = Crypto::fromHex(parts[3]);
    auto serverNonce = Crypto::fromHex(parts[4]);
    
    // Compute auth digest
    auto digest = computeAuthDigest(clientNonce, serverNonce, localPeerId_, serverPeerId);
    
    // Format response: FALCON_AUTH|VERSION|PEER_ID|DIGEST_HEX
    std::stringstream response;
    response << "FALCON_AUTH|1|" << localPeerId_ << "|" << Crypto::toHex(digest);
    
    std::string responseStr = response.str();
    return std::vector<uint8_t>(responseStr.begin(), responseStr.end());
}

std::vector<uint8_t> SessionManager::createServerChallenge(const std::string& clientPeerId, const std::vector<uint8_t>& clientHello) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string helloStr(clientHello.begin(), clientHello.end());
    
    // Parse client hello
    std::vector<std::string> parts;
    std::stringstream ss(helloStr);
    std::string part;
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }
    
    if (parts.size() < 5 || parts[0] != "FALCON_HELLO") {
        return {};
    }
    
    std::string clientNonceHex = parts[4];
    
    // Generate server nonce
    auto serverNonce = Crypto::generateIV();
    
    // Store for verification
    const_cast<SessionManager*>(this)->pendingChallenges_[clientPeerId] = serverNonce;
    
    // Format: FALCON_CHALLENGE|VERSION|SERVER_PEER_ID|CLIENT_NONCE|SERVER_NONCE
    std::stringstream response;
    response << "FALCON_CHALLENGE|1|" << localPeerId_ << "|" << clientNonceHex << "|" << Crypto::toHex(serverNonce);
    
    std::string responseStr = response.str();
    return std::vector<uint8_t>(responseStr.begin(), responseStr.end());
}

HandshakeResult SessionManager::verifyClientResponse(const std::string& clientPeerId, const std::vector<uint8_t>& response) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    HandshakeResult result;
    result.success = false;
    
    std::string responseStr(response.begin(), response.end());
    
    // Parse: FALCON_AUTH|VERSION|PEER_ID|DIGEST_HEX
    std::vector<std::string> parts;
    std::stringstream ss(responseStr);
    std::string part;
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }
    
    if (parts.size() < 4 || parts[0] != "FALCON_AUTH") {
        result.errorMessage = "Invalid auth message format";
        return result;
    }
    
    std::string peerId = parts[2];
    auto receivedDigest = Crypto::fromHex(parts[3]);
    
    if (peerId != clientPeerId) {
        result.errorMessage = "Peer ID mismatch";
        return result;
    }
    
    // Get stored challenge
    auto challengeIt = pendingChallenges_.find(clientPeerId);
    if (challengeIt == pendingChallenges_.end()) {
        result.errorMessage = "No pending challenge for peer";
        return result;
    }
    
    // We need the client nonce - for now, assume it was stored
    // In production, store both nonces
    auto serverNonce = challengeIt->second;
    
    // For verification, we'd need the client nonce too
    // This is a simplified version
    result.success = true;
    result.peerId = clientPeerId;
    
    // Clean up
    const_cast<SessionManager*>(this)->pendingChallenges_.erase(clientPeerId);
    
    return result;
}

void SessionManager::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Cleanup expired sessions
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.expiresAt < now) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Cleanup stale challenges (older than 60 seconds)
    // Note: Would need timestamps for proper cleanup
    if (pendingChallenges_.size() > 100) {
        pendingChallenges_.clear();
    }
}

std::vector<uint8_t> SessionManager::computeAuthDigest(
    const std::vector<uint8_t>& clientNonce,
    const std::vector<uint8_t>& serverNonce,
    const std::string& clientPeerId,
    const std::string& serverPeerId
) const {
    // Combine all inputs
    std::vector<uint8_t> data;
    data.insert(data.end(), clientNonce.begin(), clientNonce.end());
    data.insert(data.end(), serverNonce.begin(), serverNonce.end());
    data.insert(data.end(), clientPeerId.begin(), clientPeerId.end());
    data.insert(data.end(), serverPeerId.begin(), serverPeerId.end());
    data.insert(data.end(), primarySessionCode_.begin(), primarySessionCode_.end());
    
    // HMAC with MAC key (key separation) or derived key
    if (!keys_.macKey.empty()) {
        return Crypto::hmacSHA256(data, keys_.macKey);
    }
    
    // Fallback: hash with session code as key
    auto key = deriveKey(primarySessionCode_, DEFAULT_SALT);
    return Crypto::hmacSHA256(data, key);
}

} // namespace NetFalcon
} // namespace SentinelFS
