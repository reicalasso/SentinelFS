/**
 * @file DaemonLifecycle.cpp
 * @brief DaemonCore constructor, destructor, lifecycle methods (initialize, run, shutdown)
 */

#include "DaemonCore.h"
#include "Logger.h"
#include "Config.h"
#include <iostream>
#include <csignal>
#include <filesystem>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <sqlite3.h>

namespace SentinelFS {

namespace {
    // Signal-safe: use volatile sig_atomic_t for guaranteed async-signal-safety
    volatile sig_atomic_t signalReceived = 0;
    volatile sig_atomic_t receivedSignalNum = 0;
    
    void signalHandler(int signal) {
        receivedSignalNum = signal;
        signalReceived = 1;
    }
    
    // Expand tilde in path to home directory
    std::string expandTilde(const std::string& path) {
        if (path.empty() || path[0] != '~') {
            return path;
        }
        
        const char* home = std::getenv("HOME");
        if (!home) {
            return path;
        }
        
        if (path.size() == 1) {
            return home;
        }
        
        if (path[1] == '/') {
            return std::string(home) + path.substr(1);
        }
        
        return path;
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
    auto& logger = Logger::instance();
    logger.info("SentinelFS Daemon initializing...", "DaemonCore");
    
    printConfiguration();
    
    logger.info("Current working directory: " + std::filesystem::current_path().string(), "DaemonCore");
    
    // Load plugins
    if (!loadPlugins()) {
        logger.error("Failed to load plugins", "DaemonCore");
        return false;
    }
    
    // Configure network plugin
    if (!config_.sessionCode.empty()) {
        network_->setSessionCode(config_.sessionCode);
        logger.info("Session code configured", "DaemonCore");
    }
    
    if (config_.encryptionEnabled) {
        network_->setEncryptionEnabled(true);
        logger.info("Encryption enabled", "DaemonCore");
    }

    if (config_.uploadLimit > 0) {
        network_->setGlobalUploadLimit(config_.uploadLimit);
        logger.info("Global upload limit configured from startup options", "DaemonCore");
    }

    if (config_.downloadLimit > 0) {
        network_->setGlobalDownloadLimit(config_.downloadLimit);
        logger.info("Global download limit configured from startup options", "DaemonCore");
    }
    
    // Setup event routing
    setupEventHandlers();
    
    // Set storage reference for ML plugin
    if (mlPlugin_ && storage_) {
        // Cast to access setStorage method
        auto* rawPlugin = mlPlugin_.get();
        // Use reflection/dynamic cast approach - ML plugin implements custom interface
        try {
            // Check if plugin has a setStorage method via dynamic invocation
            eventBus_.publish("ML_SET_STORAGE", storage_.get());
            logger.info("Storage reference set for ML plugin", "DaemonCore");
        } catch (const std::exception& e) {
            logger.warn("Could not set storage for ML plugin: " + std::string(e.what()), "DaemonCore");
        }
    }
    
    // Start network services
    try {
        network_->startListening(config_.tcpPort);
        logger.info("TCP listener started on port " + std::to_string(config_.tcpPort), "DaemonCore");
        
        network_->startDiscovery(config_.discoveryPort);
        logger.info("UDP discovery started on port " + std::to_string(config_.discoveryPort), "DaemonCore");
    } catch (const std::exception& e) {
        logger.error("Failed to start network services: " + std::string(e.what()), "DaemonCore");
        initStatus_.result = InitializationStatus::Result::NetworkFailure;
        initStatus_.message = std::string("Network startup failed: ") + e.what();
        return false;
    }
    
    // Start filesystem monitoring
    try {
        // Expand tilde in watch directory path
        std::string watchDir = expandTilde(config_.watchDirectory);
        
        if (!std::filesystem::exists(watchDir)) {
            std::filesystem::create_directories(watchDir);
            logger.info("Created watch directory: " + watchDir, "DaemonCore");
        }

        filesystem_->startWatching(watchDir);
        logger.info("Filesystem watcher started for: " + watchDir, "DaemonCore");

        // Load and watch persisted folders from database
        if (storage_) {
            sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
            const char* sql = "SELECT path FROM watched_folders WHERE status = 'active'";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    if (path) {
                        std::string pathStr = path;
                        // Avoid duplicate watch for default dir
                        if (pathStr != watchDir) {
                            filesystem_->startWatching(pathStr);
                            logger.info("Restored watch for: " + pathStr, "DaemonCore");
                        }
                    }
                }
                sqlite3_finalize(stmt);
            }
        }

    } catch (const std::exception& e) {
        logger.error("Failed to start filesystem watcher: " + std::string(e.what()), "DaemonCore");
        initStatus_.result = InitializationStatus::Result::WatcherFailure;
        initStatus_.message = std::string("Filesystem watcher failed: ") + e.what();
        return false;
    }
    
    logger.info("Daemon initialization complete", "DaemonCore");
    return true;
}

void DaemonCore::run() {
    auto& logger = Logger::instance();
    
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    running_ = true;
    
    logger.info("Daemon running. Press Ctrl+C to stop.", "DaemonCore");
    
    // Main loop - keep alive and handle signals
    std::unique_lock<std::mutex> lock(runMutex_);
    while (running_ && !signalReceived) {
        runCv_.wait_for(lock, std::chrono::seconds(1));
    }

    // Log which signal was received (safe now, outside signal handler)
    if (signalReceived) {
        int sigNum = receivedSignalNum;  // volatile sig_atomic_t - direct read is safe
        logger.info("Received signal " + std::to_string(sigNum) + ", initiating shutdown", "DaemonCore");
    }

    shutdown();
}

void DaemonCore::shutdown() {
    if (!running_) return;
    
    auto& logger = Logger::instance();
    logger.info("Shutting down daemon...", "DaemonCore");
    
    running_ = false;
    
    // Shutdown in reverse order of initialization
    try {
        if (mlPlugin_) {
            mlPlugin_->shutdown();
            logger.debug("ML plugin stopped", "DaemonCore");
        }
        if (filesystem_) {
            filesystem_->shutdown();
            logger.debug("Filesystem plugin stopped", "DaemonCore");
        }
        if (network_) {
            network_->shutdown();
            logger.debug("Network plugin stopped", "DaemonCore");
        }
        if (storage_) {
            storage_->shutdown();
            logger.debug("Storage plugin stopped", "DaemonCore");
        }
    } catch (const std::exception& e) {
        logger.error("Error during shutdown: " + std::string(e.what()), "DaemonCore");
    }
    
    logger.info("Daemon stopped gracefully", "DaemonCore");
    std::cout << "Daemon stopped." << std::endl;
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
