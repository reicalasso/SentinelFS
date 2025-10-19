#include "transfer.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <fstream>
#include <openssl/ssl.h>
#include <openssl/err.h>

Transfer::Transfer() = default;

Transfer::~Transfer() {
    // Close all connections
    std::lock_guard<std::mutex> lock(connectionMutex);
    for (auto& pair : connectionPool) {
        if (pair.second.socket != -1) {
            close(pair.second.socket);
        }
    }
}

bool Transfer::sendDelta(const DeltaData& delta, const PeerInfo& peer) {
    std::cout << "Sending delta: " << delta.filePath 
              << " with " << delta.chunks.size() << " chunks to peer " << peer.id << std::endl;
    
    // Convert delta to raw data for transmission
    std::vector<uint8_t> data;
    
    // Add file path length and path
    uint32_t pathLen = static_cast<uint32_t>(delta.filePath.length());
    data.insert(data.end(), 
                reinterpret_cast<const uint8_t*>(&pathLen), 
                reinterpret_cast<const uint8_t*>(&pathLen) + sizeof(pathLen));
    data.insert(data.end(), delta.filePath.begin(), delta.filePath.end());
    
    // Add chunk count
    uint32_t chunkCount = static_cast<uint32_t>(delta.chunks.size());
    const uint8_t* chunkCountPtr = reinterpret_cast<const uint8_t*>(&chunkCount);
    data.insert(data.end(), chunkCountPtr, chunkCountPtr + sizeof(chunkCount));
    
    // Add each chunk
    for (const auto& chunk : delta.chunks) {
        const uint8_t* offsetPtr = reinterpret_cast<const uint8_t*>(&chunk.offset);
        data.insert(data.end(), offsetPtr, offsetPtr + sizeof(chunk.offset));
        
        const uint8_t* lengthPtr = reinterpret_cast<const uint8_t*>(&chunk.length);
        data.insert(data.end(), lengthPtr, lengthPtr + sizeof(chunk.length));
        
        data.insert(data.end(), chunk.data.begin(), chunk.data.end());
    }
    
    return sendRawData(data, peer);
}

bool Transfer::receiveDelta(DeltaData& delta, const PeerInfo& peer) {
    std::vector<uint8_t> data;
    if (!receiveRawData(data, peer)) {
        return false;
    }
    
    // Parse the received data back to DeltaData
    size_t offset = 0;
    
    // Read path length
    if (offset + sizeof(uint32_t) > data.size()) return false;
    uint32_t pathLen = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += sizeof(uint32_t);
    
    // Read path
    if (offset + pathLen > data.size()) return false;
    delta.filePath = std::string(reinterpret_cast<const char*>(data.data() + offset), pathLen);
    offset += pathLen;
    
    // Read chunk count
    if (offset + sizeof(uint32_t) > data.size()) return false;
    uint32_t chunkCount = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    offset += sizeof(uint32_t);
    
    // Read each chunk
    for (uint32_t i = 0; i < chunkCount; ++i) {
        if (offset + sizeof(uint64_t) + sizeof(uint64_t) > data.size()) return false;
        
        uint64_t chunkOffset = *reinterpret_cast<const uint64_t*>(data.data() + offset);
        offset += sizeof(uint64_t);
        
        uint64_t chunkLength = *reinterpret_cast<const uint64_t*>(data.data() + offset);
        offset += sizeof(uint64_t);
        
        if (offset + chunkLength > data.size()) return false;
        
        DeltaChunk chunk(chunkOffset, chunkLength);
        chunk.data.resize(chunkLength);
        std::copy(data.begin() + offset, data.begin() + offset + chunkLength, chunk.data.begin());
        offset += chunkLength;
        
        delta.chunks.push_back(chunk);
    }
    
    return true;
}

bool Transfer::sendFile(const std::string& filePath, const PeerInfo& peer) {
    std::cout << "Sending file: " << filePath << " to peer " << peer.id << std::endl;
    
    // Open file and send it
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        return false;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Send file info (size and name)
    std::vector<uint8_t> fileInfo;
    uint32_t pathLen = static_cast<uint32_t>(filePath.length());
    fileInfo.insert(fileInfo.end(), 
                    reinterpret_cast<const uint8_t*>(&pathLen), 
                    reinterpret_cast<const uint8_t*>(&pathLen) + sizeof(pathLen));
    fileInfo.insert(fileInfo.end(), filePath.begin(), filePath.end());
    
    fileInfo.insert(fileInfo.end(), 
                    reinterpret_cast<const uint8_t*>(&fileSize), 
                    reinterpret_cast<const uint8_t*>(&fileSize) + sizeof(fileSize));
    
    // Send file info first
    if (!sendRawData(fileInfo, peer)) {
        return false;
    }
    
    // Send file contents in chunks
    const size_t chunkSize = 64 * 1024; // 64KB chunks
    std::vector<uint8_t> buffer(chunkSize);
    
    while (file.read(reinterpret_cast<char*>(buffer.data()), chunkSize) || file.gcount() > 0) {
        size_t bytesRead = file.gcount();
        std::vector<uint8_t> chunk(buffer.begin(), buffer.begin() + bytesRead);
        
        if (!sendRawData(chunk, peer)) {
            file.close();
            return false;
        }
    }
    
    file.close();
    return true;
}

