/**
 * @file DaemonLifecycle.cpp
 * @brief DaemonCore constructor, destructor, lifecycle methods (initialize, run, shutdown)
 */

#include "DaemonCore.h"
#include "Logger.h"
#include "Config.h"
#include "../../core/storage/include/FileVersionManager.h"
#include <iostream>
#include <csignal>
#include <filesystem>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <sqlite3.h>
#include <tuple>
#include <vector>

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

void DaemonCore::registerThread(std::thread&& thread) {
    std::lock_guard<std::mutex> lock(threadMutex_);
    managedThreads_.push_back(std::move(thread));
}

void DaemonCore::stopAllThreads() {
    auto& logger = Logger::instance();
    
    std::lock_guard<std::mutex> lock(threadMutex_);
    logger.info("Stopping " + std::to_string(managedThreads_.size()) + " managed threads...", "DaemonCore");
    
    for (auto& thread : managedThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    managedThreads_.clear();
    logger.info("All threads stopped successfully", "DaemonCore");
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
        
        // Clean up stale peers before reconnecting
        cleanupStalePeers();
        
        // Auto-reconnect to previously known peers
        reconnectToKnownPeers();
        
    } catch (const std::exception& e) {
        logger.error("Failed to start network services: " + std::string(e.what()), "DaemonCore");
        initStatus_.result = InitializationStatus::Result::NetworkFailure;
        initStatus_.message = std::string("Network startup failed: ") + e.what();
        return false;
    }
    
    // Start filesystem monitoring
    std::string watchDir;
    try {
        // Expand tilde in watch directory path
        watchDir = expandTilde(config_.watchDirectory);
        
        if (!std::filesystem::exists(watchDir)) {
            std::filesystem::create_directories(watchDir);
            logger.info("Created watch directory: " + watchDir, "DaemonCore");
        }

        // Use addWatchDirectory to properly register in database
        addWatchDirectory(watchDir);
        logger.info("Filesystem watcher started for: " + watchDir, "DaemonCore");

        // Load and watch persisted folders from database
        if (storage_) {
            sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
            const char* sql = "SELECT path FROM watched_folders WHERE status_id = 1";
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
    
    // Initialize file version manager
    try {
        if (!watchDir.empty()) {
            versionManager_ = std::make_unique<FileVersionManager>(watchDir);
            logger.info("File version manager initialized", "DaemonCore");
        }
    } catch (const std::exception& e) {
        logger.error("Failed to initialize version manager: " + std::string(e.what()), "DaemonCore");
        // Non-critical, continue without versioning
    }
    
    // Initialize file version manager
    try {
        versionManager_ = std::make_unique<FileVersionManager>(watchDir);
        logger.info("File version manager initialized", "DaemonCore");
    } catch (const std::exception& e) {
        logger.error("Failed to initialize version manager: " + std::string(e.what()), "DaemonCore");
        // Non-critical, continue without versioning
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

void DaemonCore::reconnectToKnownPeers() {
    auto& logger = Logger::instance();
    
    if (!storage_ || !network_) {
        return;
    }
    
    sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
    if (!db) {
        return;
    }
    
    // Get local peer info to filter out ourselves
    std::string localPeerId = network_->getLocalPeerId();
    int localPort = network_->getLocalPort();
    
    // Get all previously known peers with valid addresses
    const char* sql = "SELECT p.peer_id, p.address, p.port FROM peers p "
                      "WHERE p.address != '0.0.0.0' AND p.address != '' AND p.port > 0";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logger.warn("Failed to query known peers for reconnection", "DaemonCore");
        return;
    }
    
    std::vector<std::tuple<std::string, std::string, int>> peersToConnect;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* peerId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        int port = sqlite3_column_int(stmt, 2);
        
        if (peerId && address && port > 0) {
            // Skip our own peer ID
            if (peerId == localPeerId) {
                continue;
            }
            // Skip our own port on localhost
            if (port == localPort && (std::string(address) == "127.0.0.1" || 
                                       std::string(address) == "localhost" ||
                                       std::string(address) == "192.168.1.100")) {
                continue;
            }
            peersToConnect.emplace_back(peerId, address, port);
        }
    }
    sqlite3_finalize(stmt);
    
    if (peersToConnect.empty()) {
        logger.info("No known peers to reconnect", "DaemonCore");
        return;
    }
    
    logger.info("Attempting to reconnect to " + std::to_string(peersToConnect.size()) + " known peer(s)", "DaemonCore");
    
    // Reconnect in a separate thread to not block startup
    std::thread([this, peersToConnect]() {
        auto& logger = Logger::instance();
        
        // Wait a bit for network to fully initialize
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        for (const auto& [peerId, address, port] : peersToConnect) {
            logger.info("Reconnecting to peer: " + peerId + " at " + address + ":" + std::to_string(port), "DaemonCore");
            
            if (network_->connectToPeer(address, port)) {
                logger.info("Successfully reconnected to peer: " + peerId, "DaemonCore");
            } else {
                logger.warn("Failed to reconnect to peer: " + peerId + " - will retry on discovery", "DaemonCore");
                
                // Mark peer as offline in database using API
                if (storage_) {
                    storage_->updatePeerStatus(peerId, "offline");
                }
            }
            
            // Small delay between connection attempts
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }).detach();
}

void DaemonCore::cleanupStalePeers() {
    auto& logger = Logger::instance();
    
    if (!storage_) {
        return;
    }
    
    sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
    if (!db) {
        return;
    }
    
    // Remove peers that have invalid addresses or are stale
    // status_id 6 = offline
    const char* sql = R"(
        DELETE FROM peers 
        WHERE address IN ('Unknown', '0.0.0.0', '') 
           OR address IS NULL
           OR port <= 0
           OR (last_seen < strftime('%s', 'now') - 86400 AND status_id = 6)
    )";
    
    char* errMsg = nullptr;
    
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) == SQLITE_OK) {
        int deleted = sqlite3_changes(db);
        if (deleted > 0) {
            logger.info("Cleaned up " + std::to_string(deleted) + " stale peer(s) from database", "DaemonCore");
        }
    } else {
        if (errMsg) {
            logger.warn("Failed to cleanup stale peers: " + std::string(errMsg), "DaemonCore");
            sqlite3_free(errMsg);
        }
    }
    
    // Also remove duplicate peers (keep only the one with latest last_seen)
    const char* dedupSql = R"(
        DELETE FROM peers 
        WHERE id NOT IN (
            SELECT MIN(id) FROM peers GROUP BY address, port
        )
    )";
    
    if (sqlite3_exec(db, dedupSql, nullptr, nullptr, &errMsg) == SQLITE_OK) {
        int deleted = sqlite3_changes(db);
        if (deleted > 0) {
            logger.info("Removed " + std::to_string(deleted) + " duplicate peer(s) from database", "DaemonCore");
        }
    } else {
        if (errMsg) {
            logger.warn("Failed to remove duplicate peers: " + std::string(errMsg), "DaemonCore");
            sqlite3_free(errMsg);
        }
    }
}

