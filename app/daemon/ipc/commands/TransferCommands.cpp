#include "TransferCommands.h"
#include "../../DaemonCore.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "MetricsCollector.h"
#include <sstream>
#include <random>
#include <filesystem>
#include <sqlite3.h>

namespace SentinelFS {

std::string TransferCommands::handleMetrics() {
    auto& metrics = MetricsCollector::instance();
    std::string summary = metrics.getMetricsSummary();
    summary += "\n--- Bandwidth Limiter ---\n";
    summary += ctx_.network->getBandwidthStats();
    summary += "\n";
    return summary;
}

std::string TransferCommands::handleUploadLimit(const std::string& args) {
    std::string trimmed = args;
    auto first = trimmed.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "Usage: UPLOAD-LIMIT <KB/s>\n";
    }
    trimmed = trimmed.substr(first);

    try {
        long long kb = std::stoll(trimmed);
        if (kb < 0) {
            return "Upload limit must be >= 0 KB/s.\n";
        }

        std::size_t bytesPerSecond = static_cast<std::size_t>(kb) * 1024;
        ctx_.network->setGlobalUploadLimit(bytesPerSecond);

        if (bytesPerSecond == 0) {
            return "Global upload limit set to unlimited.\n";
        }

        std::ostringstream ss;
        ss << "Global upload limit set to " << kb << " KB/s.\n";
        return ss.str();
    } catch (const std::invalid_argument&) {
        return "Invalid upload limit. Usage: UPLOAD-LIMIT <KB/s>\n";
    } catch (const std::out_of_range&) {
        return "Upload limit value out of range.\n";
    }
}

std::string TransferCommands::handleDownloadLimit(const std::string& args) {
    std::string trimmed = args;
    auto first = trimmed.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "Usage: DOWNLOAD-LIMIT <KB/s>\n";
    }
    trimmed = trimmed.substr(first);

    try {
        long long kb = std::stoll(trimmed);
        if (kb < 0) {
            return "Download limit must be >= 0 KB/s.\n";
        }

        std::size_t bytesPerSecond = static_cast<std::size_t>(kb) * 1024;
        ctx_.network->setGlobalDownloadLimit(bytesPerSecond);

        if (bytesPerSecond == 0) {
            return "Global download limit set to unlimited.\n";
        }

        std::ostringstream ss;
        ss << "Global download limit set to " << kb << " KB/s.\n";
        return ss.str();
    } catch (const std::invalid_argument&) {
        return "Invalid download limit. Usage: DOWNLOAD-LIMIT <KB/s>\n";
    } catch (const std::out_of_range&) {
        return "Download limit value out of range.\n";
    }
}

std::string TransferCommands::handlePause() {
    if (ctx_.syncEnabledCallback) {
        ctx_.syncEnabledCallback(false);
        return "Synchronization PAUSED.\n";
    }
    return "Pause callback not set.\n";
}

std::string TransferCommands::handleResume() {
    if (ctx_.syncEnabledCallback) {
        ctx_.syncEnabledCallback(true);
        return "Synchronization RESUMED.\n";
    }
    return "Resume callback not set.\n";
}

std::string TransferCommands::handleDiscover() {
    if (!ctx_.network || !ctx_.daemonCore) {
        return "Error: Network subsystem not ready.\n";
    }

    const auto& cfg = ctx_.daemonCore->getConfig();
    ctx_.network->broadcastPresence(cfg.discoveryPort, cfg.tcpPort);
    return "Discovery broadcast sent.\n";
}

std::string TransferCommands::handleSetDiscovery(const std::string& args) {
    std::string setting = args;
    
    size_t eq = setting.find('=');
    if (eq == std::string::npos) {
        return "Error: Invalid format. Use SET_DISCOVERY key=value\n";
    }
    
    std::string key = setting.substr(0, eq);
    std::string value = setting.substr(eq + 1);
    bool enabled = (value == "1" || value == "true");
    
    if (key == "udp") {
        return "OK: UDP discovery " + std::string(enabled ? "enabled" : "disabled") + " (note: UDP is always active)\n";
    } else if (key == "tcp") {
        if (ctx_.network) {
            ctx_.network->setRelayEnabled(enabled);
            bool connected = ctx_.network->isRelayConnected();
            std::string status = enabled 
                ? (connected ? "enabled and connected" : "enabled (connecting...)")
                : "disabled";
            return "OK: TCP relay " + status + "\n";
        }
        return "Error: Network subsystem not ready\n";
    }
    
    return "Error: Unknown discovery setting: " + key + "\n";
}

