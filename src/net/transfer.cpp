#include "transfer.hpp"
#include "secure_transfer.hpp"
#include "../security/security_manager.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iterator>
#include <cerrno>
#include <ctime>
#include <limits>
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace {
constexpr uint64_t MAX_FRAME_SIZE = 100ULL * 1024 * 1024; // 100 MB safety cap

void appendUint32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

void appendUint64(std::vector<uint8_t>& buffer, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        buffer.push_back(static_cast<uint8_t>((value >> shift) & 0xFF));
    }
}

bool readUint32(const std::vector<uint8_t>& data, size_t& offset, uint32_t& value) {
    if (offset + 4 > data.size()) {
        return false;
    }

    value = 0;
    for (int i = 0; i < 4; ++i) {
        value = (value << 8) | data[offset++];
    }
    return true;
}

bool readUint64(const std::vector<uint8_t>& data, size_t& offset, uint64_t& value) {
    if (offset + 8 > data.size()) {
        return false;
    }

    value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | data[offset++];
    }
    return true;
}
}

Transfer::Transfer() {
    // Initialize SecureTransfer
    secureTransfer = std::make_unique<SecureTransfer>(*this);
    std::cout << "[Transfer] Initialized with security enabled by default" << std::endl;
}

Transfer::~Transfer() {
    stopListener();
    // Close all connections
    std::lock_guard<std::mutex> lock(connectionMutex);
    for (auto& pair : connectionPool) {
        if (pair.second.socket != -1) {
            close(pair.second.socket);
        }
    }
}

void Transfer::setDeltaHandler(DeltaHandler handler) {
    std::lock_guard<std::mutex> lock(handlerMutex);
    deltaHandler = std::move(handler);
}

void Transfer::setFileHandler(FileHandler handler) {
    std::lock_guard<std::mutex> lock(handlerMutex);
    fileHandler = std::move(handler);
}

bool Transfer::startListener(int port) {
    if (listenerRunning.load()) {
        return true;
    }

    listenerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenerSocket < 0) {
        std::cerr << "[Transfer] Failed to create listener socket" << std::endl;
        return false;
    }

    int reuse = 1;
    setsockopt(listenerSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenerSocket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[Transfer] Failed to bind listener socket on port " << port << std::endl;
        close(listenerSocket);
        listenerSocket = -1;
        return false;
    }

    if (listen(listenerSocket, 16) < 0) {
        std::cerr << "[Transfer] Failed to listen on port " << port << std::endl;
        close(listenerSocket);
        listenerSocket = -1;
        return false;
    }

    listenerRunning = true;
    listenerThread = std::thread(&Transfer::listenerLoop, this, port);
    std::cout << "[Transfer] Listening for inbound connections on port " << port << std::endl;
    return true;
}

void Transfer::stopListener() {
    if (!listenerRunning.exchange(false)) {
        return;
    }

    if (listenerSocket >= 0) {
        shutdown(listenerSocket, SHUT_RDWR);
        close(listenerSocket);
        listenerSocket = -1;
    }

    if (listenerThread.joinable()) {
        listenerThread.join();
    }
}

