#include "HandshakeProtocol.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include "Crypto.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace SentinelFS {

HandshakeProtocol::HandshakeProtocol(const std::string& localPeerId,
                                     const std::string& sessionCode,
                                     bool encryptionEnabled)
    : localPeerId_(localPeerId)
    , sessionCode_(sessionCode)
    , encryptionEnabled_(encryptionEnabled)
{
}

HandshakeProtocol::HandshakeResult 
HandshakeProtocol::performClientHandshake(int socket) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    HandshakeResult result;
    result.success = false;
    
    logger.log(LogLevel::DEBUG, "Starting client handshake", "HandshakeProtocol");
    
    auto clientNonce = generateNonce();
    // Send HELLO message with nonce
    std::string hello = createHelloMessage(clientNonce);
    if (!sendMessage(socket, hello)) {
        result.errorMessage = "Failed to send HELLO message";
        logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }
    
    // Wait for WELCOME or REJECT
    std::string response = receiveMessage(socket);
    if (response.empty()) {
        result.errorMessage = "No response from server";
        logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }
    
    if (response.find("SENTINEL_REJECT|") == 0) {
        result.errorMessage = response.substr(16);
        logger.log(LogLevel::WARN, "Connection rejected: " + result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    // Server may reply with challenge for hardened handshake.
    if (response.find("SENTINEL_CHALLENGE|") == 0) {
        std::string remotePeerId;
        std::vector<uint8_t> echoedClientNonce;
        std::vector<uint8_t> serverNonce;
        if (!parseChallengeMessage(response, remotePeerId, echoedClientNonce, serverNonce)) {
            result.errorMessage = "Malformed CHALLENGE message";
            logger.log(LogLevel::WARN, result.errorMessage, "HandshakeProtocol");
            metrics.incrementSyncErrors();
            return result;
        }

        if (clientNonce != echoedClientNonce) {
            result.errorMessage = "Challenge nonce mismatch";
            logger.log(LogLevel::WARN, result.errorMessage, "HandshakeProtocol");
            metrics.incrementSyncErrors();
            return result;
        }

        if (sessionCode_.empty()) {
            result.errorMessage = "Session code required for secure handshake";
            logger.log(LogLevel::WARN, result.errorMessage, "HandshakeProtocol");
            metrics.incrementSyncErrors();
            return result;
        }

        std::string digest = computeAuthDigest(clientNonce, serverNonce, remotePeerId, "CLIENT_AUTH");
        if (digest.empty()) {
            result.errorMessage = "Failed to compute handshake digest";
            logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
            metrics.incrementSyncErrors();
            return result;
        }

        std::string authMessage = createAuthMessage(digest);
        if (!sendMessage(socket, authMessage)) {
            result.errorMessage = "Failed to send AUTH message";
            logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
            metrics.incrementSyncErrors();
            return result;
        }

        response = receiveMessage(socket);
        if (response.empty()) {
            result.errorMessage = "Server closed connection before WELCOME";
            logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
            metrics.incrementSyncErrors();
            return result;
        }
    }

    if (response.find("SENTINEL_REJECT|") == 0) {
        result.errorMessage = response.substr(16);
        logger.log(LogLevel::WARN, "Connection rejected: " + result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    if (response.find("ERROR|INVALID_SESSION_CODE") == 0) {
        result.errorMessage = "Invalid session code";
        logger.log(LogLevel::WARN, "Connection rejected: Invalid session code! Make sure all peers use the same session code.", "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    if (response.find("SENTINEL_WELCOME|") != 0) {
        result.errorMessage = "Invalid handshake response: " + response;
        logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    size_t firstPipe = response.find('|');
    size_t secondPipe = response.find('|', firstPipe + 1);
    
    if (secondPipe != std::string::npos) {
        result.peerId = response.substr(secondPipe + 1);
    } else {
        result.peerId = response.substr(17);
    }
    
    result.success = true;
    logger.log(LogLevel::INFO, "Handshake successful with peer: " + result.peerId, "HandshakeProtocol");
    metrics.incrementConnections();
    
    return result;
}

HandshakeProtocol::HandshakeResult 
HandshakeProtocol::performServerHandshake(int socket) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    HandshakeResult result;
    result.success = false;
    
    logger.log(LogLevel::DEBUG, "Starting server handshake", "HandshakeProtocol");
    
    std::string hello = receiveMessage(socket);
    if (hello.empty()) {
        result.errorMessage = "No HELLO message received";
        logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }
    
    std::string remotePeerId;
    std::vector<uint8_t> clientNonce;
    std::string remoteSessionCode;
    bool legacyFormat = false;
    if (!parseHelloMessage(hello, remotePeerId, clientNonce, remoteSessionCode, legacyFormat)) {
        std::string reject = createRejectMessage("Malformed HELLO");
        sendMessage(socket, reject);
        result.errorMessage = "Invalid HELLO message";
        logger.log(LogLevel::WARN, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    if (!sessionCode_.empty() && sessionCode_ != remoteSessionCode) {
        std::string reject = createRejectMessage("Invalid session code");
        sendMessage(socket, reject);
        result.errorMessage = "Session code mismatch";
        logger.log(LogLevel::WARN, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    const bool canUseHardenedHandshake = !sessionCode_.empty() && !legacyFormat && !clientNonce.empty();

    if (!canUseHardenedHandshake) {
        std::string welcome = createWelcomeMessage();
        if (!sendMessage(socket, welcome)) {
            result.errorMessage = "Failed to send WELCOME message";
            logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
            metrics.incrementSyncErrors();
            return result;
        }

        result.success = true;
        result.peerId = remotePeerId;
        logger.log(LogLevel::INFO, "Server handshake (legacy) successful with peer: " + result.peerId, "HandshakeProtocol");
        metrics.incrementConnections();
        return result;
    }

    auto serverNonce = generateNonce();
    std::string challenge = createChallengeMessage(remotePeerId, clientNonce, serverNonce);
    if (!sendMessage(socket, challenge)) {
        result.errorMessage = "Failed to send CHALLENGE";
        logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    std::string authMessage = receiveMessage(socket);
    if (authMessage.empty()) {
        result.errorMessage = "Client did not respond to CHALLENGE";
        logger.log(LogLevel::WARN, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    if (authMessage.find("SENTINEL_AUTH|") != 0) {
        result.errorMessage = "Unexpected message instead of AUTH";
        logger.log(LogLevel::WARN, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    std::string authPeerId;
    std::string digest;
    if (!parseAuthMessage(authMessage, authPeerId, digest)) {
        result.errorMessage = "Malformed AUTH message";
        logger.log(LogLevel::WARN, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    if (authPeerId != remotePeerId) {
        result.errorMessage = "AUTH peer mismatch";
        logger.log(LogLevel::WARN, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    std::string expectedDigest = computeAuthDigest(clientNonce, serverNonce, remotePeerId, "CLIENT_AUTH");
    if (expectedDigest.empty() || expectedDigest != digest) {
        result.errorMessage = "Handshake authentication failed";
        logger.log(LogLevel::WARN, "Authentication failed - Expected: " + expectedDigest.substr(0, 16) + 
                   "..., Received: " + digest.substr(0, 16) + "...", "HandshakeProtocol");
        metrics.incrementSyncErrors();
        std::string reject = createRejectMessage("Authentication failed");
        sendMessage(socket, reject);
        return result;
    }

    std::string welcome = createWelcomeMessage();
    if (!sendMessage(socket, welcome)) {
        result.errorMessage = "Failed to send WELCOME message";
        logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }

    result.success = true;
    result.peerId = remotePeerId;
    logger.log(LogLevel::INFO, "Server handshake successful with peer: " + result.peerId, "HandshakeProtocol");
    metrics.incrementConnections();
    
    return result;
}

std::string HandshakeProtocol::createHelloMessage(const std::vector<uint8_t>& clientNonce) const {
    std::stringstream ss;
    ss << "SENTINEL_HELLO|" << PROTOCOL_VERSION << "|"
       << localPeerId_ << "|" << sessionCode_;
    if (!clientNonce.empty()) {
        ss << "|" << Crypto::toHex(clientNonce);
    }
    return ss.str();
}

std::string HandshakeProtocol::createChallengeMessage(const std::string& remotePeerId,
                                                       const std::vector<uint8_t>& clientNonce,
                                                       const std::vector<uint8_t>& serverNonce) const {
    std::stringstream ss;
    ss << "SENTINEL_CHALLENGE|" << PROTOCOL_VERSION << "|"
       << localPeerId_ << "|" << remotePeerId << "|"
       << Crypto::toHex(clientNonce) << "|" << Crypto::toHex(serverNonce);
    return ss.str();
}

std::string HandshakeProtocol::createWelcomeMessage() const {
    // Format: SENTINEL_WELCOME|VERSION|PEER_ID
    std::stringstream ss;
    ss << "SENTINEL_WELCOME|" << PROTOCOL_VERSION << "|" << localPeerId_;
    return ss.str();
}

std::string HandshakeProtocol::createAuthMessage(const std::string& digest) const {
    std::stringstream ss;
    ss << "SENTINEL_AUTH|" << PROTOCOL_VERSION << "|"
       << localPeerId_ << "|" << digest;
    return ss.str();
}

std::string HandshakeProtocol::createRejectMessage(const std::string& reason) const {
    // Format: SENTINEL_REJECT|REASON
    return "SENTINEL_REJECT|" + reason;
}

bool HandshakeProtocol::sendMessage(int socket, const std::string& message) {
    auto& logger = Logger::instance();
    
    ssize_t sent = send(socket, message.c_str(), message.length(), 0);
    if (sent < 0) {
        logger.log(LogLevel::ERROR, "Failed to send message: " + std::string(strerror(errno)), "HandshakeProtocol");
        return false;
    }
    logger.log(LogLevel::DEBUG, "Sent handshake message: " + message.substr(0, message.find('|')), "HandshakeProtocol");
    return true;
}

std::string HandshakeProtocol::receiveMessage(int socket, size_t maxSize) {
    auto& logger = Logger::instance();
    
    char buffer[maxSize];
    ssize_t len = recv(socket, buffer, maxSize - 1, 0);
    
    if (len <= 0) {
        if (len < 0) {
            logger.log(LogLevel::ERROR, "Failed to receive message: " + std::string(strerror(errno)), "HandshakeProtocol");
        } else {
            logger.log(LogLevel::DEBUG, "Connection closed while receiving message", "HandshakeProtocol");
        }
        return "";
    }
    
    buffer[len] = '\0';
    std::string msg(buffer);
    logger.log(LogLevel::DEBUG, "Received handshake message: " + msg.substr(0, msg.find('|')), "HandshakeProtocol");
    return msg;
}

bool HandshakeProtocol::parseHelloMessage(const std::string& message,
                                          std::string& remotePeerId,
                                          std::vector<uint8_t>& clientNonce,
                                          std::string& remoteSessionCode,
                                          bool& legacyFormat) {
    auto& logger = Logger::instance();
    clientNonce.clear();
    legacyFormat = true;

    if (message.find("SENTINEL_HELLO|") != 0) {
        logger.log(LogLevel::WARN, "Invalid HELLO prefix", "HandshakeProtocol");
        return false;
    }

    std::vector<std::string> parts;
    std::stringstream ss(message);
    std::string part;
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }

    if (parts.size() < 4) {
        logger.log(LogLevel::WARN, "Incomplete HELLO message", "HandshakeProtocol");
        return false;
    }

    remotePeerId = parts[2];
    remoteSessionCode = parts[3];

    if (parts.size() >= 5 && !parts[4].empty()) {
        try {
            clientNonce = Crypto::fromHex(parts[4]);
            legacyFormat = false;
        } catch (const std::exception& ex) {
            logger.log(LogLevel::WARN, std::string("Invalid client nonce: ") + ex.what(), "HandshakeProtocol");
            return false;
        }
    } else {
        legacyFormat = true;
    }

    logger.log(LogLevel::INFO, "Received HELLO from " + remotePeerId + " (version " + parts[1] + ")", "HandshakeProtocol");
    return true;
}

bool HandshakeProtocol::parseChallengeMessage(const std::string& message,
                                              std::string& remotePeerId,
                                              std::vector<uint8_t>& echoedClientNonce,
                                              std::vector<uint8_t>& serverNonce) {
    auto& logger = Logger::instance();
    echoedClientNonce.clear();
    serverNonce.clear();

    if (message.find("SENTINEL_CHALLENGE|") != 0) {
        logger.log(LogLevel::WARN, "Invalid CHALLENGE prefix", "HandshakeProtocol");
        return false;
    }

    std::vector<std::string> parts;
    std::stringstream ss(message);
    std::string part;
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }

    if (parts.size() < 6) {
        logger.log(LogLevel::WARN, "Incomplete CHALLENGE message", "HandshakeProtocol");
        return false;
    }

    remotePeerId = parts[2];
    const std::string& intendedClient = parts[3];
    if (intendedClient != localPeerId_) {
        logger.log(LogLevel::WARN, "CHALLENGE not intended for this peer", "HandshakeProtocol");
        return false;
    }

    try {
        echoedClientNonce = Crypto::fromHex(parts[4]);
        serverNonce = Crypto::fromHex(parts[5]);
    } catch (const std::exception& ex) {
        logger.log(LogLevel::WARN, std::string("Failed to parse CHALLENGE nonces: ") + ex.what(), "HandshakeProtocol");
        return false;
    }

    return true;
}

bool HandshakeProtocol::parseAuthMessage(const std::string& message,
                                         std::string& remotePeerId,
                                         std::string& digest) {
    auto& logger = Logger::instance();
    remotePeerId.clear();
    digest.clear();

    if (message.find("SENTINEL_AUTH|") != 0) {
        logger.log(LogLevel::WARN, "Invalid AUTH prefix", "HandshakeProtocol");
        return false;
    }

    std::vector<std::string> parts;
    std::stringstream ss(message);
    std::string part;
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }

    if (parts.size() < 4) {
        logger.log(LogLevel::WARN, "Incomplete AUTH message", "HandshakeProtocol");
        return false;
    }

    remotePeerId = parts[2];
    digest = parts[3];
    return true;
}

std::string HandshakeProtocol::computeAuthDigest(const std::vector<uint8_t>& clientNonce,
                                                 const std::vector<uint8_t>& serverNonce,
                                                 const std::string& remotePeerId,
                                                 const std::string& purpose) const {
    auto& logger = Logger::instance();
    if (sessionCode_.empty()) {
        logger.log(LogLevel::WARN, "Cannot compute digest without session code", "HandshakeProtocol");
        return "";
    }

    static const std::vector<uint8_t> handshakeSalt = {
        0x53, 0x46, 0x53, 0x5f, 0x48, 0x53, 0x48, 0x32
    };

    std::vector<uint8_t> key;
    try {
        key = Crypto::deriveKeyFromSessionCode(sessionCode_, handshakeSalt);
    } catch (const std::exception& ex) {
        logger.log(LogLevel::ERROR, std::string("Key derivation failed: ") + ex.what(), "HandshakeProtocol");
        return "";
    }

    std::string payload = purpose + "|" + localPeerId_ + "|" + remotePeerId + "|"
        + Crypto::toHex(clientNonce) + "|" + Crypto::toHex(serverNonce);
    std::vector<uint8_t> payloadBytes(payload.begin(), payload.end());

    logger.log(LogLevel::DEBUG, "Computing auth digest - Purpose: " + purpose + 
               ", Local: " + localPeerId_ + ", Remote: " + remotePeerId + 
               ", SessionCode: " + sessionCode_, "HandshakeProtocol");

    std::vector<uint8_t> digest;
    try {
        digest = Crypto::hmacSHA256(payloadBytes, key);
    } catch (const std::exception& ex) {
        logger.log(LogLevel::ERROR, std::string("HMAC computation failed: ") + ex.what(), "HandshakeProtocol");
        return "";
    }

    std::string result = Crypto::toHex(digest);
    logger.log(LogLevel::DEBUG, "Computed digest: " + result.substr(0, 16) + "...", "HandshakeProtocol");
    return result;
}

std::vector<uint8_t> HandshakeProtocol::generateNonce() const {
    try {
        return Crypto::generateKey();
    } catch (const std::exception&) {
        return {};
    }
}

} // namespace SentinelFS
