#include "ConfigCommands.h"
#include "../../DaemonCore.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include <sstream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <sqlite3.h>
#include <sys/utsname.h>

namespace SentinelFS {

std::string ConfigCommands::handleConfigJson() {
    std::stringstream ss;
    ss << "{";
    
    // Get current config from daemon
    if (ctx_.daemonCore) {
        const auto& config = ctx_.daemonCore->getConfig();
        
        ss << "\"tcpPort\":" << config.tcpPort << ",";
        ss << "\"discoveryPort\":" << config.discoveryPort << ",";
        ss << "\"metricsPort\":" << config.metricsPort << ",";
        ss << "\"watchDirectory\":\"" << config.watchDirectory << "\",";
        // Get actual session code from network plugin (runtime value)
        std::string currentSessionCode = ctx_.network ? ctx_.network->getSessionCode() : config.sessionCode;
        ss << "\"sessionCode\":\"" << currentSessionCode << "\",";
        ss << "\"encryptionEnabled\":" << (config.encryptionEnabled ? "true" : "false") << ",";
        ss << "\"uploadLimit\":" << (config.uploadLimit / 1024) << ",";  // Convert to KB/s
        ss << "\"downloadLimit\":" << (config.downloadLimit / 1024);  // Convert to KB/s
    } else {
        ss << "\"error\":\"Daemon not initialized\"";
    }
    
    // Add network status
    if (ctx_.network) {
        ss << ",\"encryption\":" << (ctx_.network->isEncryptionEnabled() ? "true" : "false");
        ss << ",\"hasSessionCode\":" << (!ctx_.network->getSessionCode().empty() ? "true" : "false");
    }
    
    // Add sync status
    if (ctx_.daemonCore) {
        ss << ",\"syncEnabled\":" << (ctx_.daemonCore->isSyncEnabled() ? "true" : "false");
    }
    
    // Add watched folders list from database
    ss << ",\"watchedFolders\":[";
    if (ctx_.storage) {
        sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
        const char* sql = "SELECT path FROM watched_folders WHERE status_id = 1";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            bool first = true;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (path) {
                    ss << "\"" << path << "\"";
                }
            }
            sqlite3_finalize(stmt);
        }
    }
    ss << "]";

    ss << "}\n";
    return ss.str();
}

std::string ConfigCommands::handleSetConfig(const std::string& args) {
    // Expected format: SET_CONFIG key=value
    size_t eqPos = args.find('=');
    if (eqPos == std::string::npos) {
        return "Error: Invalid format. Use: SET_CONFIG key=value\n";
    }
    
    std::string key = args.substr(0, eqPos);
    std::string value = args.substr(eqPos + 1);
    
    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t\r\n"));
    key.erase(key.find_last_not_of(" \t\r\n") + 1);
    value.erase(0, value.find_first_not_of(" \t\r\n"));
    value.erase(value.find_last_not_of(" \t\r\n") + 1);
    
    try {
        if (key == "uploadLimit") {
            if (!ctx_.network) {
                return "Error: Network subsystem not initialized\n";
            }
            size_t kb = std::stoull(value);
            if (kb > 10 * 1024 * 1024) {
                return "Error: Upload limit too high (max 10 GB/s)\n";
            }
            ctx_.network->setGlobalUploadLimit(kb * 1024);
            return "Success: Upload limit set to " + value + " KB/s\n";
        } else if (key == "downloadLimit") {
            if (!ctx_.network) {
                return "Error: Network subsystem not initialized\n";
            }
            size_t kb = std::stoull(value);
            if (kb > 10 * 1024 * 1024) {
                return "Error: Download limit too high (max 10 GB/s)\n";
            }
            ctx_.network->setGlobalDownloadLimit(kb * 1024);
            return "Success: Download limit set to " + value + " KB/s\n";
        } else if (key == "sessionCode") {
            if (!ctx_.network) {
                return "Error: Network subsystem not initialized\n";
            }
            if (value.length() != 6) {
                return "Error: Session code must be exactly 6 characters\n";
            }
            for (char c : value) {
                if (!std::isalnum(c)) {
                    return "Error: Session code must contain only alphanumeric characters\n";
                }
            }
            ctx_.network->setSessionCode(value);
            return "Success: Session code updated\n";
        } else if (key == "encryption") {
            if (!ctx_.network) {
                return "Error: Network subsystem not initialized\n";
            }
            bool enable = (value == "true" || value == "1" || value == "enabled");
            ctx_.network->setEncryptionEnabled(enable);
            return "Success: Encryption " + std::string(enable ? "enabled" : "disabled") + "\n";
        } else if (key == "syncEnabled") {
            if (!ctx_.daemonCore) {
                return "Error: Daemon core not initialized\n";
            }
            bool enable = (value == "true" || value == "1" || value == "enabled");
            if (enable) {
                ctx_.daemonCore->resumeSync();
            } else {
                ctx_.daemonCore->pauseSync();
            }
            return "Success: Sync " + std::string(enable ? "enabled" : "disabled") + "\n";
        } else {
            return "Error: Unknown config key: " + key + "\n";
        }
    } catch (const std::exception& e) {
        return "Error: Invalid value for " + key + ": " + e.what() + "\n";
    }
}

