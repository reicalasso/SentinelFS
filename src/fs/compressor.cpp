#include "compressor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <zlib.h>

Compressor::Compressor(CompressionAlgorithm algorithm) : algorithm(algorithm) {}

Compressor::~Compressor() = default;

std::vector<uint8_t> Compressor::compress(const std::vector<uint8_t>& data) {
    switch (algorithm) {
        case CompressionAlgorithm::GZIP:
            return compressGzip(data);
        case CompressionAlgorithm::ZSTD:
            return compressZstd(data);
        case CompressionAlgorithm::LZ4:
            return compressLZ4(data);
        case CompressionAlgorithm::NONE:
        default:
            return data; // No compression
    }
}

std::vector<uint8_t> Compressor::decompress(const std::vector<uint8_t>& compressedData) {
    switch (algorithm) {
        case CompressionAlgorithm::GZIP:
            return decompressGzip(compressedData);
        case CompressionAlgorithm::ZSTD:
            return decompressZstd(compressedData);
        case CompressionAlgorithm::LZ4:
            return decompressLZ4(compressedData);
        case CompressionAlgorithm::NONE:
        default:
            return compressedData; // No decompression
    }
}

bool Compressor::compressFile(const std::string& inputPath, const std::string& outputPath) {
    // Read input file
    std::ifstream inputFile(inputPath, std::ios::binary);
    if (!inputFile) {
        return false;
    }
    
    inputFile.seekg(0, std::ios::end);
    size_t inputSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> inputData(inputSize);
    inputFile.read(reinterpret_cast<char*>(inputData.data()), inputSize);
    inputFile.close();
    
    // Compress the data
    std::vector<uint8_t> compressedData = compress(inputData);
    
    // Write compressed data to output file
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile) {
        return false;
    }
    
    outputFile.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());
    outputFile.close();
    
    return true;
}

bool Compressor::decompressFile(const std::string& inputPath, const std::string& outputPath) {
    // Read compressed input file
    std::ifstream inputFile(inputPath, std::ios::binary);
    if (!inputFile) {
        return false;
    }
    
    inputFile.seekg(0, std::ios::end);
    size_t inputSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> inputData(inputSize);
    inputFile.read(reinterpret_cast<char*>(inputData.data()), inputSize);
    inputFile.close();
    
    // Decompress the data
    std::vector<uint8_t> decompressedData = decompress(inputData);
    
    // Write decompressed data to output file
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile) {
        return false;
    }
    
    outputFile.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size());
    outputFile.close();
    
    return true;
}

double Compressor::getCompressionRatio(const std::vector<uint8_t>& original, 
                                      const std::vector<uint8_t>& compressed) {
    if (original.empty()) {
        return 0.0;
    }
    return static_cast<double>(compressed.size()) / static_cast<double>(original.size());
}

// Gzip implementation
std::vector<uint8_t> Compressor::compressGzip(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::vector<uint8_t>();
    }
    
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return std::vector<uint8_t>(); // Error initializing
    }
    
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<uint8_t*>(data.data()));
    zs.avail_in = data.size();
    
    int ret;
    std::vector<uint8_t> outbuffer(32768);
    std::vector<uint8_t> compressed;
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer.data());
        zs.avail_out = outbuffer.size();
        
        ret = deflate(&zs, Z_FINISH);
        
        if (compressed.size() < zs.total_out) {
            compressed.insert(compressed.end(),
                              outbuffer.begin(),
                              outbuffer.begin() + zs.total_out - compressed.size());
        }
    } while (ret == Z_OK);
    
    deflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        return std::vector<uint8_t>(); // Error during compression
    }
    
    return compressed;
}

std::vector<uint8_t> Compressor::decompressGzip(const std::vector<uint8_t>& compressedData) {
    if (compressedData.empty()) {
        return std::vector<uint8_t>();
    }
    
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (inflateInit2(&zs, 15 + 16) != Z_OK) {
        return std::vector<uint8_t>(); // Error initializing
    }
    
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<uint8_t*>(compressedData.data()));
    zs.avail_in = compressedData.size();
    
    int ret;
    std::vector<uint8_t> outbuffer(32768);
    std::vector<uint8_t> decompressed;
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer.data());
        zs.avail_out = outbuffer.size();
        
        ret = inflate(&zs, Z_SYNC_FLUSH);
        
        if (decompressed.size() < zs.total_out) {
            decompressed.insert(decompressed.end(),
                                outbuffer.begin(),
                                outbuffer.begin() + zs.total_out - decompressed.size());
        }
    } while (ret == Z_OK);
    
    inflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        return std::vector<uint8_t>(); // Error during decompression
    }
    
    return decompressed;
}

// Stub implementations for Zstd and LZ4 (would require additional libraries)
std::vector<uint8_t> Compressor::compressZstd(const std::vector<uint8_t>& data) {
    // For now, use gzip implementation as fallback
    return compressGzip(data);
}

std::vector<uint8_t> Compressor::decompressZstd(const std::vector<uint8_t>& compressedData) {
    // For now, use gzip implementation as fallback
    return decompressGzip(compressedData);
}

std::vector<uint8_t> Compressor::compressLZ4(const std::vector<uint8_t>& data) {
    // For now, use no compression as fallback
    return data;
}

std::vector<uint8_t> Compressor::decompressLZ4(const std::vector<uint8_t>& compressedData) {
    // For now, return as-is as fallback
    return compressedData;
}