std::string TransferCommands::handleTransfersJson() {
    std::stringstream ss;
    ss << "{\"transfers\": [";
    bool first = true;
    
    // Get active transfers from MetricsCollector
    auto& metrics = MetricsCollector::instance();
    auto activeTransfers = metrics.getActiveTransfers();
    
    for (const auto& transfer : activeTransfers) {
        if (!first) ss << ",";
        first = false;
        
        std::filesystem::path p(transfer.filePath);
        std::string filename = p.filename().string();
        if (filename.empty()) filename = transfer.filePath;
        
        ss << "{";
        ss << "\"file\":\"" << filename << "\",";
        ss << "\"peer\":\"" << transfer.peerId << "\",";
        ss << "\"type\":\"" << (transfer.isUpload ? "upload" : "download") << "\",";
        ss << "\"status\":\"active\",";
        ss << "\"progress\":" << transfer.progress << ",";
        ss << "\"size\":\"" << formatBytes(transfer.totalBytes) << "\",";
        ss << "\"speed\":\"" << formatBytes(transfer.speedBps) << "/s\"";
        ss << "}";
    }
    
    // Get pending transfers from sync_queue
    if (ctx_.storage) {
        sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
        const char* sql = "SELECT file_path, op_type, status FROM sync_queue WHERE status = 'pending' ORDER BY created_at DESC LIMIT 20";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (!first) ss << ",";
                first = false;
                
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                const char* op = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                const char* status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                
                std::string pathStr = path ? path : "";
                std::filesystem::path p(pathStr);
                std::string filename = p.filename().string();
                if (filename.empty()) filename = pathStr;
                
                std::string type = "upload";
                if (op) {
                    std::string opStr(op);
                    if (opStr == "download" || opStr == "pull") type = "download";
                }
                
                ss << "{";
                ss << "\"file\":\"" << filename << "\",";
                ss << "\"peer\":\"Unknown\",";
                ss << "\"type\":\"" << type << "\",";
                ss << "\"status\":\"" << (status ? status : "pending") << "\",";
                ss << "\"progress\":0,";
                ss << "\"size\":\"-\",";
                ss << "\"speed\":\"-\"";
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
    }
    
    ss << "], \"history\": [";
    first = true;
    
    // Add transfer history
    if (ctx_.storage) {
        sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
        
        // First try file_access_log
        const char* historySql = "SELECT file_path, op_type, timestamp FROM file_access_log ORDER BY timestamp DESC LIMIT 20";
        sqlite3_stmt* stmt;
        bool hasAccessLog = false;
        
        if (sqlite3_prepare_v2(db, historySql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                hasAccessLog = true;
                if (!first) ss << ",";
                first = false;
                
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                const char* op = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                long long timestamp = sqlite3_column_int64(stmt, 2);
                
                std::string pathStr = path ? path : "";
                std::filesystem::path p(pathStr);
                std::string filename = p.filename().string();
                if (filename.empty()) filename = pathStr;
                
                std::string timeStr = formatTime(timestamp);
                
                ss << "{";
                ss << "\"file\":\"" << filename << "\",";
                ss << "\"type\":\"" << (op ? op : "sync") << "\",";
                ss << "\"time\":\"" << timeStr << "\"";
                ss << "}";
            }
            sqlite3_finalize(stmt);
        }
        
        // Fallback to files table
        if (!hasAccessLog) {
            const char* filesSql = "SELECT path, size, timestamp, synced FROM files WHERE synced = 1 AND timestamp > 0 ORDER BY timestamp DESC LIMIT 20";
            if (sqlite3_prepare_v2(db, filesSql, -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    if (!first) ss << ",";
                    first = false;
                    
                    const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    long long size = sqlite3_column_int64(stmt, 1);
                    long long timestamp = sqlite3_column_int64(stmt, 2);
                    
                    std::string pathStr = path ? path : "";
                    std::filesystem::path p(pathStr);
                    std::string filename = p.filename().string();
                    if (filename.empty()) filename = pathStr;
                    
                    std::string timeStr = formatTime(timestamp);
                    
                    ss << "{";
                    ss << "\"file\":\"" << filename << "\",";
                    ss << "\"type\":\"sync\",";
                    ss << "\"size\":\"" << formatBytes(size) << "\",";
                    ss << "\"time\":\"" << timeStr << "\"";
                    ss << "}";
                }
                sqlite3_finalize(stmt);
            }
        }
    }
    
    ss << "]}\n";
    return ss.str();
}

std::string TransferCommands::handleMetricsJson() {
    auto& metrics = MetricsCollector::instance();
    auto netMetrics = metrics.getNetworkMetrics();
    
    std::stringstream ss;
    ss << "{";
    ss << "\"totalUploaded\": " << netMetrics.bytesUploaded << ",";
    ss << "\"totalDownloaded\": " << netMetrics.bytesDownloaded << ",";
    ss << "\"filesSynced\": " << metrics.getSyncMetrics().filesSynced;
    ss << "}\n";
    return ss.str();
}

std::string TransferCommands::handleRelayStatus() {
    if (!ctx_.network) {
        return "Error: Network subsystem not ready\n";
    }
    
    bool enabled = ctx_.network->isRelayEnabled();
    bool connected = ctx_.network->isRelayConnected();
    
    std::stringstream ss;
    ss << "{\"enabled\":" << (enabled ? "true" : "false") << ",";
    ss << "\"connected\":" << (connected ? "true" : "false") << "}\n";
    return ss.str();
}

std::string TransferCommands::handleSetEncryption(const std::string& args) {
    bool enable = (args == "true" || args == "1" || args == "on");
    if (ctx_.network) {
        ctx_.network->setEncryptionEnabled(enable);
        return enable ? "Encryption enabled.\n" : "Encryption disabled.\n";
    }
    return "Error: Network not initialized\n";
}

std::string TransferCommands::handleSetSessionCode(const std::string& args) {
    if (args.length() != 6) {
        return "Error: Session code must be 6 characters\n";
    }
    if (ctx_.network) {
        ctx_.network->setSessionCode(args);
        return "Session code set: " + args + "\n";
    }
    return "Error: Network not initialized\n";
}

std::string TransferCommands::handleGenerateCode() {
    static const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string code;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
    for (int i = 0; i < 6; i++) {
        code += chars[dis(gen)];
    }
    if (ctx_.network) {
        ctx_.network->setSessionCode(code);
        return "CODE:" + code + "\n";
    }
    return "Error: Network plugin not initialized.\n";
}

} // namespace SentinelFS
