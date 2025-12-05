#include "DeltaSync.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

using namespace SentinelFS;

void test_adler32_wrapper() {
    std::cout << "Running test_adler32_wrapper..." << std::endl;
    
    std::string data_str = "Wikipedia";
    std::vector<uint8_t> data(data_str.begin(), data_str.end());
    
    // Adler-32 of "Wikipedia" is 0x11E60398
    uint32_t expected = 0x11E60398;
    
    uint32_t result = DeltaSync::calculateAdler32(data);
    
    assert(result == expected);
    std::cout << "test_adler32_wrapper passed." << std::endl;
}

int main() {
    try {
        test_adler32_wrapper();
        std::cout << "All DeltaSync tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
