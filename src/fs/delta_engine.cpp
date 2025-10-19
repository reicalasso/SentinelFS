#include "delta_engine.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <map>
#include <unordered_map>
#include <memory>

DeltaEngine::DeltaEngine(const std::string& filePath) 
    : filePath(filePath), compressor(std::make_unique<Compressor>(CompressionAlgorithm::GZIP)) {}

DeltaEngine::~DeltaEngine() = default;

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

// Rsync-style delta implementation
DeltaData DeltaEngine::computeRsync(const std::string& oldFile, const std::string& newFile, uint64_t blockSize) {
    DeltaData delta(newFile);
    
    // Calculate block checksums for the old file
    auto oldBlocks = calculateFileBlocks(oldFile, blockSize);
    
    // Map checksums to block information for quick lookup
    std::unordered_map<std::string, FileBlock> checksumMap;
    for (const auto& block : oldBlocks) {
        checksumMap[block.checksum] = block;
    }
    
    // Read the new file and compare blocks
    std::ifstream newFileStream(newFile, std::ios::binary);
    if (!newFileStream) {
        return delta;
    }
    
    // Get new file size
    newFileStream.seekg(0, std::ios::end);
    size_t newFileSize = newFileStream.tellg();
    newFileStream.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> newFileBuffer(newFileSize);
    newFileStream.read(reinterpret_cast<char*>(newFileBuffer.data()), newFileSize);
    newFileStream.close();
    
    // Process new file in blocks
    for (uint64_t offset = 0; offset < newFileSize; offset += blockSize) {
        uint64_t currentBlockSize = std::min(blockSize, static_cast<uint64_t>(newFileSize - offset));
        
        // Extract current block
        std::vector<uint8_t> currentBlock(newFileBuffer.begin() + offset, 
                                         newFileBuffer.begin() + offset + currentBlockSize);
        
        // Calculate checksum for current block
        std::string currentChecksum = calculateBlockChecksum(currentBlock);
        
        // Check if this block exists in the old file
        auto it = checksumMap.find(currentChecksum);
        if (it != checksumMap.end()) {
            // Block already exists in old file, no need to transfer
            continue;
        } else {
            // New or modified block - add to delta
            DeltaChunk chunk(offset, currentBlockSize);
            chunk.data = currentBlock;
            chunk.checksum = currentChecksum;
            delta.chunks.push_back(chunk);
        }
    }
    
    return delta;
}

