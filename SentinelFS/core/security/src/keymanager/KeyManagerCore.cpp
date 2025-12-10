/**
 * @file KeyManagerCore.cpp
 * @brief KeyManager constructor, destructor and core functionality
 */

#include "KeyManager.h"
#include "Logger.h"
#include <openssl/rand.h>

namespace SentinelFS {

KeyManager::KeyManager(std::shared_ptr<IKeyStore> keyStore)
    : keyStore_(std::move(keyStore))
{
}

KeyManager::~KeyManager() {
    // Securely clear sensitive data
    if (identityKey_) {
        if (!identityKey_->privateKey.empty()) {
            OPENSSL_cleanse(identityKey_->privateKey.data(), 
                           identityKey_->privateKey.size());
        }
    }
    
    for (auto& [peerId, session] : sessionKeys_) {
        if (!session.key.empty()) {
            OPENSSL_cleanse(session.key.data(), session.key.size());
        }
    }
}

} // namespace SentinelFS
