#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct DeltaChunk {
    uint64_t offset;
    uint64_t length;
    std::vector<uint8_t> data;
    
    DeltaChunk(uint64_t off, uint64_t len) : offset(off), length(len), data(len) {}
};

struct DeltaData {
    std::string filePath;
    std::vector<DeltaChunk> chunks;
    std::string oldHash;
    std::string newHash;
    
    DeltaData(const std::string& path) : filePath(path) {}
};

class DeltaEngine {
public:
    DeltaEngine(const std::string& filePath);
    ~DeltaEngine();

    // Compute delta between current file and reference
    DeltaData compute();
    
    // Compute delta between two files
    DeltaData compute(const std::string& oldFile, const std::string& newFile);
    
    // Apply delta to a file
    bool apply(const DeltaData& delta, const std::string& targetFile);
    
    // Calculate hash of a file
    static std::string calculateHash(const std::string& filePath);
    
private:
    std::string filePath;
};