std::vector<FileBlock> DeltaEngine::calculateFileBlocks(const std::string& filePath, uint64_t blockSize) {
    std::vector<FileBlock> blocks;
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return blocks;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Process file in blocks
    for (uint64_t offset = 0; offset < fileSize; offset += blockSize) {
        uint64_t currentBlockSize = std::min(blockSize, static_cast<uint64_t>(fileSize - offset));
        
        // Read block
        std::vector<uint8_t> blockData(currentBlockSize);
        file.seekg(offset, std::ios::beg);
        file.read(reinterpret_cast<char*>(blockData.data()), currentBlockSize);
        
        // Calculate checksum
        std::string checksum = calculateBlockChecksum(blockData);
        
        // Add to blocks list
        blocks.emplace_back(offset, checksum, currentBlockSize);
    }
    
    file.close();
    return blocks;
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

std::string DeltaEngine::calculateBlockChecksum(const std::vector<uint8_t>& data) {
    // Use SHA256 for block checksums
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
    
    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

std::vector<uint8_t> DeltaEngine::readBlock(const std::string& filePath, uint64_t offset, uint64_t length) {
    std::vector<uint8_t> data(length);
    std::ifstream file(filePath, std::ios::binary);
    
    if (file) {
        file.seekg(offset, std::ios::beg);
        file.read(reinterpret_cast<char*>(data.data()), length);
    }
    
    return data;
}

DeltaData DeltaEngine::computeBlockBasedDelta(const std::string& oldFile, const std::string& newFile, uint64_t blockSize) {
    DeltaData delta(newFile);
    
    // Calculate block checksums for the old file
    auto oldBlocks = calculateFileBlocks(oldFile, blockSize);
    
    // Create a map for quick lookup of blocks by checksum
    std::unordered_map<std::string, FileBlock> oldBlockMap;
    for (const auto& block : oldBlocks) {
        oldBlockMap[block.checksum] = block;
    }
    
    // Process the new file to find differences
    std::ifstream newFileStream(newFile, std::ios::binary);
    if (!newFileStream) {
        return delta; // Return empty delta if new file can't be read
    }
    
    // Get new file size
    newFileStream.seekg(0, std::ios::end);
    size_t newFileSize = newFileStream.tellg();
    newFileStream.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> newFileBuffer(newFileSize);
    newFileStream.read(reinterpret_cast<char*>(newFileBuffer.data()), newFileSize);
    newFileStream.close();
    
    // Compare block by block
    std::vector<bool> processedBlocks((newFileSize + blockSize - 1) / blockSize, false);
    
    for (uint64_t offset = 0; offset < newFileSize; offset += blockSize) {
        uint64_t currentBlockSize = std::min(blockSize, static_cast<uint64_t>(newFileSize - offset));
        
        // Extract the current block
        std::vector<uint8_t> currentBlock(newFileBuffer.begin() + offset, 
                                         newFileBuffer.begin() + offset + currentBlockSize);
        
        // Calculate checksum for the current block
        std::string currentChecksum = calculateBlockChecksum(currentBlock);
        
        // Check if this block exists in the old file
        if (oldBlockMap.find(currentChecksum) != oldBlockMap.end()) {
            // Block exists in old file with same checksum - skip
            processedBlocks[offset / blockSize] = true;
        } else {
            // New or changed block - add to delta
            DeltaChunk chunk(offset, currentBlockSize);
            chunk.data = currentBlock;
            chunk.checksum = currentChecksum;
            delta.chunks.push_back(chunk);
        }
    }
    
    return delta;
}

DeltaData DeltaEngine::computeCompressed(const std::string& oldFile, const std::string& newFile) {
    DeltaData delta = compute(oldFile, newFile);
    
    // If there are chunks to send, compress them
    if (!delta.chunks.empty() && compressor) {
        for (auto& chunk : delta.chunks) {
            // Compress the chunk data
            std::vector<uint8_t> compressedData = compressor->compress(chunk.data);
            
            // Replace original data with compressed data
            chunk.data = compressedData;
        }
        delta.isCompressed = true;
    }
    
    return delta;
}

void DeltaEngine::setCompression(CompressionAlgorithm algorithm) {
    if (compressor) {
        compressor->setAlgorithm(algorithm);
    } else {
        compressor = std::make_unique<Compressor>(algorithm);
    }
}

bool DeltaEngine::apply(const DeltaData& delta, const std::string& targetFile) {
    // Open file for read/write to allow updates
    std::fstream file(targetFile, std::ios::in | std::ios::out | std::ios::binary);
    
    // If file doesn't exist, create it
    if (!file) {
        file.open(targetFile, std::ios::out | std::ios::binary);
        file.close();
        file.open(targetFile, std::ios::in | std::ios::out | std::ios::binary);
    }
    
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

bool DeltaEngine::applyCompressed(const DeltaData& delta, const std::string& targetFile) {
    if (!delta.isCompressed || !compressor) {
        // If data is not compressed, use regular apply
        return apply(delta, targetFile);
    }
    
    // Create a temporary delta with decompressed data
    DeltaData decompressedDelta = delta;
    for (auto& chunk : decompressedDelta.chunks) {
        // Decompress the chunk data
        std::vector<uint8_t> decompressedData = compressor->decompress(chunk.data);
        chunk.data = decompressedData;
    }
    
    // Apply the decompressed delta
    decompressedDelta.isCompressed = false;
    return apply(decompressedDelta, targetFile);
}