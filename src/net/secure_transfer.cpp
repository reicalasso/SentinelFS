#include "secure_transfer.hpp"
#include <iostream>
#include <fstream>
#include <iterator>

SecureTransfer::SecureTransfer(Transfer& baseTransfer) 
    : baseTransfer(baseTransfer), securityManager(nullptr) {
}

bool SecureTransfer::sendSecureDelta(const DeltaData& delta, const PeerInfo& peer) {
    if (!securityManager) {
        std::cerr << "Security manager not set for SecureTransfer" << std::endl;
        return false;
    }
    
    // Check if peer is authenticated
    if (!securityManager->authenticatePeer(peer)) {
        std::cerr << "Peer " << peer.id << " is not authenticated" << std::endl;
        return false;
    }
    
    // Check rate limiting
    if (securityManager->isRateLimited(peer.id)) {
        std::cerr << "Peer " << peer.id << " is rate limited" << std::endl;
        return false;
    }
    
    // Encrypt the delta
    DeltaData encryptedDelta = encryptDelta(delta, peer);
    
    // Record activity for rate limiting
    size_t payloadBytes = 0;
    for (const auto& chunk : encryptedDelta.chunks) {
        payloadBytes += chunk.data.size();
    }
    securityManager->recordPeerActivity(peer.id, payloadBytes);
    
    // Send through base transfer
    return baseTransfer.sendDeltaPlain(encryptedDelta, peer);
}

bool SecureTransfer::receiveSecureDelta(DeltaData& delta, const PeerInfo& peer) {
    if (!securityManager) {
        std::cerr << "Security manager not set for SecureTransfer" << std::endl;
        return false;
    }
    
    // Check if peer is authenticated
    if (!securityManager->authenticatePeer(peer)) {
        std::cerr << "Peer " << peer.id << " is not authenticated" << std::endl;
        return false;
    }
    
    // Receive encrypted delta
    DeltaData encryptedDelta;
    bool result = baseTransfer.receiveDeltaPlain(encryptedDelta, peer);
    
    if (result) {
        // Decrypt the delta
        delta = decryptDelta(encryptedDelta, peer);
    }
    
    return result;
}

bool SecureTransfer::broadcastSecureDelta(const DeltaData& delta, const std::vector<PeerInfo>& peers) {
    if (!securityManager) {
        std::cerr << "Security manager not set for SecureTransfer" << std::endl;
        return false;
    }
    
    bool allSuccessful = true;
    
    for (const auto& peer : peers) {
        // Check authentication
        if (!securityManager->authenticatePeer(peer)) {
            std::cerr << "Peer " << peer.id << " is not authenticated, skipping" << std::endl;
            allSuccessful = false;
            continue;
        }
        
        // Check rate limiting
        if (securityManager->isRateLimited(peer.id)) {
            std::cerr << "Peer " << peer.id << " is rate limited" << std::endl;
            continue;
        }
        
        // Encrypt delta for this specific peer
        DeltaData encryptedDelta = encryptDelta(delta, peer);
        
        // Record activity
        size_t payloadBytes = 0;
        for (const auto& chunk : encryptedDelta.chunks) {
            payloadBytes += chunk.data.size();
        }
        securityManager->recordPeerActivity(peer.id, payloadBytes);
        
    bool result = baseTransfer.sendDeltaPlain(encryptedDelta, peer);
        if (!result) {
            allSuccessful = false;
            std::cerr << "Failed to send delta to peer: " << peer.id << std::endl;
        }
    }
    
    return allSuccessful;
}

