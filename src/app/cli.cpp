#include "cli.hpp"
#include <iostream>

CLI::CLI() = default;

CLI::~CLI() = default;

Config CLI::parseArguments(int argc, char* argv[]) {
    Config config = defaultConfig;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--session" && i + 1 < argc) {
            config.sessionCode = argv[++i];
        } 
        else if (arg == "--path" && i + 1 < argc) {
            config.syncPath = argv[++i];
        } 
        else if (arg == "--port" && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } 
        else if (arg == "--verbose") {
            config.verbose = true;
        } 
        else if (arg == "--daemon") {
            config.daemonMode = true;
        } 
        else if (arg == "--config" && i + 1 < argc) {
            config.configFile = argv[++i];
        } 
        else if (arg == "--help") {
            printUsage();
            exit(0);
        } 
        else if (arg == "--version") {
            printVersion();
            exit(0);
        } 
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage();
            exit(1);
        }
    }
    
    // Validate required arguments
    if (config.sessionCode.empty()) {
        std::cerr << "Error: --session is required" << std::endl;
        printUsage();
        exit(1);
    }
    
    if (config.syncPath.empty()) {
        std::cerr << "Error: --path is required" << std::endl;
        printUsage();
        exit(1);
    }
    
    return config;
}

void CLI::printUsage() {
    std::cout << R"(
Usage: sentinelfs-neo [OPTIONS]

Options:
  --session <CODE>      Session code (required)
  --path <PATH>         Directory to sync (required)
  --port <PORT>         Network port (default: 8080)
  --verbose             Verbose logging
  --daemon              Run as daemon
  --config <FILE>       Configuration file
  --help                Show this help
  --version             Show version
)" << std::endl;
}

void CLI::printVersion() {
    std::cout << "SentinelFS-Neo v1.0.0" << std::endl;
}