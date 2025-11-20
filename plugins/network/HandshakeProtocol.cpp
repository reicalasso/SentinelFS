#include "HandshakeProtocol.h"
#include "Logger.h"
#include "MetricsCollector.h"
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
    
    // Send HELLO message
    std::string hello = createHelloMessage();
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
    
    // Check for rejection
    if (response.find("SENTINEL_REJECT|") == 0) {
        result.errorMessage = response.substr(16); // Skip "SENTINEL_REJECT|"
        logger.log(LogLevel::WARN, "Connection rejected: " + result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }
    
    // Handle old ERROR format for backward compatibility
    if (response.find("ERROR|INVALID_SESSION_CODE") == 0) {
        result.errorMessage = "Invalid session code";
        logger.log(LogLevel::WARN, "Connection rejected: Invalid session code! Make sure all peers use the same session code.", "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }
    
    // Validate WELCOME message
    if (response.find("SENTINEL_WELCOME|") != 0) {
        result.errorMessage = "Invalid handshake response: " + response;
        logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }
    
    // Extract peer ID from: SENTINEL_WELCOME|VERSION|PEER_ID
    size_t firstPipe = response.find('|');
    size_t secondPipe = response.find('|', firstPipe + 1);
    
    if (secondPipe != std::string::npos) {
        result.peerId = response.substr(secondPipe + 1);
    } else {
        // Old format: SENTINEL_WELCOME|PEER_ID
        result.peerId = response.substr(17); // Length of "SENTINEL_WELCOME|"
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
    
    // Receive HELLO message
    std::string hello = receiveMessage(socket);
    if (hello.empty()) {
        result.errorMessage = "No HELLO message received";
        logger.log(LogLevel::ERROR, result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }
    
    // Validate HELLO and extract peer ID
    std::string remotePeerId;
    if (!validateHelloMessage(hello, remotePeerId)) {
        // Send rejection
        std::string reject = createRejectMessage("Invalid session code");
        sendMessage(socket, reject);
        result.errorMessage = "Session code mismatch";
        logger.log(LogLevel::WARN, "Handshake rejected: " + result.errorMessage, "HandshakeProtocol");
        metrics.incrementSyncErrors();
        return result;
    }
    
    // Send WELCOME
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

std::string HandshakeProtocol::createHelloMessage() const {
    // Format: SENTINEL_HELLO|VERSION|PEER_ID|SESSION_CODE
    std::stringstream ss;
    ss << "SENTINEL_HELLO|" << PROTOCOL_VERSION << "|" 
       << localPeerId_ << "|" << sessionCode_;
    return ss.str();
}

std::string HandshakeProtocol::createWelcomeMessage() const {
    // Format: SENTINEL_WELCOME|VERSION|PEER_ID
    std::stringstream ss;
    ss << "SENTINEL_WELCOME|" << PROTOCOL_VERSION << "|" << localPeerId_;
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

bool HandshakeProtocol::validateHelloMessage(const std::string& message, 
                                             std::string& remotePeerId) {
    auto& logger = Logger::instance();
    
    // Expected format: SENTINEL_HELLO|VERSION|PEER_ID|SESSION_CODE
    
    if (message.find("SENTINEL_HELLO|") != 0) {
        logger.log(LogLevel::WARN, "Invalid HELLO message format", "HandshakeProtocol");
        return false;
    }
    
    // Parse message
    std::vector<std::string> parts;
    std::stringstream ss(message);
    std::string part;
    
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }
    
    // Need at least: SENTINEL_HELLO, VERSION, PEER_ID, SESSION_CODE
    if (parts.size() < 4) {
        logger.log(LogLevel::WARN, "Incomplete HELLO message (only " + std::to_string(parts.size()) + " parts)", "HandshakeProtocol");
        return false;
    }
    
    // Extract components
    std::string remoteVersion = parts[1];
    remotePeerId = parts[2];
    std::string remoteSessionCode = parts[3];
    
    logger.log(LogLevel::INFO, "Received HELLO from " + remotePeerId + " (version " + remoteVersion + ")", "HandshakeProtocol");
    
    // Validate session code if required
    if (!sessionCode_.empty() && sessionCode_ != remoteSessionCode) {
        logger.log(LogLevel::WARN, "Session code mismatch! Expected: " + sessionCode_ + ", Received: " + remoteSessionCode, "HandshakeProtocol");
        return false;
    }
    
    if (sessionCode_.empty()) {
        logger.log(LogLevel::WARN, "No session code check - accepting all connections", "HandshakeProtocol");
    } else {
        logger.log(LogLevel::INFO, "Session code verified for peer: " + remotePeerId, "HandshakeProtocol");
    }
    
    return true;
}

} // namespace SentinelFS
