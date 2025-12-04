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
    // Signal-safe: use volatile sig_atomic_t for guaranteed async-signal-safety
    // std::atomic is technically not guaranteed async-signal-safe in C++17
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

bool DaemonCore::loadPlugins() {
    auto& logger = Logger::instance();
    
    storage_.reset();
    network_.reset();
    filesystem_.reset();
    mlPlugin_.reset();

    // Get plugin directory from environment or use default
    const char* envPluginDir = std::getenv("SENTINELFS_PLUGIN_DIR");
    std::filesystem::path desiredDir = envPluginDir ? std::filesystem::path(envPluginDir) : std::filesystem::path("./plugins");
    std::filesystem::path defaultFallback = std::filesystem::path("./build/plugins");

    if (!std::filesystem::exists(desiredDir) && std::filesystem::exists(defaultFallback)) {
        logger.warn("Plugin directory " + desiredDir.string() + " not found; falling back to " + defaultFallback.string(), "DaemonCore");
        desiredDir = defaultFallback;
    }

    if (!std::filesystem::exists(desiredDir)) {
        logger.error("Plugin directory does not exist: " + desiredDir.string(), "DaemonCore");
        initStatus_.result = InitializationStatus::Result::PlugInLoadFailure;
        initStatus_.message = "Plugin directory not found: " + desiredDir.string() + ". Set SENTINELFS_PLUGIN_DIR or build plugins.";
        return false;
    }

    std::string pluginDir = desiredDir.string();
    logger.info("Loading plugins from: " + pluginDir, "DaemonCore");

    // Optional plugin manifest
    Config manifestConfig;
    bool manifestLoaded = false;
    std::string manifestPath;
    std::vector<std::filesystem::path> manifestCandidates;
    
    if (const char* envManifest = std::getenv("SENTINELFS_PLUGIN_MANIFEST")) {
        manifestCandidates.emplace_back(envManifest);
    }
    manifestCandidates.emplace_back(desiredDir / "plugins.conf");
    if (desiredDir != std::filesystem::path("./plugins")) {
        manifestCandidates.emplace_back(std::filesystem::path("./plugins/plugins.conf"));
    }

    for (const auto& candidate : manifestCandidates) {
        if (std::filesystem::exists(candidate) && manifestConfig.loadFromFile(candidate.string())) {
            manifestLoaded = true;
            manifestPath = candidate.string();
            break;
        }
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
        std::vector<std::string> missing;
        if (!storage_) {
            logger.error("Storage plugin failed to load", "DaemonCore");
            missing.push_back("storage/libstorage_plugin.so");
        } else {
            logger.debug("Storage plugin loaded", "DaemonCore");
        }
        if (!network_) {
            logger.error("Network plugin failed to load", "DaemonCore");
            missing.push_back("network/libnetwork_plugin.so");
        } else {
            logger.debug("Network plugin loaded", "DaemonCore");
        }
        if (!filesystem_) {
            logger.error("Filesystem plugin failed to load", "DaemonCore");
            missing.push_back("filesystem/libfilesystem_plugin.so");
        } else {
            logger.debug("Filesystem plugin loaded", "DaemonCore");
        }
        if (!mlPlugin_) {
            logger.warn("ML plugin failed to load (optional)", "DaemonCore");
        } else {
            logger.debug("ML plugin loaded", "DaemonCore");
        }
        initStatus_.result = InitializationStatus::Result::PlugInLoadFailure;
        initStatus_.message = "Missing critical plugins: " + [&]() {
            std::string list;
            for (size_t i = 0; i < missing.size(); ++i) {
                list += missing[i];
                if (i + 1 < missing.size()) {
                    list += ", ";
                }
            }
            return list;
        }();
        return false;
    }
    
    initStatus_.result = InitializationStatus::Result::Success;
    initStatus_.message.clear();
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
        
        // Start watching the directory with filesystem plugin
        filesystem_->startWatching(absPath);
        logger.info("Directory watch added successfully: " + absPath, "DaemonCore");
        
        // Trigger immediate scan of the new directory
        eventBus_.publish("WATCH_ADDED", absPath);
        
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
