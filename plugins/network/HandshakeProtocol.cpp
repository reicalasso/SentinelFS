#include "HandshakeProtocol.h"
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
    HandshakeResult result;
    result.success = false;
    
    // Send HELLO message
    std::string hello = createHelloMessage();
    if (!sendMessage(socket, hello)) {
        result.errorMessage = "Failed to send HELLO message";
        return result;
    }
    
    // Wait for WELCOME or REJECT
    std::string response = receiveMessage(socket);
    if (response.empty()) {
        result.errorMessage = "No response from server";
        return result;
    }
    
    // Check for rejection
    if (response.find("SENTINEL_REJECT|") == 0) {
        result.errorMessage = response.substr(16); // Skip "SENTINEL_REJECT|"
        std::cerr << "🚫 Connection rejected: " << result.errorMessage << std::endl;
        return result;
    }
    
    // Handle old ERROR format for backward compatibility
    if (response.find("ERROR|INVALID_SESSION_CODE") == 0) {
        result.errorMessage = "Invalid session code";
        std::cerr << "🚫 Connection rejected: Invalid session code!" << std::endl;
        std::cerr << "   Make sure all peers use the same session code." << std::endl;
        return result;
    }
    
    // Validate WELCOME message
    if (response.find("SENTINEL_WELCOME|") != 0) {
        result.errorMessage = "Invalid handshake response: " + response;
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
    std::cout << "✓ Handshake successful with " << result.peerId << std::endl;
    
    return result;
}

HandshakeProtocol::HandshakeResult 
HandshakeProtocol::performServerHandshake(int socket) {
    HandshakeResult result;
    result.success = false;
    
    // Receive HELLO message
    std::string hello = receiveMessage(socket);
    if (hello.empty()) {
        result.errorMessage = "No HELLO message received";
        return result;
    }
    
    // Validate HELLO and extract peer ID
    std::string remotePeerId;
    if (!validateHelloMessage(hello, remotePeerId)) {
        // Send rejection
        std::string reject = createRejectMessage("Invalid session code");
        sendMessage(socket, reject);
        result.errorMessage = "Session code mismatch";
        return result;
    }
    
    // Send WELCOME
    std::string welcome = createWelcomeMessage();
    if (!sendMessage(socket, welcome)) {
        result.errorMessage = "Failed to send WELCOME message";
        return result;
    }
    
    result.success = true;
    result.peerId = remotePeerId;
    std::cout << "✓ Server handshake successful with " << result.peerId << std::endl;
    
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
    ssize_t sent = send(socket, message.c_str(), message.length(), 0);
    if (sent < 0) {
        std::cerr << "Failed to send message: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

std::string HandshakeProtocol::receiveMessage(int socket, size_t maxSize) {
    char buffer[maxSize];
    ssize_t len = recv(socket, buffer, maxSize - 1, 0);
    
    if (len <= 0) {
        if (len < 0) {
            std::cerr << "Failed to receive message: " << strerror(errno) << std::endl;
        }
        return "";
    }
    
    buffer[len] = '\0';
    return std::string(buffer);
}

bool HandshakeProtocol::validateHelloMessage(const std::string& message, 
                                             std::string& remotePeerId) {
    // Expected format: SENTINEL_HELLO|VERSION|PEER_ID|SESSION_CODE
    
    if (message.find("SENTINEL_HELLO|") != 0) {
        std::cerr << "Invalid HELLO message format" << std::endl;
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
        std::cerr << "Incomplete HELLO message" << std::endl;
        return false;
    }
    
    // Extract components
    std::string remoteVersion = parts[1];
    remotePeerId = parts[2];
    std::string remoteSessionCode = parts[3];
    
    std::cout << "Received HELLO from " << remotePeerId 
              << " (version " << remoteVersion << ")" << std::endl;
    
    // Validate session code if required
    if (!sessionCode_.empty() && sessionCode_ != remoteSessionCode) {
        std::cerr << "❌ Session code mismatch!" << std::endl;
        std::cerr << "   Expected: " << sessionCode_ << std::endl;
        std::cerr << "   Received: " << remoteSessionCode << std::endl;
        return false;
    }
    
    if (sessionCode_.empty()) {
        std::cout << "⚠️  No session code check (accepting all connections)" << std::endl;
    } else {
        std::cout << "✓ Session code verified" << std::endl;
    }
    
    return true;
}

} // namespace SentinelFS
