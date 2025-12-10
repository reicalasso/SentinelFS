/**
 * @file delta_fuzz.cpp
 * @brief Fuzzing tests for delta sync protocol
 * 
 * Build with: clang++ -g -fsanitize=fuzzer,address delta_fuzz.cpp -o delta_fuzz
 * Run: ./delta_fuzz corpus/ -max_len=65536
 */

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

// Minimal delta parsing for fuzzing
namespace {

struct DeltaHeader {
    uint8_t messageType;
    uint32_t payloadSize;
    uint32_t chunkIndex;
    uint32_t totalChunks;
};

bool parseDeltaHeader(const uint8_t* data, size_t size, DeltaHeader& header) {
    if (size < 13) return false;
    
    header.messageType = data[0];
    header.payloadSize = *reinterpret_cast<const uint32_t*>(data + 1);
    header.chunkIndex = *reinterpret_cast<const uint32_t*>(data + 5);
    header.totalChunks = *reinterpret_cast<const uint32_t*>(data + 9);
    
    // Sanity checks
    if (header.payloadSize > 100 * 1024 * 1024) return false; // Max 100MB
    if (header.totalChunks > 10000) return false; // Max 10k chunks
    if (header.chunkIndex >= header.totalChunks && header.totalChunks > 0) return false;
    
    return true;
}

bool parseUpdateAvailable(const uint8_t* data, size_t size) {
    // Format: relativePath|hash|size
    std::string payload(reinterpret_cast<const char*>(data), size);
    
    size_t firstPipe = payload.find('|');
    if (firstPipe == std::string::npos) return false;
    
    size_t secondPipe = payload.find('|', firstPipe + 1);
    if (secondPipe == std::string::npos) return false;
    
    std::string path = payload.substr(0, firstPipe);
    std::string hash = payload.substr(firstPipe + 1, secondPipe - firstPipe - 1);
    std::string sizeStr = payload.substr(secondPipe + 1);
    
    // Validate
    if (path.empty() || path.length() > 4096) return false;
    if (hash.length() != 64) return false; // SHA256 hex
    
    try {
        std::stoll(sizeStr);
    } catch (...) {
        return false;
    }
    
    return true;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) return 0;
    
    // Test delta header parsing
    DeltaHeader header;
    if (parseDeltaHeader(data, size, header)) {
        // Valid header - test payload parsing based on message type
        if (size > 13) {
            const uint8_t* payload = data + 13;
            size_t payloadSize = size - 13;
            
            switch (header.messageType) {
                case 1: // UPDATE_AVAILABLE
                    parseUpdateAvailable(payload, payloadSize);
                    break;
                case 2: // REQUEST_DELTA
                case 3: // DELTA_DATA
                case 4: // REQUEST_FILE
                case 5: // FILE_DATA
                case 6: // DELETE_FILE
                    // Just ensure no crash
                    break;
            }
        }
    }
    
    return 0;
}

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
// Standalone test mode
int main(int argc, char** argv) {
    // Test with some sample inputs
    uint8_t sample1[] = {1, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 1, 't', 'e', 's', 't'};
    LLVMFuzzerTestOneInput(sample1, sizeof(sample1));
    
    return 0;
}
#endif
