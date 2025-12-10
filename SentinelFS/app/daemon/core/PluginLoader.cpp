/**
 * @file PluginLoader.cpp
 * @brief DaemonCore plugin loading and management
 */

#include "DaemonCore.h"
#include "Logger.h"
#include "Config.h"
#include <filesystem>
#include <cstdlib>

namespace SentinelFS {

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

    // Modern plugin stack
    registerPlugin("storage", "falconstore/libfalconstore.so");  // FalconStore - high-performance storage
    registerPlugin("network", "netfalcon/libnetfalcon.so", {"storage"});  // NetFalcon - multi-transport network
    registerPlugin("filesystem", "ironroot/libironroot.so");  // IronRoot - advanced filesystem
    registerPlugin("ml", "zer0/libzer0.so", {"storage"}, "1.0.0", true);  // Zer0 - advanced threat detection

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
        
        // Set storage reference for ML plugin (Zer0 needs DB access)
        if (storage_) {
            mlPlugin_->setStoragePlugin(storage_.get());
            logger.info("Storage reference set for ML plugin", "DaemonCore");
        }
    } else {
        logger.warn("ML plugin not loaded - anomaly detection disabled", "DaemonCore");
    }
    
    return true;
}

} // namespace SentinelFS
