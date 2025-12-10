#pragma once

#include "CommandHandler.h"
#include <string>

namespace SentinelFS {

/**
 * @brief Handles transfer and bandwidth related IPC commands
 * 
 * Commands: TRANSFERS_JSON, METRICS, METRICS_JSON, UPLOAD-LIMIT, DOWNLOAD-LIMIT,
 *           PAUSE, RESUME, DISCOVER, SET_DISCOVERY
 */
class TransferCommands : public CommandHandler {
public:
    explicit TransferCommands(CommandContext& ctx) : CommandHandler(ctx) {}
    
    // CLI commands
    std::string handleMetrics();
    std::string handleUploadLimit(const std::string& args);
    std::string handleDownloadLimit(const std::string& args);
    std::string handlePause();
    std::string handleResume();
    std::string handleDiscover();
    std::string handleSetDiscovery(const std::string& args);
    
    // JSON commands for GUI
    std::string handleTransfersJson();
    std::string handleMetricsJson();
    std::string handleRelayStatus();
    
    // Encryption and session
    std::string handleSetEncryption(const std::string& args);
    std::string handleSetSessionCode(const std::string& args);
    std::string handleGenerateCode();
};

} // namespace SentinelFS
