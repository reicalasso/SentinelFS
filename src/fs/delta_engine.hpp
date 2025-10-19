#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct DeltaChunk {
    uint64_t offset;
    uint64_t length;
    std::vector<uint8_t> data;
    std::string checksum; // For rsync-style verification
    
    DeltaChunk(uint64_t off, uint64_t len) : offset(off), length(len), data(len) {}
    DeltaChunk(uint64_t off, uint64_t len, const std::string& chksum) 
        : offset(off), length(len), checksum(chksum) {}
};

struct DeltaData {
    std::string filePath;
    std::vector<DeltaChunk> chunks;
    std::string oldHash;
    std::string newHash;
    bool isCompressed;  // Flag to indicate if data is compressed
    
    DeltaData(const std::string& path) : filePath(path), isCompressed(false) {}
};

struct FileBlock {
    uint64_t offset;
    std::string checksum;
    uint64_t length;
    
    FileBlock() : offset(0), length(0) {} // Default constructor
    FileBlock(uint64_t off, const std::string& chksum, uint64_t len) 
        : offset(off), checksum(chksum), length(len) {}
};

#include <memory>
#include "compressor.hpp"  // For compression support

class DeltaEngine {
public:
    DeltaEngine(const std::string& filePath);
    ~DeltaEngine();

    // Compute delta between current file and reference
    DeltaData compute();
    
    // Compute delta between two files
    DeltaData compute(const std::string& oldFile, const std::string& newFile);
    
    // Rsync-style delta computation
    DeltaData computeRsync(const std::string& oldFile, const std::string& newFile, 
                          uint64_t blockSize = 1024);
    
    // Compressed delta computation
    DeltaData computeCompressed(const std::string& oldFile, const std::string& newFile);
    
    // Apply delta to a file
    bool apply(const DeltaData& delta, const std::string& targetFile);
    
    // Apply compressed delta to a file
    bool applyCompressed(const DeltaData& delta, const std::string& targetFile);
    
    // Calculate hash of a file
    static std::string calculateHash(const std::string& filePath);
    
    // Calculate blocks with checksums for rsync
    static std::vector<FileBlock> calculateFileBlocks(const std::string& filePath, 
                                                     uint64_t blockSize = 1024);
    
    // Block-based comparison for rsync
    static DeltaData computeBlockBasedDelta(const std::string& oldFile, 
                                          const std::string& newFile,
                                          uint64_t blockSize = 1024);
    
    // Set compression algorithm
    void setCompression(CompressionAlgorithm algorithm);
    
    // Get/set compressor object
    Compressor& getCompressor() { return *compressor; }
    
private:
    std::string filePath;
    std::unique_ptr<Compressor> compressor;
    
    // Helper methods
    static std::string calculateBlockChecksum(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> readBlock(const std::string& filePath, 
                                         uint64_t offset, uint64_t length);
};