std::string ConfigCommands::handleExportConfig() {
    std::stringstream ss;
    ss << "{";
    
    if (ctx_.daemonCore) {
        const auto& config = ctx_.daemonCore->getConfig();
        std::string sessionCode = ctx_.network ? ctx_.network->getSessionCode() : config.sessionCode;
        bool encryption = ctx_.network ? ctx_.network->isEncryptionEnabled() : config.encryptionEnabled;
        
        ss << "\"tcpPort\":" << config.tcpPort << ",";
        ss << "\"discoveryPort\":" << config.discoveryPort << ",";
        ss << "\"metricsPort\":" << config.metricsPort << ",";
        ss << "\"watchDirectory\":\"" << config.watchDirectory << "\",";
        ss << "\"sessionCode\":\"" << sessionCode << "\",";
        ss << "\"encryptionEnabled\":" << (encryption ? "true" : "false") << ",";
        ss << "\"uploadLimit\":" << config.uploadLimit << ",";
        ss << "\"downloadLimit\":" << config.downloadLimit << ",";
        ss << "\"syncEnabled\":" << (ctx_.daemonCore->isSyncEnabled() ? "true" : "false");
    }
    
    ss << "}\n";
    return ss.str();
}

std::string ConfigCommands::handleImportConfig(const std::string& args) {
    // Parse JSON config and apply settings
    try {
        if (args.find("sessionCode") != std::string::npos) {
            size_t start = args.find("sessionCode\":\"") + 14;
            size_t end = args.find("\"", start);
            if (start != std::string::npos && end != std::string::npos && ctx_.network) {
                std::string code = args.substr(start, end - start);
                if (!code.empty()) ctx_.network->setSessionCode(code);
            }
        }
        
        if (args.find("\"encryptionEnabled\":true") != std::string::npos && ctx_.network) {
            ctx_.network->setEncryptionEnabled(true);
        } else if (args.find("\"encryptionEnabled\":false") != std::string::npos && ctx_.network) {
            ctx_.network->setEncryptionEnabled(false);
        }
        
        if (args.find("uploadLimit\":") != std::string::npos && ctx_.network) {
            size_t start = args.find("uploadLimit\":") + 13;
            size_t end = args.find_first_of(",}", start);
            if (start != std::string::npos && end != std::string::npos) {
                size_t limit = std::stoull(args.substr(start, end - start));
                ctx_.network->setGlobalUploadLimit(limit);
            }
        }
        
        if (args.find("downloadLimit\":") != std::string::npos && ctx_.network) {
            size_t start = args.find("downloadLimit\":") + 15;
            size_t end = args.find_first_of(",}", start);
            if (start != std::string::npos && end != std::string::npos) {
                size_t limit = std::stoull(args.substr(start, end - start));
                ctx_.network->setGlobalDownloadLimit(limit);
            }
        }
        
        return "Success: Configuration imported\n";
    } catch (const std::exception& e) {
        return "Error: Failed to import config: " + std::string(e.what()) + "\n";
    }
}

