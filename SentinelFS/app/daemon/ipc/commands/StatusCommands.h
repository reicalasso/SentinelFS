#pragma once

#include "CommandHandler.h"
#include "HealthReport.h"
#include <string>

namespace SentinelFS {

/**
 * @brief ML Threat status report for IPC
 */
struct ThreatStatusReport {
    double threatScore{0.0};
    std::string threatLevel;
    uint64_t totalThreats{0};
    uint64_t ransomwareAlerts{0};
    uint64_t highEntropyFiles{0};
    uint64_t massOperationAlerts{0};
    double avgFileEntropy{0.0};
    bool mlEnabled{false};
};

/**
 * @brief Handles status and health related IPC commands
 * 
 * Commands: STATUS, STATUS_JSON, PLUGINS, STATS, HEALTH, THREAT_STATUS
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
    
    // ML Threat status
    std::string handleThreatStatus();
    std::string handleThreatStatusJson();
    ThreatStatusReport getThreatStatus() const;
    
    void setHealthThresholds(const HealthThresholds& thresholds) {
        healthThresholds_ = thresholds;
    }
    
private:
    HealthThresholds healthThresholds_;
};

} // namespace SentinelFS