void Transfer::listenerLoop(int port) {
    (void)port; // port already bound, kept for logging if needed

    while (listenerRunning.load()) {
        struct sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        int clientSock = accept(listenerSocket, reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);
        if (clientSock < 0) {
            if (!listenerRunning.load()) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::string address = inet_ntoa(clientAddr.sin_addr);
        int remotePort = ntohs(clientAddr.sin_port);

        std::thread(&Transfer::handleClient, this, clientSock, address, remotePort).detach();
    }
}

void Transfer::handleClient(int clientSock, const std::string& address, int port) {
    std::string peerKey = address + ":" + std::to_string(port);

    PeerInfo peer;
    peer.id = peerKey;
    peer.address = address;
    peer.port = port;
    peer.active = true;
    peer.lastSeen = std::to_string(std::time(nullptr));

    {
        std::lock_guard<std::mutex> lock(connectionMutex);
        Connection conn;
        conn.socket = clientSock;
        conn.connected = true;
        conn.lastUsed = std::chrono::steady_clock::now();
        conn.addr.sin_family = AF_INET;
        conn.addr.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &conn.addr.sin_addr);
        connectionPool[peerKey] = conn;
    }

    while (listenerRunning.load()) {
        MessageType type;
        std::vector<uint8_t> payload;
        if (!receiveFramedData(clientSock, type, payload)) {
            break;
        }

        {
            std::lock_guard<std::mutex> lock(connectionMutex);
            auto it = connectionPool.find(peerKey);
            if (it != connectionPool.end()) {
                it->second.lastUsed = std::chrono::steady_clock::now();
            }
        }

        switch (type) {
            case MessageType::Delta: {
                DeltaData delta;
                if (decodeDelta(payload, delta)) {
                    DeltaHandler handlerCopy;
                    {
                        std::lock_guard<std::mutex> handlerLock(handlerMutex);
                        handlerCopy = deltaHandler;
                    }
                    if (handlerCopy) {
                        handlerCopy(peer, delta);
                    }
                }
                break;
            }
            case MessageType::File: {
                std::string remotePath;
                std::vector<uint8_t> bytes;
                if (decodeFilePayload(payload, remotePath, bytes)) {
                    FileHandler handlerCopy;
                    {
                        std::lock_guard<std::mutex> handlerLock(handlerMutex);
                        handlerCopy = fileHandler;
                    }
                    if (handlerCopy) {
                        handlerCopy(peer, remotePath, bytes);
                    }
                }
                break;
            }
            default:
                std::cerr << "[Transfer] Received unknown message type from " << peer.id << std::endl;
                break;
        }
    }

    close(clientSock);

    {
        std::lock_guard<std::mutex> lock(connectionMutex);
        connectionPool.erase(peerKey);
    }
}

bool Transfer::sendDelta(const DeltaData& delta, const PeerInfo& peer) {
    return sendDeltaInternal(delta, peer, true);
}

bool Transfer::sendDeltaPlain(const DeltaData& delta, const PeerInfo& peer) {
    return sendDeltaInternal(delta, peer, false);
}

bool Transfer::receiveDelta(DeltaData& delta, const PeerInfo& peer) {
    return receiveDeltaInternal(delta, peer, true);
}

bool Transfer::receiveDeltaPlain(DeltaData& delta, const PeerInfo& peer) {
    return receiveDeltaInternal(delta, peer, false);
}

bool Transfer::sendDeltaInternal(const DeltaData& delta, const PeerInfo& peer, bool allowSecure) {
    if (allowSecure && securityEnabled && securityManager && secureTransfer) {
        std::cout << "[Transfer] Using SECURE transfer for delta to " << peer.id << std::endl;
        secureTransfer->setSecurityManager(*securityManager);
        return secureTransfer->sendSecureDelta(delta, peer);
    }

    std::cout << "Sending delta: " << delta.filePath
              << " with " << delta.chunks.size() << " chunks to peer " << peer.id << std::endl;

    auto payload = encodeDelta(delta);
    if (payload.empty()) {
        std::cerr << "[Transfer] Failed to encode delta payload" << std::endl;
        return false;
    }
    return sendFramedData(MessageType::Delta, payload, peer);
}

bool Transfer::receiveDeltaInternal(DeltaData& delta, const PeerInfo& peer, bool allowSecure) {
    if (allowSecure && securityEnabled && securityManager && secureTransfer) {
        std::cout << "[Transfer] Using SECURE transfer for receiving delta from " << peer.id << std::endl;
        secureTransfer->setSecurityManager(*securityManager);
        return secureTransfer->receiveSecureDelta(delta, peer);
    }

    MessageType type{MessageType::Delta};
    std::vector<uint8_t> payload;
    if (!receiveFramedData(type, payload, peer)) {
        return false;
    }

    if (type != MessageType::Delta) {
        std::cerr << "Unexpected message type from peer " << peer.id << std::endl;
        return false;
    }

    delta = DeltaData();
    return decodeDelta(payload, delta);
}

