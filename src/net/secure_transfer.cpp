#include "secure_transfer.hpp"
#include <iostream>

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
    securityManager->recordPeerActivity(peer.id, delta.chunks.size() * sizeof(DeltaChunk));
    
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
        securityManager->recordPeerActivity(peer.id, delta.chunks.size() * sizeof(DeltaChunk));
        
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
    auto encryptedFileData = securityManager->encryptFile(filePath);
    if (encryptedFileData.empty()) {
        std::cerr << "Failed to encrypt file: " << filePath << std::endl;
        return false;
    }
    
    // Send using base transfer (this is simplified - in reality you'd need to modify the transfer to send encrypted data)
    // For now, we'll use the base transfer functionality
    return baseTransfer.sendFilePlain(filePath, peer);
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
    
    // Receive the file using base transfer
    bool result = baseTransfer.receiveFilePlain(filePath, peer);
    
    if (result) {
        // File is already in place, now decrypt it
        // (In a real implementation, the file would be encrypted during transfer)
    }
    
    return result;
}

DeltaData SecureTransfer::encryptDelta(const DeltaData& delta, const PeerInfo& peer) {
    if (!securityManager) {
        return delta; // Return unencrypted if no security manager
    }
    
    DeltaData encryptedDelta = delta;
    
    // Encrypt each chunk
    for (auto& chunk : encryptedDelta.chunks) {
        chunk.data = securityManager->encryptData(chunk.data, peer.id);
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
        chunk.data = securityManager->decryptData(chunk.data, peer.id);
    }
    
    return delta;
}