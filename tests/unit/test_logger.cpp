#include "Logger.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

using namespace SentinelFS;

void test_singleton() {
    std::cout << "Running test_singleton..." << std::endl;
    Logger& logger1 = Logger::instance();
    Logger& logger2 = Logger::instance();
    assert(&logger1 == &logger2);
    std::cout << "test_singleton passed." << std::endl;
}

void test_file_logging() {
    std::cout << "Running test_file_logging..." << std::endl;
    std::string logFile = "test_log.txt";
    
    // Clean up previous run
    if (std::filesystem::exists(logFile)) {
        std::filesystem::remove(logFile);
    }
    
    Logger& logger = Logger::instance();
    logger.setLogFile(logFile);
    logger.setLevel(LogLevel::DEBUG);
    
    logger.info("Test info message");
    logger.error("Test error message");
    
    // Give it a moment to flush (though implementation likely flushes)
    // Logger implementation usually keeps file open. 
    // We might need to force flush or just read it.
    // Since Logger is a singleton and we can't easily destruct it to close file,
    // we rely on flush.
    
    std::ifstream file(logFile);
    assert(file.is_open());
    
    std::string line;
    bool foundInfo = false;
    bool foundError = false;
    
    while (std::getline(file, line)) {
        if (line.find("Test info message") != std::string::npos) foundInfo = true;
        if (line.find("Test error message") != std::string::npos) foundError = true;
    }
    
    assert(foundInfo);
    assert(foundError);
    
    // Clean up
    // Note: Can't easily remove file if it's still open by Logger singleton.
    // This is a limitation of the Singleton pattern in tests.
    // We'll leave it or try to set log file to something else.
    logger.setLogFile("/dev/null"); // Try to release the file
    
    std::cout << "test_file_logging passed." << std::endl;
}

int main() {
    try {
        test_singleton();
        test_file_logging();
        std::cout << "All Logger tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