bool SecureTransfer::sendSecureFile(const std::string& filePath, const PeerInfo& peer) {
    if (!securityManager) {
        std::cerr << "Security manager not set for SecureTransfer" << std::endl;
        return false;
    }
    
    // Check authentication and access
    if (!securityManager->authenticatePeer(peer)) {
        std::cerr << "Peer " << peer.id << " is not authenticated" << std::endl;
        return false;
    }
    
    if (!securityManager->checkAccess(peer.id, filePath, AccessLevel::READ_ONLY)) {
        std::cerr << "Access denied for peer " << peer.id << " to file " << filePath << std::endl;
        return false;
    }
    
    // Encrypt the file before sending
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for secure transfer: " << filePath << std::endl;
        return false;
    }

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    auto encryptedBytes = securityManager->encryptData(bytes, peer.id);
    if (encryptedBytes.empty() && !bytes.empty()) {
        std::cerr << "Failed to encrypt file payload for " << filePath << std::endl;
        return false;
    }

    auto wrapped = Transfer::wrapPayload(encryptedBytes.empty() ? bytes : encryptedBytes, !encryptedBytes.empty());
    securityManager->recordPeerActivity(peer.id, wrapped.size());
    return baseTransfer.sendFilePayload(filePath, wrapped, peer);
}

bool SecureTransfer::receiveSecureFile(const std::string& filePath, const PeerInfo& peer) {
    if (!securityManager) {
        std::cerr << "Security manager not set for SecureTransfer" << std::endl;
        return false;
    }
    
    // Check authentication and access
    if (!securityManager->authenticatePeer(peer)) {
        std::cerr << "Peer " << peer.id << " is not authenticated" << std::endl;
        return false;
    }
    
    if (!securityManager->checkAccess(peer.id, filePath, AccessLevel::READ_WRITE)) {
        std::cerr << "Write access denied for peer " << peer.id << " to file " << filePath << std::endl;
        return false;
    }
    
    std::string remotePath;
    std::vector<uint8_t> payload;
    if (!baseTransfer.receiveFilePayload(remotePath, payload, peer)) {
        return false;
    }

    std::vector<uint8_t> body;
    bool encrypted = false;
    if (!Transfer::unwrapPayload(payload, body, encrypted)) {
        return false;
    }

    std::vector<uint8_t> bytes = body;
    if (encrypted) {
        auto decrypted = securityManager->decryptData(body, peer.id);
        if (decrypted.empty() && !body.empty()) {
            std::cerr << "Failed to decrypt incoming secure file from " << peer.id << std::endl;
            return false;
        }
        bytes = std::move(decrypted);
    }

    const std::string& target = filePath.empty() ? remotePath : filePath;
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "Failed to open destination for secure file: " << target << std::endl;
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close();

    securityManager->recordPeerActivity(peer.id, bytes.size());
    return true;
}

DeltaData SecureTransfer::encryptDelta(const DeltaData& delta, const PeerInfo& peer) {
    if (!securityManager) {
        return delta; // Return unencrypted if no security manager
    }
    
    DeltaData encryptedDelta = delta;
    
    // Encrypt each chunk
    for (auto& chunk : encryptedDelta.chunks) {
        auto cipher = securityManager->encryptData(chunk.data, peer.id);
        if (cipher.empty() && !chunk.data.empty()) {
            std::cerr << "Failed to encrypt delta chunk for peer " << peer.id << std::endl;
            chunk.data = Transfer::wrapPayload(chunk.data, false);
        } else {
            chunk.data = Transfer::wrapPayload(cipher, true);
        }
    }
    
    return encryptedDelta;
}

DeltaData SecureTransfer::decryptDelta(const DeltaData& encryptedDelta, const PeerInfo& peer) {
    if (!securityManager) {
        return encryptedDelta; // Return as-is if no security manager
    }
    
    DeltaData delta = encryptedDelta;
    
    // Decrypt each chunk
    for (auto& chunk : delta.chunks) {
        std::vector<uint8_t> body;
        bool encrypted = false;
        if (!Transfer::unwrapPayload(chunk.data, body, encrypted)) {
            continue;
        }

        if (!encrypted) {
            chunk.data = std::move(body);
            continue;
        }

        auto plain = securityManager->decryptData(body, peer.id);
        if (!plain.empty() || body.empty()) {
            chunk.data = std::move(plain);
        } else {
            std::cerr << "Failed to decrypt delta chunk from peer " << peer.id << std::endl;
            chunk.data.clear();
        }
    }
    
    return delta;
}