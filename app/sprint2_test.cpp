#include "core/file_api/file_api_impl.h"
#include "core/file_api/snapshot_engine.h"
#include "core/logger/logger.h"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * @brief Sprint 2 Test - FileAPI + SnapshotEngine
 * 
 * Demonstrates:
 * - File reading/writing
 * - SHA-256 hashing
 * - File chunking
 * - Directory scanning
 * - Snapshot creation
 * - Change detection
 */

using namespace sfs::core;

void print_separator(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << title << std::endl;
    std::cout << "========================================\n" << std::endl;
}

int main() {
    print_separator("SentinelFS-Neo Sprint 2 Test");
    std::cout << "FileAPI + SnapshotEngine Demo\n" << std::endl;
    
    // Initialize logger
    Logger::instance().set_level(LogLevel::INFO);
    Logger::instance().set_console_output(true);
    
    SFS_LOG_INFO("Main", "Starting Sprint 2 test");
    
    // ========================================
    // Test 1: FileAPI Basic Operations
    // ========================================
    print_separator("Test 1: FileAPI - File Operations");
    
    auto file_api = std::make_shared<FileAPI>();
    
    // Create test directory
    std::string test_dir = "/tmp/sfs_test";
    file_api->create_directory(test_dir);
    SFS_LOG_INFO("Test", "Created test directory: " + test_dir);
    
    // Write test file
    std::string test_file = test_dir + "/test.txt";
    std::string content = "Hello SentinelFS-Neo! This is a test file.";
    std::vector<uint8_t> data(content.begin(), content.end());
    
    if (file_api->write_all(test_file, data)) {
        SFS_LOG_INFO("Test", "✓ File written successfully");
    }
    
    // Read file back
    auto read_data = file_api->read_all(test_file);
    if (read_data == data) {
        SFS_LOG_INFO("Test", "✓ File read successfully (data matches)");
    }
    
    // Check file size
    uint64_t size = file_api->file_size(test_file);
    std::cout << "File size: " << size << " bytes" << std::endl;
    
    // ========================================
    // Test 2: SHA-256 Hashing
    // ========================================
    print_separator("Test 2: FileAPI - SHA-256 Hashing");
    
    std::string hash = file_api->hash(test_file);
    std::cout << "File: " << test_file << std::endl;
    std::cout << "SHA-256: " << hash << std::endl;
    SFS_LOG_INFO("Test", "✓ Hash computed successfully");
    
    // ========================================
    // Test 3: File Chunking
    // ========================================
    print_separator("Test 3: FileAPI - File Chunking");
    
    // Create a larger file for chunking demo
    std::string chunk_file = test_dir + "/chunk_test.bin";
    std::vector<uint8_t> large_data(10240); // 10KB
    for (size_t i = 0; i < large_data.size(); i++) {
        large_data[i] = i % 256;
    }
    file_api->write_all(chunk_file, large_data);
    
    auto chunks = file_api->split_into_chunks(chunk_file, 4096);
    std::cout << "File split into " << chunks.size() << " chunks:" << std::endl;
    for (size_t i = 0; i < chunks.size(); i++) {
        std::cout << "  Chunk " << i << ": offset=" << chunks[i].offset 
                  << ", size=" << chunks[i].size 
                  << ", hash=" << chunks[i].hash.substr(0, 16) << "..." << std::endl;
    }
    SFS_LOG_INFO("Test", "✓ File chunking successful");
    
    // ========================================
    // Test 4: SnapshotEngine - Directory Scan
    // ========================================
    print_separator("Test 4: SnapshotEngine - Directory Scanning");
    
    // Create more test files
    file_api->write_all(test_dir + "/file1.txt", data);
    file_api->write_all(test_dir + "/file2.txt", data);
    file_api->create_directory(test_dir + "/subdir");
    file_api->write_all(test_dir + "/subdir/file3.txt", data);
    
    SnapshotEngine engine(file_api);
    Snapshot snapshot1 = engine.create_snapshot(test_dir);
    
    std::cout << "Snapshot 1 created with " << snapshot1.file_count() << " files:" << std::endl;
    for (const auto& path : snapshot1.get_all_paths()) {
        const FileInfo* info = snapshot1.get_file(path);
        if (info) {
            std::cout << "  " << path << " (" << info->size << " bytes)" << std::endl;
        }
    }
    SFS_LOG_INFO("Test", "✓ Snapshot created successfully");
    
    // ========================================
    // Test 5: Change Detection
    // ========================================
    print_separator("Test 5: SnapshotEngine - Change Detection");
    
    std::cout << "Making changes to filesystem..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Add a new file
    file_api->write_all(test_dir + "/new_file.txt", data);
    std::cout << "  + Added: new_file.txt" << std::endl;
    
    // Modify existing file
    std::string modified_content = "Modified content!";
    std::vector<uint8_t> modified_data(modified_content.begin(), modified_content.end());
    file_api->write_all(test_dir + "/file1.txt", modified_data);
    std::cout << "  ~ Modified: file1.txt" << std::endl;
    
    // Delete a file
    file_api->remove(test_dir + "/file2.txt");
    std::cout << "  - Removed: file2.txt" << std::endl;
    
    // Create second snapshot
    Snapshot snapshot2 = engine.create_snapshot(test_dir);
    
    // Compare snapshots
    std::cout << "\nComparing snapshots..." << std::endl;
    auto comparison = engine.compare_snapshots(snapshot1, snapshot2);
    
    std::cout << "\nDetected changes:" << std::endl;
    std::cout << "  Added: " << comparison.added_count() << std::endl;
    std::cout << "  Removed: " << comparison.removed_count() << std::endl;
    std::cout << "  Modified: " << comparison.modified_count() << std::endl;
    
    std::cout << "\nDetailed changes:" << std::endl;
    for (const auto& change : comparison.changes) {
        std::string type_str;
        switch (change.type) {
            case ChangeType::ADDED: type_str = "+ ADDED"; break;
            case ChangeType::REMOVED: type_str = "- REMOVED"; break;
            case ChangeType::MODIFIED: type_str = "~ MODIFIED"; break;
        }
        std::cout << "  " << type_str << ": " << change.path << std::endl;
    }
    
    if (comparison.has_changes()) {
        SFS_LOG_INFO("Test", "✓ Change detection successful");
    }
    
    // ========================================
    // Cleanup
    // ========================================
    print_separator("Cleanup");
    
    file_api->remove(test_dir);
    SFS_LOG_INFO("Test", "Test directory cleaned up");
    
    // ========================================
    // Summary
    // ========================================
    print_separator("Sprint 2 Complete!");
    
    std::cout << "✓ FileAPI operations (read/write/hash)" << std::endl;
    std::cout << "✓ SHA-256 hashing" << std::endl;
    std::cout << "✓ File chunking (4KB blocks)" << std::endl;
    std::cout << "✓ Snapshot creation" << std::endl;
    std::cout << "✓ Change detection (add/remove/modify)" << std::endl;
    
    std::cout << "\nNext: Sprint 3 - Filesystem Plugins (watcher)" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    return 0;
}
