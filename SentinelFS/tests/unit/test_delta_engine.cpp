#include "DeltaEngine.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>

using namespace SentinelFS;

void test_adler32() {
    std::cout << "Running test_adler32..." << std::endl;
    
    std::string data = "Wikipedia";
    // Adler-32 of "Wikipedia" is 0x11E60398
    uint32_t expected = 0x11E60398;
    
    uint32_t result = DeltaEngine::calculateAdler32(
        reinterpret_cast<const uint8_t*>(data.data()), 
        data.size()
    );
    
    assert(result == expected);
    std::cout << "test_adler32 passed." << std::endl;
}

void test_signature_calculation() {
    std::cout << "Running test_signature_calculation..." << std::endl;
    
    // Create a temporary file
    std::string filename = "test_delta_engine.tmp";
    std::ofstream file(filename, std::ios::binary);
    // Write enough data for at least 2 blocks (Block size is 4096)
    std::vector<char> buffer(8192, 'A');
    file.write(buffer.data(), buffer.size());
    file.close();
    
    auto signatures = DeltaEngine::calculateSignature(filename);
    
    assert(signatures.size() == 2);
    assert(signatures[0].index == 0);
    assert(signatures[1].index == 1);
    
    std::filesystem::remove(filename);
    std::cout << "test_signature_calculation passed." << std::endl;
}

int main() {
    try {
        test_adler32();
        test_signature_calculation();
        std::cout << "All DeltaEngine tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
