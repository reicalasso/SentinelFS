#include "SHA256.h"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

using namespace SentinelFS;

void test_string_hash() {
    std::cout << "Running test_string_hash..." << std::endl;
    
    // Known SHA256 hashes
    // "hello" -> "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"
    std::string input = "hello";
    std::string expected = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    
    std::string result = SHA256::hash(input);
    assert(result == expected);
    
    // Empty string
    // "" -> "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    std::string emptyExpected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    assert(SHA256::hash("") == emptyExpected);
    
    std::cout << "test_string_hash passed." << std::endl;
}

void test_bytes_hash() {
    std::cout << "Running test_bytes_hash..." << std::endl;
    
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    // 010203 -> "039058c6f2c0cb492c533b0a4d14ef77cc0f78abccced5287d84a1a2011cfb81"
    std::string expected = "039058c6f2c0cb492c533b0a4d14ef77cc0f78abccced5287d84a1a2011cfb81";
    
    std::string result = SHA256::hashBytes(data);
    assert(result == expected);
    
    std::cout << "test_bytes_hash passed." << std::endl;
}

int main() {
    try {
        test_string_hash();
        test_bytes_hash();
        std::cout << "All SHA256 tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
