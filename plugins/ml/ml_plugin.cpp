#include "IPlugin.h"
#include "EventBus.h"
#include "AnomalyDetector.h"
#include <iostream>
#include <memory>

namespace SentinelFS {

/**
 * @brief ML-based anomaly detection plugin
 * 
 * Monitors file activity patterns and detects:
 * - Rapid file modifications (potential ransomware)
 * - Mass file deletions (data destruction)
 * - Other suspicious patterns
 */
class MLPlugin : public IPlugin {
public:
    MLPlugin() : detector_(std::make_unique<AnomalyDetector>()) {}

    bool initialize(EventBus* eventBus) override {
        eventBus_ = eventBus;
        
        // Set up alert callback
        detector_->setAlertCallback([this](const std::string& type, const std::string& details) {
            handleAnomaly(type, details);
        });
        
        // Subscribe to file events
        eventBus_->subscribe("FILE_CREATED", [this](const std::any& data) {
            handleFileEvent("CREATE", data);
        });
        
        eventBus_->subscribe("FILE_MODIFIED", [this](const std::any& data) {
            handleFileEvent("MODIFY", data);
        });
        
        eventBus_->subscribe("FILE_DELETED", [this](const std::any& data) {
            handleFileEvent("DELETE", data);
        });
        
        std::cout << "MLPlugin initialized with anomaly detection" << std::endl;
        return true;
    }

    void shutdown() override {
        std::cout << "MLPlugin shutdown" << std::endl;
        std::cout << "  Total activities recorded: " << detector_->getActivityCount() << std::endl;
    }

    std::string getName() const override {
        return "MLPlugin";
    }

    std::string getVersion() const override {
        return "1.0.0-anomaly-detection";
    }

private:
    EventBus* eventBus_ = nullptr;
    std::unique_ptr<AnomalyDetector> detector_;

    /**
     * @brief Handle file system event
     */
    void handleFileEvent(const std::string& action, const std::any& data) {
        try {
            std::string path = std::any_cast<std::string>(data);
            detector_->recordActivity(action, path);
        } catch (const std::bad_any_cast&) {
            // Ignore invalid data
        }
    }

    /**
     * @brief Handle detected anomaly
     */
    void handleAnomaly(const std::string& type, const std::string& details) {
        if (eventBus_) {
            eventBus_->publish("ANOMALY_DETECTED", type);
        }
    }
};

extern "C" {
    IPlugin* create_plugin() {
        return new MLPlugin();
    }

    void destroy_plugin(IPlugin* plugin) {
        delete plugin;
    }
}

} // namespace SentinelFS