bool Transfer::sendFileInternal(const std::string& filePath, const PeerInfo& peer, bool allowSecure) {
    if (allowSecure && securityEnabled && securityManager && secureTransfer) {
        std::cout << "[Transfer] Using SECURE transfer for file to " << peer.id << std::endl;
        secureTransfer->setSecurityManager(*securityManager);
        return secureTransfer->sendSecureFile(filePath, peer);
    }

    std::cout << "Sending file: " << filePath << " to peer " << peer.id << std::endl;

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        return false;
    }

    std::vector<uint8_t> contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    auto payload = encodeFilePayload(filePath, contents);
    if (payload.empty()) {
        std::cerr << "[Transfer] Failed to encode file payload" << std::endl;
        return false;
    }
    return sendFramedData(MessageType::File, payload, peer);
}

bool Transfer::receiveFileInternal(const std::string& filePath, const PeerInfo& peer, bool allowSecure) {
    if (allowSecure && securityEnabled && securityManager && secureTransfer) {
        std::cout << "[Transfer] Using SECURE transfer for receiving file from " << peer.id << std::endl;
        secureTransfer->setSecurityManager(*securityManager);
        return secureTransfer->receiveSecureFile(filePath, peer);
    }

    std::cout << "Receiving file: " << filePath << " from peer " << peer.id << std::endl;

    MessageType type{MessageType::File};
    std::vector<uint8_t> payload;
    if (!receiveFramedData(type, payload, peer)) {
        return false;
    }

    if (type != MessageType::File) {
        std::cerr << "Unexpected message type while receiving file from peer " << peer.id << std::endl;
        return false;
    }

    std::string remotePath;
    std::vector<uint8_t> contents;
    if (!decodeFilePayload(payload, remotePath, contents)) {
        return false;
    }

    const std::string& targetPath = filePath.empty() ? remotePath : filePath;
    std::ofstream file(targetPath, std::ios::binary);
    if (!file) {
        std::cerr << "Error creating file: " << targetPath << std::endl;
        return false;
    }
    file.write(reinterpret_cast<const char*>(contents.data()), static_cast<std::streamsize>(contents.size()));
    file.close();
    return true;
}

bool Transfer::sendFile(const std::string& filePath, const PeerInfo& peer) {
    return sendFileInternal(filePath, peer, true);
}

bool Transfer::sendFilePlain(const std::string& filePath, const PeerInfo& peer) {
    return sendFileInternal(filePath, peer, false);
}

bool Transfer::receiveFile(const std::string& filePath, const PeerInfo& peer) {
    return receiveFileInternal(filePath, peer, true);
}

bool Transfer::receiveFilePlain(const std::string& filePath, const PeerInfo& peer) {
    return receiveFileInternal(filePath, peer, false);
}

