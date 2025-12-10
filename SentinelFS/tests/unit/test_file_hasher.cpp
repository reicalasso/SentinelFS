#include "FileHasher.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <vector>

using namespace SentinelFS;

void test_file_hashing() {
    std::cout << "Running test_file_hashing..." << std::endl;
    
    std::string filename = "test_hasher.tmp";
    std::string content = "Hello World";
    
    std::ofstream file(filename);
    file << content;
    file.close();
    
    // SHA256 of "Hello World"
    // a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e
    std::string expected = "a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e";
    
    std::string result = FileHasher::calculateSHA256(filename);
    assert(result == expected);
    
    std::filesystem::remove(filename);
    std::cout << "test_file_hashing passed." << std::endl;
}

void test_buffer_hashing() {
    std::cout << "Running test_buffer_hashing..." << std::endl;
    
    std::string content = "Hello World";
    std::vector<uint8_t> buffer(content.begin(), content.end());
    
    std::string expected = "a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e";
    
    std::string result = FileHasher::calculateSHA256(buffer.data(), buffer.size());
    assert(result == expected);
    
    std::cout << "test_buffer_hashing passed." << std::endl;
}

int main() {
    try {
        test_file_hashing();
        test_buffer_hashing();
        std::cout << "All FileHasher tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
