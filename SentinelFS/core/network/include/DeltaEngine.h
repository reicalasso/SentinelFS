#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace SentinelFS {

    struct BlockSignature {
        uint32_t index;
        uint32_t adler32;
        std::string sha256;
    };

    struct DeltaInstruction {
        bool isLiteral;
        std::vector<uint8_t> literalData; // If isLiteral is true
        uint32_t blockIndex;              // If isLiteral is false
    };

    class DeltaEngine {
    public:
        static const size_t BLOCK_SIZE = 4096;
        
        /**
         * @brief Get optimal block size based on file size
         * @param fileSize Total file size in bytes
         * @return Optimal block size for delta computation
         */
        static size_t getOptimalBlockSize(size_t fileSize) {
            if (fileSize < 1024 * 1024) {           // < 1MB
                return 32 * 1024;                    // 32KB
            } else if (fileSize < 100 * 1024 * 1024) { // < 100MB
                return 128 * 1024;                   // 128KB
            } else {
                return 256 * 1024;                   // 256KB for large files
            }
        }

        /**
         * @brief Calculate Adler-32 rolling checksum.
         * @param data Pointer to data buffer.
         * @param len Length of data.
         * @return 32-bit Adler checksum.
         */
        static uint32_t calculateAdler32(const uint8_t* data, size_t len);
        
        /**
         * @brief Rolling Adler-32 state for efficient sliding window computation
         */
        struct RollingAdler32 {
            uint32_t a = 1;
            uint32_t b = 0;
            static constexpr uint32_t MOD_ADLER = 65521;
            
            /**
             * @brief Initialize with a block of data
             */
            void init(const uint8_t* data, size_t len) {
                a = 1;
                b = 0;
                for (size_t i = 0; i < len; ++i) {
                    a = (a + data[i]) % MOD_ADLER;
                    b = (b + a) % MOD_ADLER;
                }
            }
            
            /**
             * @brief Roll the hash by removing oldByte and adding newByte
             * @param oldByte The byte leaving the window
             * @param newByte The byte entering the window
             * @param windowSize The size of the sliding window
             */
            void roll(uint8_t oldByte, uint8_t newByte, size_t windowSize) {
                // Remove contribution of oldByte
                a = (a - oldByte + newByte + MOD_ADLER) % MOD_ADLER;
                // b loses oldByte * windowSize and gains new 'a'
                b = (b - (windowSize * oldByte) % MOD_ADLER + a - 1 + MOD_ADLER) % MOD_ADLER;
            }
            
            /**
             * @brief Get the current hash value
             */
            uint32_t get() const {
                return (b << 16) | a;
            }
        };

        /**
         * @brief Calculate SHA-256 checksum.
         * @param data Pointer to data buffer.
         * @param len Length of data.
         * @return Hex string of SHA-256 hash.
         */
        static std::string calculateSHA256(const uint8_t* data, size_t len);
        
        /**
         * @brief Generates the signature (list of checksums) for a file.
         * @param filePath Path to the file.
         * @return Vector of BlockSignatures.
         */
        static std::vector<BlockSignature> calculateSignature(const std::string& filePath);

        /**
         * @brief Calculates the delta between a new file and an old signature.
         * @param newFilePath Path to the new version of the file.
         * @param oldSignature Signature of the old version of the file.
         * @return Vector of DeltaInstructions.
         */
        static std::vector<DeltaInstruction> calculateDelta(const std::string& newFilePath, const std::vector<BlockSignature>& oldSignature);

        /**
         * @brief Applies a delta to an old file to reconstruct the new file.
         * @param oldFilePath Path to the old version of the file.
         * @param deltas Vector of DeltaInstructions.
         * @return Reconstructed file data.
         */
        static std::vector<uint8_t> applyDelta(const std::string& oldFilePath, const std::vector<DeltaInstruction>& deltas);
    };
}
