#include "core/logger/logger.h"
#include "core/delta_engine/delta_engine.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>

using namespace sfs::core;

// Helper function to read file into memory
std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

// Helper function to write file
bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

// Helper function to create test data
std::vector<uint8_t> create_test_data(size_t size, const std::string& pattern) {
    std::vector<uint8_t> data;
    data.reserve(size);
    
    while (data.size() < size) {
        for (char c : pattern) {
            if (data.size() >= size) break;
            data.push_back(static_cast<uint8_t>(c));
        }
    }
    
    return data;
}

void print_delta_stats(const DeltaResult& delta) {
    std::cout << "\n📊 Delta Statistics:\n";
    std::cout << "  Original Size:    " << delta.original_size << " bytes\n";
    std::cout << "  Delta Size:       " << delta.delta_size << " bytes\n";
    std::cout << "  Matched Blocks:   " << delta.matched_blocks << "\n";
    std::cout << "  Literal Bytes:    " << delta.literal_bytes << "\n";
    std::cout << "  Compression:      " << std::fixed << std::setprecision(1) 
              << (delta.compression_ratio() * 100) << "%\n";
    std::cout << "  Operations:       " << delta.operations.size() << "\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Sprint 4 - Delta Engine Test\n";
    std::cout << "========================================\n\n";

    auto& logger = Logger::instance();
    logger.set_level(LogLevel::INFO);
    logger.info("Test", "Starting Delta Engine tests");

    // Create delta engine
    RsyncDeltaEngine engine;

    // Test 1: Identical files
    {
        std::cout << "\n🧪 Test 1: Identical Files\n";
        std::cout << "----------------------------------------\n";
        
        auto data = create_test_data(16384, "Hello World! ");
        
        auto signatures = engine.generate_signatures(data.data(), data.size());
        std::cout << "✓ Generated " << signatures.size() << " block signatures\n";
        
        auto delta = engine.compute_delta(data.data(), data.size(), signatures);
        print_delta_stats(delta);
        
        if (delta.matched_blocks == signatures.size() && delta.literal_bytes == 0) {
            std::cout << "✅ Perfect match - no changes needed!\n";
        } else {
            std::cout << "❌ Expected all blocks to match!\n";
            return 1;
        }
    }

    // Test 2: Modified file
    {
        std::cout << "\n🧪 Test 2: Modified File (Single Block)\n";
        std::cout << "----------------------------------------\n";
        
        auto base = create_test_data(16384, "Original data ");
        auto modified = base;
        
        // Modify one block in the middle
        size_t mod_start = 8192;
        std::string modification = "MODIFIED BLOCK!!!";
        for (size_t i = 0; i < modification.size() && mod_start + i < modified.size(); ++i) {
            modified[mod_start + i] = modification[i];
        }
        
        auto signatures = engine.generate_signatures(base.data(), base.size());
        auto delta = engine.compute_delta(modified.data(), modified.size(), signatures);
        print_delta_stats(delta);
        
        // Apply delta
        std::vector<uint8_t> reconstructed;
        if (engine.apply_delta(base.data(), base.size(), delta, reconstructed)) {
            if (reconstructed == modified) {
                std::cout << "✅ Delta applied successfully - file reconstructed!\n";
            } else {
                std::cout << "❌ Reconstructed file doesn't match!\n";
                return 1;
            }
        } else {
            std::cout << "❌ Failed to apply delta!\n";
            return 1;
        }
    }

    // Test 3: Completely different file
    {
        std::cout << "\n🧪 Test 3: Completely Different File\n";
        std::cout << "----------------------------------------\n";
        
        auto base = create_test_data(8192, "AAAAAAA");
        auto different = create_test_data(8192, "BBBBBBB");
        
        auto signatures = engine.generate_signatures(base.data(), base.size());
        auto delta = engine.compute_delta(different.data(), different.size(), signatures);
        print_delta_stats(delta);
        
        if (delta.matched_blocks == 0) {
            std::cout << "✅ No matches found (as expected for different files)\n";
        }
    }

    // Test 4: Rolling checksum
    {
        std::cout << "\n🧪 Test 4: Rolling Checksum Algorithm\n";
        std::cout << "----------------------------------------\n";
        
        std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        size_t window_size = 4;
        
        // Compute initial checksum
        auto checksum1 = engine.compute_weak_checksum(data.data(), window_size);
        std::cout << "Initial checksum [1,2,3,4]: 0x" << std::hex << checksum1 << std::dec << "\n";
        
        // Roll window and update checksum
        auto checksum2 = engine.update_rolling_checksum(checksum1, data[0], data[4], window_size);
        std::cout << "Rolled checksum [2,3,4,5]:  0x" << std::hex << checksum2 << std::dec << "\n";
        
        // Verify by computing from scratch
        auto checksum2_verify = engine.compute_weak_checksum(data.data() + 1, window_size);
        std::cout << "Verify checksum [2,3,4,5]:  0x" << std::hex << checksum2_verify << std::dec << "\n";
        
        if (checksum2 == checksum2_verify) {
            std::cout << "✅ Rolling checksum works correctly!\n";
        } else {
            std::cout << "❌ Rolling checksum mismatch!\n";
            return 1;
        }
    }

    // Test 5: Strong hash (SHA-256)
    {
        std::cout << "\n🧪 Test 5: Strong Hash (SHA-256)\n";
        std::cout << "----------------------------------------\n";
        
        std::string data = "Hello, SentinelFS!";
        auto hash = engine.compute_strong_hash(
            reinterpret_cast<const uint8_t*>(data.data()), 
            data.size()
        );
        
        std::cout << "Data: \"" << data << "\"\n";
        std::cout << "SHA-256: " << hash.to_hex() << "\n";
        
        // Compute same hash again
        auto hash2 = engine.compute_strong_hash(
            reinterpret_cast<const uint8_t*>(data.data()), 
            data.size()
        );
        
        if (hash == hash2) {
            std::cout << "✅ Strong hash is deterministic!\n";
        } else {
            std::cout << "❌ Hash mismatch!\n";
            return 1;
        }
    }

    // Test 6: Large file simulation
    {
        std::cout << "\n🧪 Test 6: Large File Simulation (1MB)\n";
        std::cout << "----------------------------------------\n";
        
        size_t file_size = 1024 * 1024; // 1MB
        auto base = create_test_data(file_size, "LARGE FILE DATA PATTERN ");
        auto modified = base;
        
        // Modify 10 random blocks
        for (int i = 0; i < 10; ++i) {
            size_t offset = (i * file_size / 10);
            std::string mod = "MODIFIED_BLOCK_" + std::to_string(i);
            for (size_t j = 0; j < mod.size() && offset + j < modified.size(); ++j) {
                modified[offset + j] = mod[j];
            }
        }
        
        auto signatures = engine.generate_signatures(base.data(), base.size());
        std::cout << "✓ Generated signatures for " << (file_size / 1024) << "KB file\n";
        
        auto delta = engine.compute_delta(modified.data(), modified.size(), signatures);
        print_delta_stats(delta);
        
        // Apply delta
        std::vector<uint8_t> reconstructed;
        if (engine.apply_delta(base.data(), base.size(), delta, reconstructed)) {
            if (reconstructed == modified) {
                std::cout << "✅ Large file delta works perfectly!\n";
            } else {
                std::cout << "❌ Reconstruction failed!\n";
                return 1;
            }
        }
    }

    std::cout << "\n========================================\n";
    std::cout << "✅ Sprint 4 Complete!\n";
    std::cout << "========================================\n";
    std::cout << "\n🎉 Delta Engine Features:\n";
    std::cout << "  ✓ Adler-32 rolling checksum\n";
    std::cout << "  ✓ SHA-256 strong hashing\n";
    std::cout << "  ✓ Block signature generation\n";
    std::cout << "  ✓ Delta computation (rsync-style)\n";
    std::cout << "  ✓ Delta application & reconstruction\n";
    std::cout << "  ✓ Efficient incremental sync\n\n";

    return 0;
}
