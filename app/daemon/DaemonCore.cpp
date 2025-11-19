#include "DaemonCore.h"
#include <iostream>
#include <csignal>
#include <filesystem>
#include <thread>

namespace SentinelFS {

namespace {
    std::atomic<bool> signalReceived{false};
    
    void signalHandler(int signal) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        signalReceived = true;
    }
}

DaemonCore::DaemonCore(const DaemonConfig& config)
    : config_(config)
{
}

DaemonCore::~DaemonCore() {
    shutdown();
}

bool DaemonCore::initialize() {
    std::cout << "SentinelFS Daemon Starting..." << std::endl;
    
    printConfiguration();
    
    // Print current working directory
    std::cout << "Current Working Directory: " << std::filesystem::current_path() << std::endl;
    
    // Load plugins
    if (!loadPlugins()) {
        std::cerr << "Failed to load plugins" << std::endl;
        return false;
    }
    
    // Configure network plugin
    if (!config_.sessionCode.empty()) {
        network_->setSessionCode(config_.sessionCode);
    }
    
    if (config_.encryptionEnabled) {
        network_->setEncryptionEnabled(true);
    }
    
    // Setup event routing
    setupEventHandlers();
    
    // Start network services
    network_->startListening(config_.tcpPort);
    network_->startDiscovery(config_.discoveryPort);
    
    // Start filesystem monitoring
    filesystem_->startWatching(config_.watchDirectory);
    
    return true;
}

void DaemonCore::run() {
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    running_ = true;
    
    std::cout << "\nDaemon running. Press Ctrl+C to stop." << std::endl;
    
    // Main loop - just keep alive and handle signals
    while (running_ && !signalReceived) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    shutdown();
}

void DaemonCore::shutdown() {
    if (!running_) return;
    
    std::cout << "Shutting down daemon..." << std::endl;
    running_ = false;
    
    if (mlPlugin_) mlPlugin_->shutdown();
    if (filesystem_) filesystem_->shutdown();
    if (network_) network_->shutdown();
    if (storage_) storage_->shutdown();
    
    std::cout << "Daemon stopped." << std::endl;
}

bool DaemonCore::loadPlugins() {
    std::string pluginDir = "./plugins";
    std::cout << "Loading plugins from: " << pluginDir << std::endl;
    
    // Load plugins
    auto storagePlugin = loader_.loadPlugin(pluginDir + "/storage/libstorage_plugin.so", &eventBus_);
    auto networkPlugin = loader_.loadPlugin(pluginDir + "/network/libnetwork_plugin.so", &eventBus_);
    auto filesystemPlugin = loader_.loadPlugin(pluginDir + "/filesystem/libfilesystem_plugin.so", &eventBus_);
    mlPlugin_ = loader_.loadPlugin(pluginDir + "/ml/libml_plugin.so", &eventBus_);
    
    // Cast to specific interfaces using dynamic_pointer_cast
    storage_ = std::dynamic_pointer_cast<IStorageAPI>(storagePlugin);
    network_ = std::dynamic_pointer_cast<INetworkAPI>(networkPlugin);
    filesystem_ = std::dynamic_pointer_cast<IFileAPI>(filesystemPlugin);
    
    // Verify critical plugins loaded
    if (!storage_ || !network_ || !filesystem_) {
        std::cerr << "Failed to load one or more critical plugins" << std::endl;
        
        std::cout << "\nChecking plugin paths:" << std::endl;
        std::cout << "  Storage: " << (storage_ ? "OK" : "NOT FOUND") 
                  << " at \"" << pluginDir << "/storage/libstorage_plugin.so\"" << std::endl;
        std::cout << "  Network: " << (network_ ? "OK" : "NOT FOUND")
                  << " at \"" << pluginDir << "/network/libnetwork_plugin.so\"" << std::endl;
        std::cout << "  Filesystem: " << (filesystem_ ? "OK" : "NOT FOUND")
                  << " at \"" << pluginDir << "/filesystem/libfilesystem_plugin.so\"" << std::endl;
        std::cout << "  ML: " << (mlPlugin_ ? "OK" : "NOT FOUND")
                  << " at \"" << pluginDir << "/ml/libml_plugin.so\"" << std::endl;
        
        return false;
    }
    
    return true;
}

void DaemonCore::setupEventHandlers() {
    // Event handlers are now in EventHandlers class
    // This method is kept for internal initialization if needed
}

void DaemonCore::printConfiguration() const {
    std::cout << "Configuration:" << std::endl;
    std::cout << "  TCP Port: " << config_.tcpPort << std::endl;
    std::cout << "  Discovery Port: " << config_.discoveryPort << std::endl;
    std::cout << "  Watch Directory: " << config_.watchDirectory << std::endl;
    std::cout << "  Encryption: " << (config_.encryptionEnabled ? "Enabled" : "Disabled") << std::endl;
    std::cout << "  Upload Limit: " 
              << (config_.uploadLimit > 0 ? std::to_string(config_.uploadLimit / 1024) + " KB/s" : "Unlimited") 
              << std::endl;
    std::cout << "  Download Limit: " 
              << (config_.downloadLimit > 0 ? std::to_string(config_.downloadLimit / 1024) + " KB/s" : "Unlimited") 
              << std::endl;
    
    if (!config_.sessionCode.empty()) {
        std::cout << "  Session Code: Set ✓" << std::endl;
        
        if (config_.encryptionEnabled) {
            std::cout << "  🔒 Encryption key will be derived from session code" << std::endl;
        }
    } else {
        std::cout << "  Session Code: Not set (any peer can connect)" << std::endl;
        std::cout << "  ⚠️  WARNING: For security, use --generate-code to create a session code!" << std::endl;
        
        if (config_.encryptionEnabled) {
            std::cerr << "Error: Cannot enable encryption without a session code!" << std::endl;
        }
    }
}

} // namespace SentinelFS
