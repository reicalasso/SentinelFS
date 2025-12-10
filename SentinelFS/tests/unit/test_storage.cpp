#include "FileVersionManager.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

using namespace SentinelFS;

void test_version_struct() {
    std::cout << "Running test_version_struct..." << std::endl;
    
    FileVersion v1;
    v1.timestamp = 100;
    
    FileVersion v2;
    v2.timestamp = 200;
    
    assert(v1 < v2);
    
    std::cout << "test_version_struct passed." << std::endl;
}

void test_manager_init() {
    std::cout << "Running test_manager_init..." << std::endl;
    
    std::string watchDir = "test_watch_dir";
    std::filesystem::create_directory(watchDir);
    
    VersioningConfig config;
    config.versionStoragePath = ".versions";
    
    FileVersionManager manager(watchDir, config);
    
    // Should create version dir
    assert(std::filesystem::exists(watchDir + "/.versions"));
    
    std::filesystem::remove_all(watchDir);
    std::cout << "test_manager_init passed." << std::endl;
}

int main() {
    try {
        test_version_struct();
        test_manager_init();
        std::cout << "All FileVersionManager tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
