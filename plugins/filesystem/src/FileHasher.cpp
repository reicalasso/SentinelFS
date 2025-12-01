#include "FileHasher.h"
#include "DeltaEngine.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <filesystem>

namespace SentinelFS {

std::string FileHasher::calculateSHA256(const std::string& filePath) {
    auto& logger = Logger::instance();
    auto& metrics = MetricsCollector::instance();
    
    logger.log(LogLevel::DEBUG, "Calculating SHA256 for: " + filePath, "FileHasher");
    
    std::error_code ec;
    if (std::filesystem::is_directory(filePath, ec)) {
        logger.log(LogLevel::WARN, "Skipping SHA256 calculation for directory: " + filePath, "FileHasher");
        return "";
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        logger.log(LogLevel::ERROR, "Failed to open file for hashing: " + filePath, "FileHasher");
        metrics.incrementSyncErrors();
        return "";
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), 
                                 std::istreambuf_iterator<char>());
    
    std::string hash = DeltaEngine::calculateSHA256(buffer.data(), buffer.size());
    logger.log(LogLevel::DEBUG, "SHA256 calculated: " + hash.substr(0, 16) + "... for " + filePath, "FileHasher");
    return hash;
}

std::string FileHasher::calculateSHA256(const uint8_t* data, size_t length) {
    return DeltaEngine::calculateSHA256(data, length);
}

} // namespace SentinelFS