void DaemonCore::printConfiguration() const {
    auto& logger = Logger::instance();
    std::string configStr = "Configuration:\n";
    configStr += "  TCP Port: " + std::to_string(config_.tcpPort) + "\n";
    configStr += "  Discovery Port: " + std::to_string(config_.discoveryPort) + "\n";
    configStr += "  Watch Directory: " + config_.watchDirectory + "\n";
    configStr += "  Encryption: " + std::string(config_.encryptionEnabled ? "Enabled" : "Disabled") + "\n";
    
    std::string ul = (config_.uploadLimit > 0 ? std::to_string(config_.uploadLimit / 1024) + " KB/s" : "Unlimited");
    configStr += "  Upload Limit: " + ul + "\n";
    
    std::string dl = (config_.downloadLimit > 0 ? std::to_string(config_.downloadLimit / 1024) + " KB/s" : "Unlimited");
    configStr += "  Download Limit: " + dl;
    
    logger.info(configStr, "DaemonCore");
    
    if (!config_.sessionCode.empty()) {
        if (config_.encryptionEnabled) {
            logger.info("Session Code: Set (Encryption Enabled)", "DaemonCore");
        } else {
            logger.info("Session Code: Set (Encryption Disabled)", "DaemonCore");
        }
    } else {
        logger.warn("Session Code: Not set (any peer can connect). Use --generate-code for security.", "DaemonCore");
        if (config_.encryptionEnabled) {
            logger.error("Error: Cannot enable encryption without a session code!", "DaemonCore");
        }
    }
}

FileVersionManager* DaemonCore::getVersionManager() {
    return versionManager_.get();
}

const FileVersionManager* DaemonCore::getVersionManager() const {
    return versionManager_.get();
}

} // namespace SentinelFS
