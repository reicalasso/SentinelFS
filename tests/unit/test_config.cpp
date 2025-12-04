#include "Config.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

using namespace SentinelFS;

void test_basic_operations() {
    std::cout << "Running test_basic_operations..." << std::endl;
    Config config;
    
    config.set("key1", "value1");
    assert(config.get("key1") == "value1");
    assert(config.hasKey("key1"));
    assert(!config.hasKey("key2"));
    
    config.setInt("intKey", 42);
    assert(config.getInt("intKey") == 42);
    
    config.setBool("boolKey", true);
    assert(config.getBool("boolKey") == true);
    
    config.setDouble("doubleKey", 3.14);
    assert(std::abs(config.getDouble("doubleKey") - 3.14) < 0.001);
    
    std::cout << "test_basic_operations passed." << std::endl;
}

void test_file_operations() {
    std::cout << "Running test_file_operations..." << std::endl;
    Config config;
    config.set("server_port", "8080");
    config.set("enable_logging", "true");
    
    std::string testFile = "test_config.conf";
    config.saveToFile(testFile);
    
    Config config2;
    bool loaded = config2.loadFromFile(testFile);
    assert(loaded);
    assert(config2.get("server_port") == "8080");
    assert(config2.get("enable_logging") == "true");
    
    std::filesystem::remove(testFile);
    std::cout << "test_file_operations passed." << std::endl;
}

void test_validation() {
    std::cout << "Running test_validation..." << std::endl;
    Config config;
    config.set("port", "8080");
    config.set("host", "localhost");
    
    std::unordered_map<std::string, Config::Validator> schema;
    schema["port"] = [](const std::string& k, const std::string& v) {
        try {
            int port = std::stoi(v);
            return port > 0 && port < 65536;
        } catch (...) {
            return false;
        }
    };
    
    assert(config.validate(schema));
    
    config.set("port", "70000"); // Invalid port
    assert(!config.validate(schema));
    
    std::cout << "test_validation passed." << std::endl;
}

int main() {
    try {
        test_basic_operations();
        test_file_operations();
        test_validation();
        std::cout << "All Config tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