std::string ConfigCommands::handleAddIgnore(const std::string& args) {
    if (args.empty()) {
        return "Error: No pattern provided\n";
    }
    
    if (!ctx_.storage) {
        return "Error: Storage not initialized\n";
    }
    
    // Normalize pattern
    std::string pattern = args;
    if (ctx_.daemonCore) {
        std::string watchDir = ctx_.daemonCore->getConfig().watchDirectory;
        if (watchDir.back() != '/') watchDir += '/';
        
        if (pattern.find(watchDir) == 0) {
            pattern = pattern.substr(watchDir.length());
        } else if (pattern.find("file://" + watchDir) == 0) {
            pattern = pattern.substr(7 + watchDir.length());
        }
        
        if (!pattern.empty() && pattern[0] == '/') {
            pattern = pattern.substr(1);
        }
    }
    
    // Use API for proper statistics tracking
    if (ctx_.storage->addIgnorePattern(pattern)) {
        return "Success: Added ignore pattern: " + pattern + "\n";
    }
    return "Error: Failed to add ignore pattern\n";
}

std::string ConfigCommands::handleRemoveIgnore(const std::string& args) {
    if (args.empty()) {
        return "Error: No pattern provided\n";
    }
    
    if (!ctx_.storage) {
        return "Error: Storage not initialized\n";
    }
    
    // Use API for proper statistics tracking
    if (ctx_.storage->removeIgnorePattern(args)) {
        return "Success: Removed ignore pattern: " + args + "\n";
    }
    return "Error: Failed to remove ignore pattern\n";
}