bool Transfer::receiveFile(const std::string& filePath, const PeerInfo& peer) {
    std::cout << "Receiving file: " << filePath << " from peer " << peer.id << std::endl;
    
    // First receive file info
    std::vector<uint8_t> fileInfo;
    if (!receiveRawData(fileInfo, peer)) {
        return false;
    }
    
    size_t offset = 0;
    
    // Read path length and path (should match expected path)
    if (offset + sizeof(uint32_t) > fileInfo.size()) return false;
    uint32_t pathLen = *reinterpret_cast<const uint32_t*>(fileInfo.data() + offset);
    offset += sizeof(uint32_t);
    
    if (offset + pathLen > fileInfo.size()) return false;
    std::string receivedPath = std::string(reinterpret_cast<const char*>(fileInfo.data() + offset), pathLen);
    offset += pathLen;
    
    // Read file size
    if (offset + sizeof(size_t) > fileInfo.size()) return false;
    size_t fileSize = *reinterpret_cast<const size_t*>(fileInfo.data() + offset);
    
    // Open file for writing
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Error creating file: " << filePath << std::endl;
        return false;
    }
    
    // Receive file contents
    size_t totalReceived = 0;
    while (totalReceived < fileSize) {
        std::vector<uint8_t> chunk;
        if (!receiveRawData(chunk, peer)) {
            file.close();
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
        totalReceived += chunk.size();
    }
    
    file.close();
    return true;
}

bool Transfer::broadcastDelta(const DeltaData& delta, const std::vector<PeerInfo>& peers) {
    bool allSuccessful = true;
    
    for (const auto& peer : peers) {
        if (!sendDelta(delta, peer)) {
            allSuccessful = false;
            std::cerr << "Failed to send delta to peer: " << peer.id << std::endl;
        }
    }
    
    return allSuccessful;
}

// Connection management methods
bool Transfer::establishConnection(const PeerInfo& peer) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    
    std::string peerKey = peer.address + ":" + std::to_string(peer.port);
    
    // Check if connection already exists and is active
    auto it = connectionPool.find(peerKey);
    if (it != connectionPool.end() && it->second.connected) {
        // Update last used time
        it->second.lastUsed = std::chrono::steady_clock::now();
        return true;
    }
    
    // Create new connection
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return false;
    }
    
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(peer.port);
    
    if (inet_pton(AF_INET, peer.address.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << peer.address << std::endl;
        close(sock);
        return false;
    }
    
    // Connect to peer
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed to " << peer.address << ":" << peer.port << std::endl;
        close(sock);
        return false;
    }
    
    // Update or create connection in pool
    Connection conn;
    conn.socket = sock;
    conn.addr = serverAddr;
    conn.lastUsed = std::chrono::steady_clock::now();
    conn.connected = true;
    conn.ssl = nullptr;  // Initialize SSL to nullptr initially
    
    connectionPool[peerKey] = conn;
    
    std::cout << "Connected to peer: " << peer.id << " at " << peer.address << ":" << peer.port << std::endl;
    return true;
}

void Transfer::closeConnection(const PeerInfo& peer) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    
    std::string peerKey = peer.address + ":" + std::to_string(peer.port);
    auto it = connectionPool.find(peerKey);
    
    if (it != connectionPool.end()) {
        if (it->second.socket != -1) {
            close(it->second.socket);
        }
        connectionPool.erase(it);
    }
}

void Transfer::cleanupConnections() {
    std::lock_guard<std::mutex> lock(connectionMutex);
    auto now = std::chrono::steady_clock::now();
    
    // Remove connections that haven't been used for 5 minutes
    for (auto it = connectionPool.begin(); it != connectionPool.end(); ) {
        auto timeSinceLastUse = std::chrono::duration_cast<std::chrono::minutes>(
            now - it->second.lastUsed);
        
        if (timeSinceLastUse.count() > 5) {
            if (it->second.socket != -1) {
                close(it->second.socket);
            }
            it = connectionPool.erase(it);
        } else {
            ++it;
        }
    }
}

