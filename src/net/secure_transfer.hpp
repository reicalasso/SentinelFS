#pragma once

#include "transfer.hpp"
#include "security/security_manager.hpp"
#include <vector>
#include <string>

class SecureTransfer {
public:
    SecureTransfer(Transfer& baseTransfer);
    ~SecureTransfer() = default;
    
    // Secure versions of transfer methods
    bool sendSecureDelta(const DeltaData& delta, const PeerInfo& peer);
    bool receiveSecureDelta(DeltaData& delta, const PeerInfo& peer);
    bool broadcastSecureDelta(const DeltaData& delta, const std::vector<PeerInfo>& peers);
    
    // Secure file transfer
    bool sendSecureFile(const std::string& filePath, const PeerInfo& peer);
    bool receiveSecureFile(const std::string& filePath, const PeerInfo& peer);
    
    // Set security manager
    void setSecurityManager(SecurityManager& secMgr) { securityManager = &secMgr; }
    
private:
    Transfer& baseTransfer;
    SecurityManager* securityManager;

    friend class Transfer;
    
    // Helper methods
    DeltaData encryptDelta(const DeltaData& delta, const PeerInfo& peer);
    DeltaData decryptDelta(const DeltaData& encryptedDelta, const PeerInfo& peer);
};