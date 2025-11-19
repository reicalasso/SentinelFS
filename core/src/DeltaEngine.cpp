#include "DeltaEngine.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>
#include <unordered_map>

namespace SentinelFS {

    uint32_t DeltaEngine::calculateAdler32(const uint8_t* data, size_t len) {
        uint32_t a = 1, b = 0;
        const uint32_t MOD_ADLER = 65521;

        for (size_t i = 0; i < len; ++i) {
            a = (a + data[i]) % MOD_ADLER;
            b = (b + a) % MOD_ADLER;
        }

        return (b << 16) | a;
    }

    std::string DeltaEngine::calculateSHA256(const uint8_t* data, size_t len) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, data, len);
        SHA256_Final(hash, &sha256);

        std::stringstream ss;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

    std::vector<BlockSignature> DeltaEngine::calculateSignature(const std::string& filePath) {
        std::vector<BlockSignature> signatures;
        std::ifstream file(filePath, std::ios::binary);
        
        if (!file) {
            std::cerr << "Failed to open file for signature calculation: " << filePath << std::endl;
            return signatures;
        }

        std::vector<uint8_t> buffer(BLOCK_SIZE);
        uint32_t index = 0;

        while (file) {
            file.read(reinterpret_cast<char*>(buffer.data()), BLOCK_SIZE);
            size_t bytesRead = file.gcount();
            
            if (bytesRead == 0) break;

            BlockSignature sig;
            sig.index = index++;
            sig.adler32 = calculateAdler32(buffer.data(), bytesRead);
            sig.sha256 = calculateSHA256(buffer.data(), bytesRead);
            
            signatures.push_back(sig);
        }

        return signatures;
    }

    std::vector<DeltaInstruction> DeltaEngine::calculateDelta(const std::string& newFilePath, const std::vector<BlockSignature>& oldSignature) {
        std::vector<DeltaInstruction> deltas;
        std::ifstream file(newFilePath, std::ios::binary);
        
        if (!file) {
            std::cerr << "Failed to open file for delta calculation: " << newFilePath << std::endl;
            return deltas;
        }

        // Map Adler32 to list of signatures (to handle collisions)
        std::unordered_map<uint32_t, std::vector<BlockSignature>> signatureMap;
        for (const auto& sig : oldSignature) {
            signatureMap[sig.adler32].push_back(sig);
        }

        // Read entire file into memory for simplicity (optimization: use sliding window buffer)
        // For large files, this should be optimized to use a circular buffer.
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> fileData(fileSize);
        file.read(reinterpret_cast<char*>(fileData.data()), fileSize);

        size_t offset = 0;
        std::vector<uint8_t> literalBuffer;

        while (offset < fileSize) {
            size_t remaining = fileSize - offset;
            size_t windowSize = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;
            
            uint32_t currentAdler = calculateAdler32(fileData.data() + offset, windowSize);
            
            bool matchFound = false;
            if (signatureMap.count(currentAdler)) {
                std::string currentSHA = calculateSHA256(fileData.data() + offset, windowSize);
                for (const auto& sig : signatureMap[currentAdler]) {
                    if (sig.sha256 == currentSHA) {
                        // Match found!
                        
                        // Flush literals if any
                        if (!literalBuffer.empty()) {
                            deltas.push_back({true, literalBuffer, 0});
                            literalBuffer.clear();
                        }

                        // Add block reference
                        deltas.push_back({false, {}, sig.index});
                        
                        offset += windowSize;
                        matchFound = true;
                        break;
                    }
                }
            }

            if (!matchFound) {
                literalBuffer.push_back(fileData[offset]);
                offset++;
            }
        }

        // Flush remaining literals
        if (!literalBuffer.empty()) {
            deltas.push_back({true, literalBuffer, 0});
        }

        return deltas;
    }

}