bool Transfer::sendRawData(const std::vector<uint8_t>& data, const PeerInfo& peer) {
    int sock = getOrCreateSocket(peer);
    if (sock < 0) {
        return false;
    }
    
    // Get connection info to check for TLS
    std::lock_guard<std::mutex> lock(connectionMutex);
    std::string peerKey = peer.address + ":" + std::to_string(peer.port);
    auto it = connectionPool.find(peerKey);
    if (it == connectionPool.end()) {
        return false;
    }
    
    // Send data length first
    uint64_t dataLen = static_cast<uint64_t>(data.size());
    bool success;
    if (it->second.ssl != nullptr) {
        success = sendSecureData(sock, &dataLen, sizeof(dataLen));
    } else {
        success = sendWithRetry(sock, &dataLen, sizeof(dataLen));
    }
    
    if (!success) {
        return false;
    }
    
    // Send actual data
    if (it->second.ssl != nullptr) {
        success = sendSecureData(sock, data.data(), data.size());
    } else {
        success = sendWithRetry(sock, data.data(), data.size());
    }
    
    return success;
}

bool Transfer::receiveRawData(std::vector<uint8_t>& data, const PeerInfo& peer) {
    int sock = getOrCreateSocket(peer);
    if (sock < 0) {
        return false;
    }
    
    // Get connection info to check for TLS
    std::lock_guard<std::mutex> lock(connectionMutex);
    std::string peerKey = peer.address + ":" + std::to_string(peer.port);
    auto it = connectionPool.find(peerKey);
    if (it == connectionPool.end()) {
        return false;
    }
    
    // Receive data length first
    uint64_t dataLen;
    bool success;
    if (it->second.ssl != nullptr) {
        success = receiveSecureData(sock, &dataLen, sizeof(dataLen));
    } else {
        success = receiveWithRetry(sock, &dataLen, sizeof(dataLen));
    }
    
    if (!success) {
        return false;
    }
    
    if (dataLen > 100 * 1024 * 1024) { // Limit to 100MB to prevent memory attacks
        std::cerr << "Data size too large: " << dataLen << " bytes" << std::endl;
        return false;
    }
    
    // Resize data vector and receive the actual data
    data.resize(dataLen);
    if (it->second.ssl != nullptr) {
        success = receiveSecureData(sock, data.data(), dataLen);
    } else {
        success = receiveWithRetry(sock, data.data(), dataLen);
    }
    
    return success;
}

// Helper methods
int Transfer::getOrCreateSocket(const PeerInfo& peer) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    
    std::string peerKey = peer.address + ":" + std::to_string(peer.port);
    
    // Look for existing connection
    auto it = connectionPool.find(peerKey);
    if (it != connectionPool.end() && it->second.connected) {
        // Update last used time
        it->second.lastUsed = std::chrono::steady_clock::now();
        return it->second.socket;
    }
    
    // Establish new connection if needed
    if (establishConnection(peer)) {
        it = connectionPool.find(peerKey);
        if (it != connectionPool.end()) {
            return it->second.socket;
        }
    }
    
    return -1;
}

bool Transfer::sendWithRetry(int sock, const void* data, size_t length) {
    const char* ptr = static_cast<const char*>(data);
    size_t totalSent = 0;
    
    while (totalSent < length) {
        ssize_t sent = send(sock, ptr + totalSent, length - totalSent, 0);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            } else {
                std::cerr << "Send failed: " << strerror(errno) << std::endl;
                return false;
            }
        }
        totalSent += sent;
    }
    
    return true;
}

bool Transfer::receiveWithRetry(int sock, void* data, size_t length) {
    char* ptr = static_cast<char*>(data);
    size_t totalReceived = 0;
    
    while (totalReceived < length) {
        ssize_t received = recv(sock, ptr + totalReceived, length - totalReceived, 0);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            } else {
                std::cerr << "Receive failed: " << strerror(errno) << std::endl;
                return false;
            }
        } else if (received == 0) {
            std::cerr << "Connection closed by peer" << std::endl;
            return false;
        }
        
        totalReceived += received;
    }
    
    return true;
}

// TLS/SSL methods
bool Transfer::initializeTLS() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    return true;
}

bool Transfer::setupSecureConnection(int sock) {
    // For this implementation, we'll just return true to indicate that the connection 
    // can be secured (in a real implementation, we would establish an SSL connection)
    (void)sock; // unused parameter
    return true;
}

bool Transfer::sendSecureData(int sock, const void* data, size_t length) {
    // In a real implementation, this would send TLS-encrypted data
    // For now, we'll just send the data normally
    return sendWithRetry(sock, data, length);
}

bool Transfer::receiveSecureData(int sock, void* data, size_t length) {
    // In a real implementation, this would receive TLS-encrypted data
    // For now, we'll just receive the data normally
    return receiveWithRetry(sock, data, length);
}