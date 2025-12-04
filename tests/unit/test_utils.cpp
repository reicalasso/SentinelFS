#include "PathUtils.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <string>

using namespace SentinelFS;

void test_paths() {
    std::cout << "Running test_paths..." << std::endl;
    
    auto home = PathUtils::getHome();
    assert(!home.empty());
    
    auto configDir = PathUtils::getConfigDir();
    assert(!configDir.empty());
    
    auto dataDir = PathUtils::getDataDir();
    assert(!dataDir.empty());
    
    // Just check they don't crash and return something reasonable
    std::cout << "Home: " << home << std::endl;
    std::cout << "Config: " << configDir << std::endl;
    
    std::cout << "test_paths passed." << std::endl;
}

void test_ensure_directory() {
    std::cout << "Running test_ensure_directory..." << std::endl;
    
    std::filesystem::path testDir = "test_dir_created";
    if (std::filesystem::exists(testDir)) {
        std::filesystem::remove(testDir);
    }
    
    PathUtils::ensureDirectory(testDir);
    assert(std::filesystem::exists(testDir));
    assert(std::filesystem::is_directory(testDir));
    
    std::filesystem::remove(testDir);
    std::cout << "test_ensure_directory passed." << std::endl;
}

int main() {
    try {
        test_paths();
        test_ensure_directory();
        std::cout << "All PathUtils tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
