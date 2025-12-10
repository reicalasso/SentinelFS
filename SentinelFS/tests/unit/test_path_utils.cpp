#include "PathUtils.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace SentinelFS;

void test_path_utils() {
    std::cout << "Running test_path_utils..." << std::endl;
    
    std::filesystem::path home = PathUtils::getHome();
    std::cout << "Home: " << home << std::endl;
    assert(!home.empty());
    
    std::filesystem::path config = PathUtils::getConfigDir();
    std::cout << "Config: " << config << std::endl;
    assert(!config.empty());
    
    std::filesystem::path data = PathUtils::getDataDir();
    std::cout << "Data: " << data << std::endl;
    assert(!data.empty());
    
    std::cout << "test_path_utils passed." << std::endl;
}

int main() {
    try {
        test_path_utils();
        std::cout << "All PathUtils tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
