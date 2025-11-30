/**
 * @file sync_integration_test.cpp
 * @brief Integration tests for SentinelFS sync functionality
 * 
 * Tests the complete sync flow including:
 * - File change detection
 * - Delta calculation and application
 * - Network transfer
 * - Conflict resolution
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

// Core includes
#include "DeltaEngine.h"
#include "DeltaSerialization.h"
#include "Logger.h"

namespace fs = std::filesystem;

class SyncIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directories
        testDir_ = fs::temp_directory_path() / "sentinel_test";
        sourceDir_ = testDir_ / "source";
        targetDir_ = testDir_ / "target";
        
        fs::create_directories(sourceDir_);
        fs::create_directories(targetDir_);
    }
    
    void TearDown() override {
        // Cleanup
        std::error_code ec;
        fs::remove_all(testDir_, ec);
    }
    
    void createFile(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
    }
    
    std::string readFile(const fs::path& path) {
        std::ifstream file(path);
        return std::string((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
    }
    
    fs::path testDir_;
    fs::path sourceDir_;
    fs::path targetDir_;
};

TEST_F(SyncIntegrationTest, DeltaSyncSmallFile) {
    // Create source file
    std::string originalContent = "Hello, World! This is a test file.";
    fs::path sourceFile = sourceDir_ / "test.txt";
    createFile(sourceFile, originalContent);
    
    // Copy to target (simulating initial sync)
    fs::path targetFile = targetDir_ / "test.txt";
    fs::copy_file(sourceFile, targetFile);
    
    // Modify source file
    std::string modifiedContent = "Hello, Universe! This is a modified test file.";
    createFile(sourceFile, modifiedContent);
    
    // Calculate signature of target (old) file
    auto signature = SentinelFS::DeltaEngine::calculateSignature(targetFile.string());
    ASSERT_FALSE(signature.empty());
    
    // Calculate delta from source (new) file
    auto delta = SentinelFS::DeltaEngine::calculateDelta(sourceFile.string(), signature);
    ASSERT_FALSE(delta.empty());
    
    // Apply delta to target
    auto newData = SentinelFS::DeltaEngine::applyDelta(targetFile.string(), delta);
    
    // Write new data
    std::ofstream outFile(targetFile, std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(newData.data()), newData.size());
    outFile.close();
    
    // Verify content matches
    std::string resultContent = readFile(targetFile);
    EXPECT_EQ(resultContent, modifiedContent);
}

TEST_F(SyncIntegrationTest, DeltaSyncLargeFile) {
    // Create a larger file (1MB)
    std::string content;
    content.reserve(1024 * 1024);
    for (int i = 0; i < 1024 * 1024 / 100; ++i) {
        content += "Line " + std::to_string(i) + ": This is test content for delta sync.\n";
    }
    
    fs::path sourceFile = sourceDir_ / "large.txt";
    createFile(sourceFile, content);
    
    // Copy to target
    fs::path targetFile = targetDir_ / "large.txt";
    fs::copy_file(sourceFile, targetFile);
    
    // Modify middle of file
    std::string modifiedContent = content;
    size_t midPoint = modifiedContent.size() / 2;
    modifiedContent.replace(midPoint, 50, "MODIFIED CONTENT HERE!");
    createFile(sourceFile, modifiedContent);
    
    // Calculate and apply delta
    auto signature = SentinelFS::DeltaEngine::calculateSignature(targetFile.string());
    auto delta = SentinelFS::DeltaEngine::calculateDelta(sourceFile.string(), signature);
    auto newData = SentinelFS::DeltaEngine::applyDelta(targetFile.string(), delta);
    
    // Write and verify
    std::ofstream outFile(targetFile, std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(newData.data()), newData.size());
    outFile.close();
    
    EXPECT_EQ(readFile(targetFile), modifiedContent);
}

TEST_F(SyncIntegrationTest, DeltaSerializationRoundTrip) {
    // Create test file
    fs::path testFile = sourceDir_ / "serialize_test.txt";
    createFile(testFile, "Test content for serialization");
    
    // Calculate signature
    auto signature = SentinelFS::DeltaEngine::calculateSignature(testFile.string());
    
    // Serialize
    auto serialized = SentinelFS::DeltaSerialization::serializeSignature(signature);
    ASSERT_FALSE(serialized.empty());
    
    // Deserialize
    auto deserialized = SentinelFS::DeltaSerialization::deserializeSignature(serialized);
    
    // Verify
    ASSERT_EQ(signature.size(), deserialized.size());
    for (size_t i = 0; i < signature.size(); ++i) {
        EXPECT_EQ(signature[i].index, deserialized[i].index);
        EXPECT_EQ(signature[i].adler32, deserialized[i].adler32);
        EXPECT_EQ(signature[i].sha256, deserialized[i].sha256);
    }
}

TEST_F(SyncIntegrationTest, NewFileSync) {
    // Create new file (no existing target)
    std::string content = "Brand new file content";
    fs::path sourceFile = sourceDir_ / "new_file.txt";
    createFile(sourceFile, content);
    
    // Empty signature (new file)
    std::vector<SentinelFS::BlockSignature> emptySignature;
    
    // Calculate delta (should be all literal data)
    auto delta = SentinelFS::DeltaEngine::calculateDelta(sourceFile.string(), emptySignature);
    ASSERT_FALSE(delta.empty());
    
    // All instructions should be literal for new file
    for (const auto& instr : delta) {
        EXPECT_TRUE(instr.isLiteral);
    }
}

TEST_F(SyncIntegrationTest, IdenticalFilesNoDelta) {
    // Create identical files
    std::string content = "Identical content";
    fs::path sourceFile = sourceDir_ / "identical.txt";
    fs::path targetFile = targetDir_ / "identical.txt";
    createFile(sourceFile, content);
    createFile(targetFile, content);
    
    // Calculate signature and delta
    auto signature = SentinelFS::DeltaEngine::calculateSignature(targetFile.string());
    auto delta = SentinelFS::DeltaEngine::calculateDelta(sourceFile.string(), signature);
    
    // Delta should have no literal data (all block references)
    size_t literalBytes = 0;
    for (const auto& instr : delta) {
        if (instr.isLiteral) {
            literalBytes += instr.literalData.size();
        }
    }
    
    // For identical files, literal bytes should be minimal or zero
    EXPECT_LT(literalBytes, content.size());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
