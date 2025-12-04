#pragma once

#include "CommandHandler.h"
#include "HealthReport.h"
#include <string>

namespace SentinelFS {

/**
 * @brief Handles status and health related IPC commands
 * 
 * Commands: STATUS, STATUS_JSON, PLUGINS, STATS, HEALTH
 */
class StatusCommands : public CommandHandler {
public:
    explicit StatusCommands(CommandContext& ctx) : CommandHandler(ctx) {}
    
    // CLI commands
    std::string handleStatus();
    std::string handlePlugins();
    std::string handleStats();
    
    // JSON commands for GUI
    std::string handleStatusJson();
    
    // Health reporting
    HealthSummary computeHealthSummary() const;
    std::vector<PeerHealthReport> computePeerHealthReports() const;
    AnomalyReport getAnomalyReport() const;
    
    void setHealthThresholds(const HealthThresholds& thresholds) {
        healthThresholds_ = thresholds;
    }
    
private:
    HealthThresholds healthThresholds_;
};

} // namespace SentinelFS
