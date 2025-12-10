#include "KeyManager.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace SentinelFS;

void test_key_info() {
    std::cout << "Running test_key_info..." << std::endl;
    
    KeyInfo info;
    info.keyId = "test_key";
    info.type = KeyType::SESSION;
    info.created = std::chrono::system_clock::now();
    info.expires = info.created + std::chrono::hours(1);
    
    assert(!info.isExpired());
    
    info.expires = info.created - std::chrono::hours(1);
    assert(info.isExpired());
    
    std::cout << "test_key_info passed." << std::endl;
}

void test_session_key_rotation() {
    std::cout << "Running test_session_key_rotation..." << std::endl;
    
    SessionKey key;
    key.bytesEncrypted = 0;
    key.messagesEncrypted = 0;
    // Initialize expires to future, otherwise it defaults to epoch (0) which is expired
    key.expires = std::chrono::system_clock::now() + std::chrono::hours(1);
    
    assert(!key.needsRotation());
    
    key.bytesEncrypted = SessionKey::MAX_BYTES + 1;
    assert(key.needsRotation());
    
    key.bytesEncrypted = 0;
    key.messagesEncrypted = SessionKey::MAX_MESSAGES + 1;
    assert(key.needsRotation());
    
    std::cout << "test_session_key_rotation passed." << std::endl;
}

void test_vector_clock() {
    // VectorClock is in ConflictResolver.h but I'll test it here or in test_conflict_resolver
    // Wait, I should put it in test_conflict_resolver.
    // This file is for KeyManager.
}

int main() {
    try {
        test_key_info();
        test_session_key_rotation();
        std::cout << "All KeyManager tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
