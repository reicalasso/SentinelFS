#include "Compression.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

using namespace sfs;

void test_compress_decompress() {
    std::cout << "Running test_compress_decompress..." << std::endl;
    
    // Create a large repetitive string to ensure it's compressible and above MIN_COMPRESS_SIZE
    std::string text;
    for (int i = 0; i < 1000; ++i) {
        text += "Repeating string to ensure compression works well. ";
    }
    
    std::vector<uint8_t> data(text.begin(), text.end());
    
    auto compressed = Compression::compress(data);
    
    // If it returns empty, it might be because it decided not to compress.
    // But for this highly repetitive large string, it SHOULD compress.
    if (compressed.empty()) {
        std::cerr << "Compression returned empty! Input size: " << data.size() << std::endl;
        // Check if it's just "not beneficial" (unlikely) or error.
        // We can't distinguish easily from outside, but we expect it to work.
        assert(!compressed.empty());
    }
    
    assert(compressed.size() < data.size());
    
    auto decompressed = Compression::decompress(compressed);
    assert(decompressed == data);
    
    std::cout << "test_compress_decompress passed." << std::endl;
}

void test_small_data() {
    std::cout << "Running test_small_data..." << std::endl;
    
    // Small data might not be compressed
    std::string text = "Small";
    std::vector<uint8_t> data(text.begin(), text.end());
    
    auto compressed = Compression::compress(data);
    // Expect empty because it's too small or overhead makes it larger
    assert(compressed.empty());
    
    std::cout << "test_small_data passed." << std::endl;
}

int main() {
    try {
        test_compress_decompress();
        test_small_data();
        std::cout << "All Compression tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
