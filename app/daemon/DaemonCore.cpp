#include "DaemonCore.h"
#include "Logger.h"
#include "Config.h"
#include "../../plugins/storage/include/SQLiteHandler.h"
#include <iostream>
#include <csignal>
#include <filesystem>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <sqlite3.h>

namespace SentinelFS {

namespace {
    // Signal-safe: only atomic operations, no I/O
    std::atomic<bool> signalReceived{false};
    std::atomic<int> receivedSignalNum{0};
    
    void signalHandler(int signal) {
        receivedSignalNum = signal;
        signalReceived = true;
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
    
    // Start network services
    try {
        network_->startListening(config_.tcpPort);
        logger.info("TCP listener started on port " + std::to_string(config_.tcpPort), "DaemonCore");
        
        network_->startDiscovery(config_.discoveryPort);
        logger.info("UDP discovery started on port " + std::to_string(config_.discoveryPort), "DaemonCore");
    } catch (const std::exception& e) {
        logger.error("Failed to start network services: " + std::string(e.what()), "DaemonCore");
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
    } catch (const std::exception& e) {
        logger.error("Failed to start filesystem watcher: " + std::string(e.what()), "DaemonCore");
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
    std::cout << "\nDaemon running. Press Ctrl+C to stop." << std::endl;
    
    // Main loop - keep alive and handle signals
    while (running_ && !signalReceived) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Log which signal was received (safe now, outside signal handler)
    if (signalReceived) {
        int sigNum = receivedSignalNum.load();
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

bool DaemonCore::loadPlugins() {
    auto& logger = Logger::instance();
    
    storage_.reset();
    network_.reset();
    filesystem_.reset();
    mlPlugin_.reset();

    // Get plugin directory from environment or use default
    const char* envPluginDir = std::getenv("SENTINELFS_PLUGIN_DIR");
    std::string pluginDir = envPluginDir ? envPluginDir : "./plugins";
    
    logger.info("Loading plugins from: " + pluginDir, "DaemonCore");
    
    // Check if plugin directory exists
    if (!std::filesystem::exists(pluginDir)) {
        logger.error("Plugin directory does not exist: " + pluginDir, "DaemonCore");
        std::cerr << "Plugin directory not found: " << pluginDir << std::endl;
        std::cerr << "Tip: Set SENTINELFS_PLUGIN_DIR environment variable or run from build directory" << std::endl;
        return false;
    }

    // Optional plugin manifest
    Config manifestConfig;
    bool manifestLoaded = false;
    std::string manifestPath;

    if (const char* envManifest = std::getenv("SENTINELFS_PLUGIN_MANIFEST")) {
        manifestPath = envManifest;
        manifestLoaded = manifestConfig.loadFromFile(manifestPath);
    } else {
        manifestPath = pluginDir + "/plugins.conf";
        manifestLoaded = manifestConfig.loadFromFile(manifestPath);
    }

    if (manifestLoaded) {
        logger.info("Loaded plugin manifest from: " + manifestPath, "DaemonCore");
    } else {
        logger.info("No plugin manifest found at: " + manifestPath + " (using built-in defaults)", "DaemonCore");
    }

    auto parseDependencies = [](const std::string& depsStr) {
        std::vector<std::string> deps;
        size_t start = 0;
        while (start < depsStr.size()) {
            size_t end = depsStr.find(',', start);
            if (end == std::string::npos) {
                end = depsStr.size();
            }
            std::string token = depsStr.substr(start, end - start);
            auto first = token.find_first_not_of(" \t\r\n");
            auto last = token.find_last_not_of(" \t\r\n");
            if (first != std::string::npos && last != std::string::npos) {
                token = token.substr(first, last - first + 1);
                if (!token.empty()) {
                    deps.push_back(token);
                }
            }
            start = end + 1;
        }
        return deps;
    };
    
    pluginManager_.unloadAll();
    auto registerPlugin = [&](const std::string& key,
                              const std::string& relativePath,
                              std::vector<std::string> deps = {},
                              const std::string& minVersion = "1.0.0",
                              bool optional = false) {
        PluginManager::Descriptor desc;

        std::string baseDir = pluginDir;
        if (manifestLoaded) {
            std::string configuredDir = manifestConfig.get("plugins.dir", pluginDir);
            if (!configuredDir.empty()) {
                baseDir = configuredDir;
            }
        }

        std::string pathKey = "plugin." + key + ".path";
        std::string depsKey = "plugin." + key + ".deps";
        std::string verKey  = "plugin." + key + ".min_version";

        std::string relPath = manifestLoaded
            ? manifestConfig.get(pathKey, relativePath)
            : relativePath;

        std::string depsStr = manifestLoaded
            ? manifestConfig.get(depsKey, "")
            : "";

        std::string minVer = manifestLoaded
            ? manifestConfig.get(verKey, minVersion)
            : minVersion;

        desc.path = baseDir + "/" + relPath;
        desc.optional = optional;

        if (!depsStr.empty()) {
            desc.dependencies = parseDependencies(depsStr);
        } else {
            desc.dependencies = std::move(deps);
        }

        desc.minVersion = minVer;
        pluginManager_.registerPlugin(key, std::move(desc));
    };

    registerPlugin("storage", "storage/libstorage_plugin.so");
    registerPlugin("network", "network/libnetwork_plugin.so", {"storage"});
    registerPlugin("filesystem", "filesystem/libfilesystem_plugin.so");
    registerPlugin("ml", "ml/libml_plugin.so", {"storage"}, "1.0.0", true);

    auto storagePlugin = pluginManager_.load("storage", &eventBus_);
    auto networkPlugin = pluginManager_.load("network", &eventBus_);
    auto filesystemPlugin = pluginManager_.load("filesystem", &eventBus_);
    mlPlugin_ = pluginManager_.load("ml", &eventBus_);
    
    // Cast to specific interfaces using dynamic_pointer_cast
    storage_ = std::dynamic_pointer_cast<IStorageAPI>(storagePlugin);
    network_ = std::dynamic_pointer_cast<INetworkAPI>(networkPlugin);
    filesystem_ = std::dynamic_pointer_cast<IFileAPI>(filesystemPlugin);
    
    // Verify critical plugins loaded
    if (!storage_ || !network_ || !filesystem_) {
        logger.critical("Failed to load one or more critical plugins", "DaemonCore");
        
        std::cerr << "\nPlugin Status:" << std::endl;
        if (!storage_) {
            logger.error("Storage plugin failed to load", "DaemonCore");
            std::cerr << "  ✗ Storage: FAILED at \"" << pluginDir << "/storage/libstorage_plugin.so\"" << std::endl;
        } else {
            logger.debug("Storage plugin loaded", "DaemonCore");
            std::cerr << "  ✓ Storage: OK" << std::endl;
        }
        
        if (!network_) {
            logger.error("Network plugin failed to load", "DaemonCore");
            std::cerr << "  ✗ Network: FAILED at \"" << pluginDir << "/network/libnetwork_plugin.so\"" << std::endl;
        } else {
            logger.debug("Network plugin loaded", "DaemonCore");
            std::cerr << "  ✓ Network: OK" << std::endl;
        }
        
        if (!filesystem_) {
            logger.error("Filesystem plugin failed to load", "DaemonCore");
            std::cerr << "  ✗ Filesystem: FAILED at \"" << pluginDir << "/filesystem/libfilesystem_plugin.so\"" << std::endl;
        } else {
            logger.debug("Filesystem plugin loaded", "DaemonCore");
            std::cerr << "  ✓ Filesystem: OK" << std::endl;
        }
        
        if (!mlPlugin_) {
            logger.warn("ML plugin failed to load (optional)", "DaemonCore");
            std::cerr << "  ⚠ ML: OPTIONAL - not loaded" << std::endl;
        } else {
            logger.debug("ML plugin loaded", "DaemonCore");
            std::cerr << "  ✓ ML: OK" << std::endl;
        }
        
        return false;
    }
    
    logger.info("All critical plugins loaded successfully", "DaemonCore");
    if (mlPlugin_) {
        logger.info("ML plugin (anomaly detection) loaded", "DaemonCore");
    } else {
        logger.warn("ML plugin not loaded - anomaly detection disabled", "DaemonCore");
    }
    
    return true;
}

void DaemonCore::setupEventHandlers() {
    // Event handlers are now in EventHandlers class
    // This method is kept for internal initialization if needed
}

bool DaemonCore::addWatchDirectory(const std::string& path) {
    auto& logger = Logger::instance();
    
    // Validate path
    if (!std::filesystem::exists(path)) {
        logger.error("Directory does not exist: " + path, "DaemonCore");
        return false;
    }
    
    if (!std::filesystem::is_directory(path)) {
        logger.error("Path is not a directory: " + path, "DaemonCore");
        return false;
    }
    
    // Add watch via filesystem plugin
    if (!filesystem_) {
        logger.error("Filesystem plugin not initialized", "DaemonCore");
        return false;
    }
    
    try {
        // Get absolute path
        auto absPath = std::filesystem::absolute(path).string();
        
        // Add watch (filesystem plugin should support multiple watches)
        logger.info("Adding watch for directory: " + absPath, "DaemonCore");
        
        // Save to database (or reactivate if exists)
        if (storage_) {
            sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
            
            // First check if folder already exists
            const char* checkSql = "SELECT status FROM watched_folders WHERE path = ?";
            sqlite3_stmt* checkStmt;
            bool exists = false;
            
            if (sqlite3_prepare_v2(db, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(checkStmt, 1, absPath.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(checkStmt) == SQLITE_ROW) {
                    exists = true;
                }
                sqlite3_finalize(checkStmt);
            }
            
            if (exists) {
                // Update existing folder to active
                const char* updateSql = "UPDATE watched_folders SET status = 'active', added_at = ? WHERE path = ?";
                sqlite3_stmt* updateStmt;
                
                if (sqlite3_prepare_v2(db, updateSql, -1, &updateStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_int64(updateStmt, 1, std::time(nullptr));
                    sqlite3_bind_text(updateStmt, 2, absPath.c_str(), -1, SQLITE_TRANSIENT);
                    
                    if (sqlite3_step(updateStmt) == SQLITE_DONE) {
                        logger.info("Reactivated watched folder: " + absPath, "DaemonCore");
                    }
                    sqlite3_finalize(updateStmt);
                }
            } else {
                // Insert new folder
                const char* insertSql = "INSERT INTO watched_folders (path, added_at, status) VALUES (?, ?, 'active')";
                sqlite3_stmt* insertStmt;
                
                if (sqlite3_prepare_v2(db, insertSql, -1, &insertStmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(insertStmt, 1, absPath.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int64(insertStmt, 2, std::time(nullptr));
                    
                    if (sqlite3_step(insertStmt) == SQLITE_DONE) {
                        logger.info("Watched folder saved to database: " + absPath, "DaemonCore");
                    } else {
                        logger.error("Failed to save watched folder to database", "DaemonCore");
                    }
                    sqlite3_finalize(insertStmt);
                }
            }
        }
        
        // TODO: Filesystem plugin needs addWatch() method for actual file monitoring
        logger.info("Directory watch added successfully: " + absPath, "DaemonCore");
        return true;
        
    } catch (const std::exception& e) {
        logger.error("Failed to add watch: " + std::string(e.what()), "DaemonCore");
        return false;
    }
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
