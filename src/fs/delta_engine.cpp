#include "delta_engine.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

DeltaEngine::DeltaEngine(const std::string& filePath) : filePath(filePath) {}

DeltaEngine::~DeltaEngine() {}

DeltaData DeltaEngine::compute() {
    // For now, implement a basic delta calculation
    // In a real implementation, this would compare with a reference version
    DeltaData delta(filePath);
    
    // Calculate hash of the current file
    delta.newHash = calculateHash(filePath);
    
    // For this basic implementation, we'll return the entire file as one chunk
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return delta;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (fileSize > 0) {
        DeltaChunk chunk(0, fileSize);
        chunk.data.resize(fileSize);
        file.read(reinterpret_cast<char*>(chunk.data.data()), fileSize);
        delta.chunks.push_back(chunk);
    }
    
    file.close();
    return delta;
}

DeltaData DeltaEngine::compute(const std::string& oldFile, const std::string& newFile) {
    DeltaData delta(newFile);
    
    // Calculate hashes
    delta.oldHash = calculateHash(oldFile);
    delta.newHash = calculateHash(newFile);
    
    // This is a simplified implementation - in reality, we'd compare the files block by block
    if (delta.oldHash != delta.newHash) {
        // If hashes are different, include the entire new file
        std::ifstream file(newFile, std::ios::binary);
        if (file) {
            file.seekg(0, std::ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);
            
            if (fileSize > 0) {
                DeltaChunk chunk(0, fileSize);
                chunk.data.resize(fileSize);
                file.read(reinterpret_cast<char*>(chunk.data.data()), fileSize);
                delta.chunks.push_back(chunk);
            }
            file.close();
        }
    }
    
    return delta;
}

bool DeltaEngine::apply(const DeltaData& delta, const std::string& targetFile) {
    std::ofstream file(targetFile, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // Apply each chunk to the target file
    for (const auto& chunk : delta.chunks) {
        file.seekp(chunk.offset, std::ios::beg);
        file.write(reinterpret_cast<const char*>(chunk.data.data()), chunk.length);
    }
    
    file.close();
    return true;
}

std::string DeltaEngine::calculateHash(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return "";
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read entire file into buffer
    std::vector<unsigned char> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();
    
    // Calculate SHA256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(buffer.data(), buffer.size(), hash);
    
    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}