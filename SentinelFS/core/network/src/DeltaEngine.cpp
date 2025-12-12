#include "DeltaEngine.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include "ThreadPool.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <openssl/evp.h>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <cstring>

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
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen = 0;

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to allocate EVP_MD_CTX");
        }

        const EVP_MD* md = EVP_sha256();
        if (EVP_DigestInit_ex(ctx, md, nullptr) != 1 ||
            EVP_DigestUpdate(ctx, data, len) != 1 ||
            EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("Failed to compute SHA-256 digest");
        }

        EVP_MD_CTX_free(ctx);

        std::stringstream ss;
        for (unsigned int i = 0; i < hashLen; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }

    std::vector<BlockSignature> DeltaEngine::calculateSignature(const std::string& filePath) {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();

        logger.log(LogLevel::DEBUG, "Calculating signature for: " + filePath, "DeltaEngine");

        std::vector<BlockSignature> signatures;
        std::ifstream file(filePath, std::ios::binary);

        if (!file) {
            logger.log(LogLevel::ERROR, "Failed to open file for signature calculation: " + filePath, "DeltaEngine");
            metrics.incrementSyncErrors();
            return signatures;
        }
        
        // Get file size for adaptive block sizing
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        size_t blockSize = getOptimalBlockSize(fileSize);
        logger.log(LogLevel::DEBUG, "Using block size: " + std::to_string(blockSize) + 
                   " bytes for file size: " + std::to_string(fileSize), "DeltaEngine");

        // Batch processing: read multiple blocks before submitting to thread pool
        // This reduces task scheduling overhead
        constexpr size_t BATCH_SIZE = 16;  // Process 16 blocks per task
        
        // Use global thread pool to avoid repeated construction overhead
        ThreadPool& pool = ThreadPool::global();
        std::vector<std::future<void>> futures;
        std::mutex sigMutex;

        std::vector<std::pair<uint32_t, std::vector<uint8_t>>> batch;
        batch.reserve(BATCH_SIZE);
        uint32_t index = 0;

        auto processBatch = [&futures, &pool, &signatures, &sigMutex](
                std::vector<std::pair<uint32_t, std::vector<uint8_t>>>&& batchToProcess) {
            if (batchToProcess.empty()) return;
            
            futures.push_back(pool.enqueue([batch = std::move(batchToProcess), &signatures, &sigMutex]() {
                std::vector<BlockSignature> batchSigs;
                batchSigs.reserve(batch.size());
                
                for (const auto& item : batch) {
                    BlockSignature sig;
                    sig.index = item.first;
                    sig.adler32 = DeltaEngine::calculateAdler32(item.second.data(), item.second.size());
                    sig.sha256 = DeltaEngine::calculateSHA256(item.second.data(), item.second.size());
                    batchSigs.push_back(std::move(sig));
                }
                
                // Add batch results to shared vector
                std::lock_guard<std::mutex> lock(sigMutex);
                signatures.insert(signatures.end(), 
                                std::make_move_iterator(batchSigs.begin()),
                                std::make_move_iterator(batchSigs.end()));
            }));
        };

        std::vector<uint8_t> buffer(blockSize);
        while (file) {
            file.read(reinterpret_cast<char*>(buffer.data()), blockSize);
            size_t bytesRead = static_cast<size_t>(file.gcount());

            if (bytesRead == 0) break;

            std::vector<uint8_t> block(buffer.begin(), buffer.begin() + bytesRead);
            batch.emplace_back(index++, std::move(block));

            if (batch.size() >= BATCH_SIZE) {
                processBatch(std::move(batch));
                batch.clear();
                batch.reserve(BATCH_SIZE);
            }
        }

        // Process remaining blocks
        processBatch(std::move(batch));

        // Wait for all tasks to complete
        for (auto& fut : futures) {
            fut.wait();
        }

        std::sort(signatures.begin(), signatures.end(), [](const BlockSignature& a, const BlockSignature& b) {
            return a.index < b.index;
        });

        return signatures;
    }

    std::vector<DeltaInstruction> DeltaEngine::calculateDelta(const std::string& newFilePath, const std::vector<BlockSignature>& oldSignature) {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        
        logger.log(LogLevel::DEBUG, "Calculating delta for: " + newFilePath, "DeltaEngine");
        
        std::vector<DeltaInstruction> deltas;
        std::ifstream file(newFilePath, std::ios::binary);
        
        if (!file) {
            logger.log(LogLevel::ERROR, "Failed to open file for delta calculation: " + newFilePath, "DeltaEngine");
            metrics.incrementSyncErrors();
            return deltas;
        }

        // Map Adler32 to list of signatures (to handle collisions)
        std::unordered_map<uint32_t, std::vector<BlockSignature>> signatureMap;
        for (const auto& sig : oldSignature) {
            signatureMap[sig.adler32].push_back(sig);
        }

        // Sliding window over the new file using a bounded buffer.
        constexpr size_t MAX_BUFFER = BLOCK_SIZE * 4;
        std::vector<uint8_t> buffer(MAX_BUFFER);
        size_t bufStart = 0;  // index of first valid byte
        size_t bufSize = 0;   // number of valid bytes
        bool eof = false;

        std::vector<uint8_t> literalBuffer;

        auto fillBuffer = [&]() {
            // Compact if there is not enough space at the end for a full block
            if (bufStart > 0 && (bufStart + bufSize + BLOCK_SIZE) > MAX_BUFFER) {
                std::memmove(buffer.data(), buffer.data() + bufStart, bufSize);
                bufStart = 0;
            }

            while (!eof && bufSize < MAX_BUFFER) {
                file.read(reinterpret_cast<char*>(buffer.data() + bufStart + bufSize), MAX_BUFFER - (bufStart + bufSize));
                std::streamsize read = file.gcount();
                if (read <= 0) {
                    eof = true;
                    break;
                }
                bufSize += static_cast<size_t>(read);

                // Stop early once we have at least one full block
                if (bufSize >= BLOCK_SIZE) {
                    break;
                }
            }
        };

        fillBuffer();

        // Use rolling hash for efficient sliding window
        RollingAdler32 rollingHash;
        bool hashInitialized = false;
        size_t currentWindowSize = 0;

        while (bufSize > 0 || !eof) {
            if (bufSize == 0 && eof) {
                break;
            }

            if (bufSize < BLOCK_SIZE && !eof) {
                fillBuffer();
                if (bufSize == 0 && eof) {
                    break;
                }
            }

            if (bufSize == 0) {
                break;
            }

            size_t windowSize = (bufSize < BLOCK_SIZE) ? bufSize : BLOCK_SIZE;
            const uint8_t* base = buffer.data() + bufStart;

            // Initialize or update rolling hash
            uint32_t currentAdler;
            if (!hashInitialized || windowSize != currentWindowSize) {
                // First time or window size changed - compute from scratch
                rollingHash.init(base, windowSize);
                hashInitialized = true;
                currentWindowSize = windowSize;
                currentAdler = rollingHash.get();
            } else {
                currentAdler = rollingHash.get();
            }
            
            bool matchFound = false;
            auto it = signatureMap.find(currentAdler);
            if (it != signatureMap.end()) {
                std::string currentSHA = calculateSHA256(base, windowSize);
                for (const auto& sig : it->second) {
                    if (sig.sha256 == currentSHA) {
                        // Match found!
                        
                        // Flush literals if any
                        if (!literalBuffer.empty()) {
                            deltas.push_back({true, literalBuffer, 0});
                            literalBuffer.clear();
                        }

                        // Add block reference
                        deltas.push_back({false, {}, sig.index});
                        
                        bufStart += windowSize;
                        bufSize  -= windowSize;
                        matchFound = true;
                        // Reset hash - next window needs fresh computation
                        hashInitialized = false;
                        break;
                    }
                }
            }

            if (!matchFound) {
                // No matching block: emit one literal byte and advance by 1
                literalBuffer.push_back(*base);
                
                // Roll the hash forward by 1 byte if we have enough data
                if (bufSize > windowSize && hashInitialized) {
                    uint8_t oldByte = *base;
                    uint8_t newByte = *(base + windowSize);
                    rollingHash.roll(oldByte, newByte, windowSize);
                } else {
                    // Not enough data to roll, will reinitialize next iteration
                    hashInitialized = false;
                }
                
                bufStart += 1;
                bufSize  -= 1;
            }
        }

        // Flush remaining literals
        if (!literalBuffer.empty()) {
            deltas.push_back({true, literalBuffer, 0});
        }

        return deltas;
    }

    std::vector<uint8_t> DeltaEngine::applyDelta(const std::string& oldFilePath, const std::vector<DeltaInstruction>& deltas) {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        
        logger.log(LogLevel::DEBUG, "Applying delta to: " + oldFilePath, "DeltaEngine");
        
        std::ifstream oldFile(oldFilePath, std::ios::binary);
        if (!oldFile) {
            logger.log(LogLevel::ERROR, "Failed to open old file for patching: " + oldFilePath, "DeltaEngine");
            metrics.incrementSyncErrors();
            return {};
        }

        // Get file size to estimate output size and check for large files
        oldFile.seekg(0, std::ios::end);
        std::streamsize fileSize = oldFile.tellg();
        oldFile.seekg(0, std::ios::beg);
        
        // For very large files (>100MB), use streaming approach
        constexpr std::streamsize LARGE_FILE_THRESHOLD = 100 * 1024 * 1024;
        
        std::vector<uint8_t> newData;
        
        if (fileSize > LARGE_FILE_THRESHOLD) {
            // Streaming approach for large files - read blocks on demand
            logger.log(LogLevel::INFO, "Using streaming delta apply for large file (" + 
                      std::to_string(fileSize / (1024 * 1024)) + " MB)", "DeltaEngine");
            
            // Reserve estimated size
            newData.reserve(static_cast<size_t>(fileSize));
            
            for (const auto& delta : deltas) {
                if (delta.isLiteral) {
                    newData.insert(newData.end(), delta.literalData.begin(), delta.literalData.end());
                } else {
                    // Seek and read block on demand
                    std::streamoff offset = static_cast<std::streamoff>(delta.blockIndex) * BLOCK_SIZE;
                    if (offset >= fileSize) {
                        logger.log(LogLevel::ERROR, "Block index out of bounds: " + std::to_string(delta.blockIndex), "DeltaEngine");
                        continue;
                    }
                    
                    oldFile.seekg(offset);
                    size_t len = BLOCK_SIZE;
                    if (offset + static_cast<std::streamoff>(len) > fileSize) {
                        len = static_cast<size_t>(fileSize - offset);
                    }
                    
                    std::vector<uint8_t> block(len);
                    oldFile.read(reinterpret_cast<char*>(block.data()), static_cast<std::streamsize>(len));
                    newData.insert(newData.end(), block.begin(), block.end());
                }
            }
        } else {
            // Original approach for smaller files - read entire file into memory
            std::vector<uint8_t> oldData(static_cast<size_t>(fileSize));
            oldFile.read(reinterpret_cast<char*>(oldData.data()), fileSize);

            for (const auto& delta : deltas) {
                if (delta.isLiteral) {
                    newData.insert(newData.end(), delta.literalData.begin(), delta.literalData.end());
                } else {
                    // Copy block from old file
                    size_t offset = delta.blockIndex * BLOCK_SIZE;
                    if (offset >= oldData.size()) {
                        logger.log(LogLevel::ERROR, "Block index out of bounds: " + std::to_string(delta.blockIndex), "DeltaEngine");
                        continue;
                    }
                    size_t len = BLOCK_SIZE;
                    if (offset + len > oldData.size()) {
                        len = oldData.size() - offset;
                    }
                    newData.insert(newData.end(), oldData.begin() + offset, oldData.begin() + offset + len);
                }
            }
        }

        return newData;
    }

}
