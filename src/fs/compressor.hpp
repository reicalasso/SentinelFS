#pragma once

#include <string>
#include <vector>
#include <memory>

enum class CompressionAlgorithm {
    GZIP,
    ZSTD,
    LZ4,
    NONE
};

class Compressor {
public:
    Compressor(CompressionAlgorithm algorithm = CompressionAlgorithm::GZIP);
    ~Compressor();
    
    // Compress data
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data);
    
    // Decompress data
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressedData);
    
    // Compress file
    bool compressFile(const std::string& inputPath, const std::string& outputPath);
    
    // Decompress file
    bool decompressFile(const std::string& inputPath, const std::string& outputPath);
    
    // Set compression algorithm
    void setAlgorithm(CompressionAlgorithm algorithm) { this->algorithm = algorithm; }
    
    // Get compression ratio
    double getCompressionRatio(const std::vector<uint8_t>& original, 
                              const std::vector<uint8_t>& compressed);
    
private:
    CompressionAlgorithm algorithm;
    
    // Gzip methods
    std::vector<uint8_t> compressGzip(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompressGzip(const std::vector<uint8_t>& compressedData);
    
    // Zstd methods (stub implementations for now)
    std::vector<uint8_t> compressZstd(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompressZstd(const std::vector<uint8_t>& compressedData);
    
    // LZ4 methods (stub implementations for now)
    std::vector<uint8_t> compressLZ4(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompressLZ4(const std::vector<uint8_t>& compressedData);
};