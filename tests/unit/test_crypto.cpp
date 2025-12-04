#include "Crypto.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>

using namespace SentinelFS;

void test_key_generation() {
    std::cout << "Running test_key_generation..." << std::endl;
    auto key1 = Crypto::generateKey();
    auto key2 = Crypto::generateKey();
    
    assert(key1.size() == Crypto::KEY_SIZE);
    assert(key2.size() == Crypto::KEY_SIZE);
    assert(key1 != key2); // Extremely unlikely to be same
    
    auto iv = Crypto::generateIV();
    assert(iv.size() == Crypto::IV_SIZE);
    
    std::cout << "test_key_generation passed." << std::endl;
}

void test_encryption_decryption() {
    std::cout << "Running test_encryption_decryption..." << std::endl;
    
    std::string message = "Hello, SentinelFS!";
    std::vector<uint8_t> plaintext(message.begin(), message.end());
    
    auto key = Crypto::generateKey();
    auto iv = Crypto::generateIV();
    
    auto encrypted = Crypto::encrypt(plaintext, key, iv);
    assert(!encrypted.empty());
    assert(encrypted != plaintext);
    
    auto decrypted = Crypto::decrypt(encrypted, key, iv);
    assert(decrypted == plaintext);
    
    std::string decryptedMessage(decrypted.begin(), decrypted.end());
    assert(decryptedMessage == message);
    
    std::cout << "test_encryption_decryption passed." << std::endl;
}

void test_invalid_decrypt() {
    std::cout << "Running test_invalid_decrypt..." << std::endl;
    
    std::string message = "Secret Data";
    std::vector<uint8_t> plaintext(message.begin(), message.end());
    
    auto key = Crypto::generateKey();
    auto iv = Crypto::generateIV();
    
    auto encrypted = Crypto::encrypt(plaintext, key, iv);
    
    // Try decrypting with wrong key
    auto wrongKey = Crypto::generateKey();
    bool caught = false;
    try {
        Crypto::decrypt(encrypted, wrongKey, iv);
    } catch (...) {
        caught = true;
    }
    // Note: Depending on implementation, it might produce garbage or throw. 
    // If it uses HMAC or authenticated encryption, it should throw. 
    // If just AES-CBC, it might just produce garbage. 
    // The header says "AES-256-CBC encryption/decryption with PKCS7 padding".
    // Incorrect padding usually throws.
    
    // Let's assume it might throw on padding error.
    // If it doesn't throw, we check that output is not original plaintext.
    if (!caught) {
         auto decrypted = Crypto::decrypt(encrypted, wrongKey, iv);
         assert(decrypted != plaintext);
    }

    std::cout << "test_invalid_decrypt passed." << std::endl;
}

int main() {
    try {
        test_key_generation();
        test_encryption_decryption();
        test_invalid_decrypt();
        std::cout << "All Crypto tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