std::string ConfigCommands::handleListIgnore() {
    std::stringstream ss;
    ss << "{\"patterns\":[";
    
    if (ctx_.storage) {
        sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
        
        // Use normalized schema - ignore_patterns table is created by SQLiteHandler
        const char* sql = "SELECT pattern FROM ignore_patterns ORDER BY pattern";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            bool first = true;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                
                const char* pattern = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                
                ss << "{\"pattern\":\"" << (pattern ? pattern : "") << "\",";
                ss << "\"type\":\"glob\"}";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string ConfigCommands::handleExportSupportBundle() {
    namespace fs = std::filesystem;
    
    // Determine output directory
    std::string dataHome;
    if (const char* xdg = std::getenv("XDG_DATA_HOME")) {
        dataHome = xdg;
    } else if (const char* home = std::getenv("HOME")) {
        dataHome = std::string(home) + "/.local/share";
    } else {
        return "Error: Cannot determine data directory\n";
    }
    
    std::string supportDir = dataHome + "/sentinelfs/support";
    
    // Create timestamp-based folder
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm);
    
    std::string bundleDir = supportDir + "/bundle_" + timestamp;
    
    try {
        fs::create_directories(bundleDir);
    } catch (const fs::filesystem_error& e) {
        return "Error: Failed to create support bundle directory: " + std::string(e.what()) + "\n";
    }
    
    std::stringstream result;
    result << "Creating support bundle at: " << bundleDir << "\n";
    
    // 1. Copy config file
    std::string configHome;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
        configHome = xdg;
    } else if (const char* home = std::getenv("HOME")) {
        configHome = std::string(home) + "/.config";
    }
    
    std::string configPath = configHome + "/sentinelfs/sentinel.conf";
    std::string destConfig = bundleDir + "/sentinel.conf";
    
    try {
        if (fs::exists(configPath)) {
            fs::copy_file(configPath, destConfig, fs::copy_options::overwrite_existing);
            result << "  ✓ Config file copied\n";
        } else {
            result << "  ⚠ Config file not found at " << configPath << "\n";
        }
    } catch (const fs::filesystem_error& e) {
        result << "  ✗ Failed to copy config: " << e.what() << "\n";
    }
    
    // 2. Copy recent log files
    std::string logsDir = dataHome + "/sentinelfs/logs";
    std::string destLogsDir = bundleDir + "/logs";
    
    try {
        fs::create_directories(destLogsDir);
        
        int logsCopied = 0;
        if (fs::exists(logsDir)) {
            for (const auto& entry : fs::directory_iterator(logsDir)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (filename.find("sentinel") != std::string::npos || 
                        filename.find(".log") != std::string::npos) {
                        fs::copy_file(entry.path(), destLogsDir + "/" + filename, 
                                     fs::copy_options::overwrite_existing);
                        logsCopied++;
                        if (logsCopied >= 5) break;
                    }
                }
            }
        }
        
        result << "  ✓ " << logsCopied << " log file(s) copied\n";
    } catch (const fs::filesystem_error& e) {
        result << "  ⚠ Log copy warning: " << e.what() << "\n";
    }
    
    // 3. Generate system info file
    std::string infoPath = bundleDir + "/system_info.txt";
    std::ofstream infoFile(infoPath);
    
    if (infoFile.is_open()) {
        infoFile << "=== SentinelFS Support Bundle ===\n";
        infoFile << "Generated: " << timestamp << "\n\n";
        
        infoFile << "--- Version ---\n";
        infoFile << "SentinelFS Version: 1.0.0\n";
        infoFile << "Build Type: Release\n\n";
        
        infoFile << "--- System ---\n";
        struct utsname sysinfo;
        if (uname(&sysinfo) == 0) {
            infoFile << "OS: " << sysinfo.sysname << " " << sysinfo.release << "\n";
            infoFile << "Kernel: " << sysinfo.version << "\n";
            infoFile << "Architecture: " << sysinfo.machine << "\n";
            infoFile << "Hostname: " << sysinfo.nodename << "\n";
        }
        infoFile << "\n";
        
        infoFile << "--- Daemon Status ---\n";
        if (ctx_.daemonCore) {
            infoFile << "Sync Enabled: " << (ctx_.daemonCore->isSyncEnabled() ? "Yes" : "No") << "\n";
            infoFile << "Watch Directory: " << ctx_.daemonCore->getConfig().watchDirectory << "\n";
            infoFile << "TCP Port: " << ctx_.daemonCore->getConfig().tcpPort << "\n";
            infoFile << "Discovery Port: " << ctx_.daemonCore->getConfig().discoveryPort << "\n";
        }
        infoFile << "\n";
        
        infoFile << "--- Network ---\n";
        if (ctx_.network) {
            infoFile << "Encryption: " << (ctx_.network->isEncryptionEnabled() ? "Enabled" : "Disabled") << "\n";
            std::string code = ctx_.network->getSessionCode();
            infoFile << "Session Code Set: " << (!code.empty() ? "Yes" : "No") << "\n";
        }
        infoFile << "\n";
        
        infoFile << "--- Peers ---\n";
        if (ctx_.storage) {
            auto peers = ctx_.storage->getAllPeers();
            infoFile << "Connected Peers: " << peers.size() << "\n";
            for (const auto& peer : peers) {
                infoFile << "  - " << peer.id << " @ " << peer.ip << ":" << peer.port 
                         << " (" << peer.status << ")\n";
            }
        }
        infoFile << "\n";
        
        infoFile << "--- Storage ---\n";
        if (ctx_.storage) {
            auto [totalConflicts, unresolvedConflicts] = ctx_.storage->getConflictStats();
            infoFile << "Total Conflicts: " << totalConflicts << "\n";
            infoFile << "Unresolved Conflicts: " << unresolvedConflicts << "\n";
        }
        
        infoFile.close();
        result << "  ✓ System info generated\n";
    } else {
        result << "  ✗ Failed to create system info file\n";
    }
    
    // 4. Copy database (if under 50MB)
    std::string dbPath = dataHome + "/sentinelfs/sentinel.db";
    std::string destDb = bundleDir + "/sentinel.db";
    
    try {
        if (fs::exists(dbPath)) {
            auto dbSize = fs::file_size(dbPath);
            if (dbSize < 50 * 1024 * 1024) {
                fs::copy_file(dbPath, destDb, fs::copy_options::overwrite_existing);
                result << "  ✓ Database copied (" << formatBytes(dbSize) << ")\n";
            } else {
                result << "  ⚠ Database too large (" << formatBytes(dbSize) << "), skipped\n";
            }
        }
    } catch (const fs::filesystem_error& e) {
        result << "  ⚠ Database copy skipped: " << e.what() << "\n";
    }
    
    result << "\nSupport bundle created successfully!\n";
    result << "BUNDLE_PATH:" << bundleDir << "\n";
    
    return result.str();
}

} // namespace SentinelFS
