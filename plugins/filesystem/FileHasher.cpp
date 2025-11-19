#include "FileHasher.h"
#include "DeltaEngine.h"
#include <fstream>
#include <vector>
#include <iostream>

namespace SentinelFS {

std::string FileHasher::calculateSHA256(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for hashing: " << filePath << std::endl;
        return "";
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), 
                                 std::istreambuf_iterator<char>());
    
    return DeltaEngine::calculateSHA256(buffer.data(), buffer.size());
}

std::string FileHasher::calculateSHA256(const uint8_t* data, size_t length) {
    return DeltaEngine::calculateSHA256(data, length);
}

} // namespace SentinelFS