bool Transfer::broadcastDelta(const DeltaData& delta, const std::vector<PeerInfo>& peers) {
    // If security is enabled, use SecureTransfer broadcast
    if (securityEnabled && securityManager && secureTransfer) {
        std::cout << "[Transfer] Using SECURE broadcast for delta to " << peers.size() << " peers" << std::endl;
        secureTransfer->setSecurityManager(*securityManager);
        return secureTransfer->broadcastSecureDelta(delta, peers);
    }
    
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

bool Transfer::sendFramedData(MessageType type, const std::vector<uint8_t>& data, const PeerInfo& peer) {
    int sock = getOrCreateSocket(peer);
    if (sock < 0) {
        return false;
    }

    bool useTLS = false;
    {
        std::lock_guard<std::mutex> lock(connectionMutex);
        auto it = connectionPool.find(peer.address + ":" + std::to_string(peer.port));
        if (it == connectionPool.end()) {
            return false;
        }
        useTLS = it->second.ssl != nullptr;
    }

    uint8_t typeByte = static_cast<uint8_t>(type);
    uint64_t dataLen = static_cast<uint64_t>(data.size());

    auto sendFn = useTLS ? &Transfer::sendSecureData : &Transfer::sendWithRetry;

    if (!(this->*sendFn)(sock, &typeByte, sizeof(typeByte))) {
        return false;
    }

    if (!(this->*sendFn)(sock, &dataLen, sizeof(dataLen))) {
        return false;
    }

    if (dataLen > 0 && !(this->*sendFn)(sock, data.data(), data.size())) {
        return false;
    }

    return true;
}

bool Transfer::receiveFramedData(MessageType& type, std::vector<uint8_t>& data, const PeerInfo& peer) {
    int sock = getOrCreateSocket(peer);
    if (sock < 0) {
        return false;
    }

    bool useTLS = false;
    {
        std::lock_guard<std::mutex> lock(connectionMutex);
        auto it = connectionPool.find(peer.address + ":" + std::to_string(peer.port));
        if (it == connectionPool.end()) {
            return false;
        }
        useTLS = it->second.ssl != nullptr;
    }

    uint8_t typeByte = 0;
    auto recvFn = useTLS ? &Transfer::receiveSecureData : &Transfer::receiveWithRetry;

    if (!(this->*recvFn)(sock, &typeByte, sizeof(typeByte))) {
        return false;
    }

    type = static_cast<MessageType>(typeByte);

    uint64_t dataLen = 0;
    if (!(this->*recvFn)(sock, &dataLen, sizeof(dataLen))) {
        return false;
    }

    if (dataLen > MAX_FRAME_SIZE) {
        std::cerr << "Frame size too large from peer " << peer.id << std::endl;
        return false;
    }

    data.resize(dataLen);
    if (dataLen > 0 && !(this->*recvFn)(sock, data.data(), data.size())) {
        return false;
    }

    return true;
}

bool Transfer::receiveFramedData(int sock, MessageType& type, std::vector<uint8_t>& data) {
    uint8_t typeByte = 0;
    if (!receiveWithRetry(sock, &typeByte, sizeof(typeByte))) {
        return false;
    }

    type = static_cast<MessageType>(typeByte);

    uint64_t dataLen = 0;
    if (!receiveWithRetry(sock, &dataLen, sizeof(dataLen))) {
        return false;
    }

    if (dataLen > MAX_FRAME_SIZE) {
        std::cerr << "Frame size too large on inbound socket" << std::endl;
        return false;
    }

    data.resize(dataLen);
    if (dataLen > 0 && !receiveWithRetry(sock, data.data(), data.size())) {
        return false;
    }

    return true;
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

// Static helpers for framing payloads
std::vector<uint8_t> Transfer::encodeDelta(const DeltaData& delta) {
    std::vector<uint8_t> buffer;

    if (delta.filePath.size() > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "[Transfer] Delta path too long to encode" << std::endl;
        return {};
    }

    appendUint32(buffer, static_cast<uint32_t>(delta.filePath.size()));
    buffer.insert(buffer.end(), delta.filePath.begin(), delta.filePath.end());
    buffer.push_back(delta.isCompressed ? 1 : 0);

    if (delta.oldHash.size() > std::numeric_limits<uint32_t>::max() ||
        delta.newHash.size() > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "[Transfer] Delta hash too long to encode" << std::endl;
        return {};
    }

    appendUint32(buffer, static_cast<uint32_t>(delta.oldHash.size()));
    buffer.insert(buffer.end(), delta.oldHash.begin(), delta.oldHash.end());
    appendUint32(buffer, static_cast<uint32_t>(delta.newHash.size()));
    buffer.insert(buffer.end(), delta.newHash.begin(), delta.newHash.end());

    if (delta.chunks.size() > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "[Transfer] Too many delta chunks to encode" << std::endl;
        return {};
    }

    appendUint32(buffer, static_cast<uint32_t>(delta.chunks.size()));

    for (const auto& chunk : delta.chunks) {
        appendUint64(buffer, chunk.offset);
        appendUint64(buffer, chunk.length);

        if (chunk.checksum.size() > std::numeric_limits<uint32_t>::max()) {
            std::cerr << "[Transfer] Chunk checksum too long" << std::endl;
            return {};
        }

        appendUint32(buffer, static_cast<uint32_t>(chunk.checksum.size()));
        buffer.insert(buffer.end(), chunk.checksum.begin(), chunk.checksum.end());

        appendUint64(buffer, static_cast<uint64_t>(chunk.data.size()));
        buffer.insert(buffer.end(), chunk.data.begin(), chunk.data.end());
    }

    if (buffer.size() > MAX_FRAME_SIZE) {
        std::cerr << "[Transfer] Encoded delta exceeds frame size" << std::endl;
        return {};
    }

    return buffer;
}

bool Transfer::decodeDelta(const std::vector<uint8_t>& payload, DeltaData& delta) {
    size_t offset = 0;
    delta = DeltaData();

    uint32_t pathLen = 0;
    if (!readUint32(payload, offset, pathLen)) {
        return false;
    }

    if (offset + pathLen > payload.size()) {
        return false;
    }
    delta.filePath.assign(payload.begin() + offset, payload.begin() + offset + pathLen);
    offset += pathLen;

    if (offset >= payload.size()) {
        return false;
    }
    delta.isCompressed = payload[offset++] != 0;

    uint32_t oldHashLen = 0;
    if (!readUint32(payload, offset, oldHashLen) || offset + oldHashLen > payload.size()) {
        return false;
    }
    delta.oldHash.assign(payload.begin() + offset, payload.begin() + offset + oldHashLen);
    offset += oldHashLen;

    uint32_t newHashLen = 0;
    if (!readUint32(payload, offset, newHashLen) || offset + newHashLen > payload.size()) {
        return false;
    }
    delta.newHash.assign(payload.begin() + offset, payload.begin() + offset + newHashLen);
    offset += newHashLen;

    uint32_t chunkCount = 0;
    if (!readUint32(payload, offset, chunkCount)) {
        return false;
    }

    delta.chunks.clear();
    delta.chunks.reserve(chunkCount);

    for (uint32_t i = 0; i < chunkCount; ++i) {
        uint64_t offsetValue = 0;
        uint64_t lengthValue = 0;
        if (!readUint64(payload, offset, offsetValue) || !readUint64(payload, offset, lengthValue)) {
            return false;
        }

        uint32_t checksumLen = 0;
        if (!readUint32(payload, offset, checksumLen) || offset + checksumLen > payload.size()) {
            return false;
        }

        std::string checksum(payload.begin() + offset, payload.begin() + offset + checksumLen);
        offset += checksumLen;

        uint64_t dataLen = 0;
        if (!readUint64(payload, offset, dataLen)) {
            return false;
        }

        if (dataLen > MAX_FRAME_SIZE || offset + dataLen > payload.size()) {
            return false;
        }

        DeltaChunk chunk(offsetValue, lengthValue);
        chunk.checksum = std::move(checksum);
        chunk.data.assign(payload.begin() + offset, payload.begin() + offset + static_cast<size_t>(dataLen));
        offset += static_cast<size_t>(dataLen);

        delta.chunks.push_back(std::move(chunk));
    }

    return offset == payload.size();
}

std::vector<uint8_t> Transfer::encodeFilePayload(const std::string& filePath, const std::vector<uint8_t>& bytes) {
    std::vector<uint8_t> buffer;

    if (filePath.size() > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "[Transfer] File path too long to encode" << std::endl;
        return {};
    }

    appendUint32(buffer, static_cast<uint32_t>(filePath.size()));
    buffer.insert(buffer.end(), filePath.begin(), filePath.end());

    appendUint64(buffer, static_cast<uint64_t>(bytes.size()));
    buffer.insert(buffer.end(), bytes.begin(), bytes.end());

    if (buffer.size() > MAX_FRAME_SIZE) {
        std::cerr << "[Transfer] Encoded file payload exceeds frame size" << std::endl;
        return {};
    }

    return buffer;
}

bool Transfer::decodeFilePayload(const std::vector<uint8_t>& payload, std::string& path, std::vector<uint8_t>& contents) {
    size_t offset = 0;

    uint32_t pathLen = 0;
    if (!readUint32(payload, offset, pathLen) || offset + pathLen > payload.size()) {
        return false;
    }

    path.assign(payload.begin() + offset, payload.begin() + offset + pathLen);
    offset += pathLen;

    uint64_t dataLen = 0;
    if (!readUint64(payload, offset, dataLen)) {
        return false;
    }

    if (dataLen > MAX_FRAME_SIZE || offset + dataLen > payload.size()) {
        return false;
    }

    contents.assign(payload.begin() + offset, payload.begin() + offset + static_cast<size_t>(dataLen));
    offset += static_cast<size_t>(dataLen);

    return offset == payload.size();
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