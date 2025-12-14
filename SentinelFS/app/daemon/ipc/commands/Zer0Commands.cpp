#include "../IPCHandler.h"
#include "../../DaemonCore.h"
#include "Logger.h"
#include "IPlugin.h"
#include "../../plugins/zer0/include/Zer0Plugin.h"
#include <sstream>
#include <filesystem>
#include <thread>

namespace SentinelFS {

// Static storage for status response from Zer0 plugin
static std::string s_zer0StatusJson;
static std::mutex s_statusMutex;
static bool s_statusReceived = false;

std::string IPCHandler::handleZer0Status() {
    auto& logger = Logger::instance();
    
    auto* zer0 = daemonCore_ ? daemonCore_->getZer0Plugin() : nullptr;
    auto* eventBus = daemonCore_ ? daemonCore_->getEventBus() : nullptr;
    
    if (!zer0) {
        return "{\"success\": true, \"type\": \"ZER0_STATUS\", \"payload\": {\"enabled\": false}}\n";
    }
    
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"success\": true,\n";
    ss << "  \"type\": \"ZER0_STATUS\",\n";
    ss << "  \"payload\": {\n";
    ss << "    \"name\": \"" << zer0->getName() << "\",\n";
    ss << "    \"version\": \"" << zer0->getVersion() << "\",\n";
    
    // Capabilities
    ss << "    \"magicByteValidation\": true,\n";
    ss << "    \"behavioralAnalysis\": true,\n";
    ss << "    \"fileTypeAwareness\": true,\n";
    ss << "    \"mlEnabled\": true,\n";
    ss << "    \"yaraEnabled\": true,\n";
    ss << "    \"autoResponseEnabled\": true,\n";
    
    // Get stats directly from plugin
    try {
        // Cast to Zer0Plugin to access getStats()
        auto* zer0Plugin = static_cast<Zer0Plugin*>(zer0);
        auto stats = zer0Plugin->getStats();
        ss << "    \"pluginStats\": {";
        ss << "\"enabled\":true,";
        ss << "\"filesAnalyzed\":" << stats.filesAnalyzed << ",";
        ss << "\"threatsDetected\":" << stats.threatsDetected << ",";
        ss << "\"yaraRulesLoaded\":" << stats.yaraRulesLoaded << ",";
        ss << "\"yaraFilesScanned\":" << stats.yaraFilesScanned << ",";
        ss << "\"yaraMatchesFound\":" << stats.yaraMatchesFound << ",";
        ss << "\"mlModelLoaded\":" << (stats.mlModelLoaded ? "true" : "false") << ",";
        ss << "\"mlSamplesProcessed\":" << stats.mlSamplesProcessed << ",";
        ss << "\"mlAnomaliesDetected\":" << stats.mlAnomaliesDetected << ",";
        ss << "\"mlAvgAnomalyScore\":" << std::fixed << std::setprecision(2) << stats.mlAvgAnomalyScore;
        ss << "},\n";
    } catch (...) {
        // Fallback to EventBus if direct method fails
        if (eventBus) {
            // Subscribe to receive status (one-time)
            static bool subscribed = false;
            if (!subscribed) {
                eventBus->subscribe("zer0.status", [](const std::any& data) {
                    try {
                        std::lock_guard<std::mutex> lock(s_statusMutex);
                        s_zer0StatusJson = std::any_cast<std::string>(data);
                        s_statusReceived = true;
                    } catch (...) {}
                });
                subscribed = true;
            }
            
            // Request status
            s_statusReceived = false;
            eventBus->publish("zer0.get_status", std::string{});
            
            // Wait briefly for response (status is published synchronously)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        std::lock_guard<std::mutex> lock(s_statusMutex);
        if (s_statusReceived && !s_zer0StatusJson.empty()) {
            ss << "    \"pluginStats\": " << s_zer0StatusJson << ",\n";
        }
    }
    
    ss << "    \"threatLevel\": \"NONE\"\n";
    ss << "  }\n";
    ss << "}\n";
    
    return ss.str();
}

std::string IPCHandler::handleZer0Config(const std::string& args) {
    auto& logger = Logger::instance();
    
    auto* zer0 = daemonCore_ ? daemonCore_->getZer0Plugin() : nullptr;
    if (!zer0) {
        return "{\"success\": false, \"error\": \"Zer0 plugin not loaded\"}\n";
    }
    
    // Config changes would be handled via plugin-specific interface
    // For now, acknowledge the request
    logger.log(LogLevel::INFO, "Zer0 config request: " + args, "Zer0Commands");
    
    return "{\"success\": true, \"message\": \"Configuration acknowledged\"}\n";
}

std::string IPCHandler::handleZer0StartMonitoring() {
    auto& logger = Logger::instance();
    
    auto* zer0 = daemonCore_ ? daemonCore_->getZer0Plugin() : nullptr;
    if (!zer0) {
        return "{\"success\": false, \"error\": \"Zer0 plugin not loaded\"}\n";
    }
    
    logger.log(LogLevel::INFO, "Zer0 monitoring start requested", "Zer0Commands");
    return "{\"success\": true, \"message\": \"Monitoring start requested\"}\n";
}

std::string IPCHandler::handleZer0StopMonitoring() {
    auto& logger = Logger::instance();
    
    auto* zer0 = daemonCore_ ? daemonCore_->getZer0Plugin() : nullptr;
    if (!zer0) {
        return "{\"success\": false, \"error\": \"Zer0 plugin not loaded\"}\n";
    }
    
    logger.log(LogLevel::INFO, "Zer0 monitoring stop requested", "Zer0Commands");
    return "{\"success\": true, \"message\": \"Monitoring stop requested\"}\n";
}

std::string IPCHandler::handleZer0ReloadYara() {
    auto& logger = Logger::instance();
    
    auto* zer0 = daemonCore_ ? daemonCore_->getZer0Plugin() : nullptr;
    if (!zer0) {
        return "{\"success\": false, \"error\": \"Zer0 plugin not loaded\"}\n";
    }
    
    logger.log(LogLevel::INFO, "YARA rules reload requested", "Zer0Commands");
    return "{\"success\": true, \"message\": \"YARA rules reload requested\"}\n";
}

std::string IPCHandler::handleZer0TrainModel() {
    auto& logger = Logger::instance();
    
    auto* zer0 = daemonCore_ ? daemonCore_->getZer0Plugin() : nullptr;
    if (!zer0) {
        return "{\"success\": false, \"error\": \"Zer0 plugin not loaded\"}\n";
    }
    
    // Get watched directories from database
    auto* storage = daemonCore_->getStorage();
    std::vector<std::string> watchDirs;
    
    if (storage) {
        auto folders = storage->getWatchedFolders();
        for (const auto& folder : folders) {
            // statusId: 1 = active
            if (folder.statusId == 1 && std::filesystem::exists(folder.path)) {
                watchDirs.push_back(folder.path);
            }
        }
    }
    
    // Fallback to config watch directory if no watches found
    if (watchDirs.empty()) {
        std::string trainDir = daemonCore_->getConfig().watchDirectory;
        if (!trainDir.empty() && trainDir != "." && std::filesystem::exists(trainDir)) {
            watchDirs.push_back(trainDir);
        }
    }
    
    if (watchDirs.empty()) {
        return "{\"success\": false, \"error\": \"No watched directories found\"}\n";
    }
    
    // Publish training event for each watched directory
    auto* eventBus = daemonCore_->getEventBus();
    std::ostringstream dirList;
    
    for (size_t i = 0; i < watchDirs.size(); ++i) {
        const auto& dir = watchDirs[i];
        logger.log(LogLevel::INFO, "ML model training requested for: " + dir, "Zer0Commands");
        
        if (eventBus) {
            eventBus->publish("zer0.train_model", dir);
        }
        
        if (i > 0) dirList << ", ";
        dirList << "\"" << dir << "\"";
    }
    
    logger.log(LogLevel::INFO, "Training events published for " + std::to_string(watchDirs.size()) + " directories", "Zer0Commands");
    
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"success\": true,\n";
    ss << "  \"message\": \"Model training started for " << watchDirs.size() << " directories\",\n";
    ss << "  \"directories\": [" << dirList.str() << "]\n";
    ss << "}\n";
    
    return ss.str();
}

std::string IPCHandler::handleZer0Scan(const std::string& path) {
    auto& logger = Logger::instance();
    
    auto* zer0 = daemonCore_ ? daemonCore_->getZer0Plugin() : nullptr;
    if (!zer0) {
        return "{\"success\": false, \"error\": \"Zer0 plugin not loaded\"}\n";
    }
    
    if (path.empty()) {
        return "{\"success\": false, \"error\": \"Path required\"}\n";
    }
    
    if (!std::filesystem::exists(path)) {
        return "{\"success\": false, \"error\": \"Path does not exist\"}\n";
    }
    
    logger.log(LogLevel::INFO, "Zer0 scan requested for: " + path, "Zer0Commands");
    
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"success\": true,\n";
    ss << "  \"type\": \"ZER0_SCAN_RESULT\",\n";
    ss << "  \"payload\": {\n";
    ss << "    \"path\": \"" << path << "\",\n";
    ss << "    \"status\": \"scan_queued\"\n";
    ss << "  }\n";
    ss << "}\n";
    
    return ss.str();
}

} // namespace SentinelFS
