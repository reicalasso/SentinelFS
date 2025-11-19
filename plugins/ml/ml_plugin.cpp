#include "IPlugin.h"
#include "EventBus.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <map>
#include <cmath>
#include <algorithm>

namespace SentinelFS {

    // Simple anomaly detection based on file activity patterns
    class MLPlugin : public IPlugin {
    private:
        EventBus* eventBus_;
        
        // Activity tracking
        struct FileActivity {
            std::string path;
            std::chrono::steady_clock::time_point timestamp;
            std::string action; // CREATE, MODIFY, DELETE
        };
        
        std::vector<FileActivity> recentActivity_;
        const size_t MAX_ACTIVITY_BUFFER = 1000;
        
        // Anomaly detection parameters
        const double RAPID_MODIFICATION_THRESHOLD = 10.0; // files per second
        const int RAPID_DELETION_THRESHOLD = 5; // consecutive deletions
        const int BULK_ENCRYPTION_THRESHOLD = 10; // rapid modifications
        
        // Statistics
        std::map<std::string, int> fileTypeCounter_;
        int consecutiveDeletions_ = 0;
        std::chrono::steady_clock::time_point lastCheckTime_;
        
    public:
        bool initialize(EventBus* eventBus) override {
            eventBus_ = eventBus;
            lastCheckTime_ = std::chrono::steady_clock::now();
            
            // Subscribe to file events
            eventBus_->subscribe("FILE_CREATED", [this](const std::any& data) {
                recordActivity("CREATE", data);
            });
            
            eventBus_->subscribe("FILE_MODIFIED", [this](const std::any& data) {
                recordActivity("MODIFY", data);
            });
            
            eventBus_->subscribe("FILE_DELETED", [this](const std::any& data) {
                recordActivity("DELETE", data);
                consecutiveDeletions_++;
                checkForRansomware();
            });
            
            std::cout << "MLPlugin initialized with anomaly detection" << std::endl;
            return true;
        }
        
        void recordActivity(const std::string& action, const std::any& data) {
            try {
                std::string path = std::any_cast<std::string>(data);
                
                FileActivity activity;
                activity.path = path;
                activity.action = action;
                activity.timestamp = std::chrono::steady_clock::now();
                
                recentActivity_.push_back(activity);
                
                // Keep buffer size limited
                if (recentActivity_.size() > MAX_ACTIVITY_BUFFER) {
                    recentActivity_.erase(recentActivity_.begin());
                }
                
                // Reset deletion counter on non-delete operations
                if (action != "DELETE") {
                    consecutiveDeletions_ = 0;
                }
                
                // Analyze for anomalies
                analyzeActivity();
                
            } catch (const std::bad_any_cast&) {
                // Ignore invalid data
            }
        }
        
        void analyzeActivity() {
            auto now = std::chrono::steady_clock::now();
            auto timeSinceLastCheck = std::chrono::duration_cast<std::chrono::seconds>(
                now - lastCheckTime_
            ).count();
            
            if (timeSinceLastCheck < 1) return; // Check every second
            
            // Count recent modifications (last 1 second)
            int recentMods = 0;
            for (const auto& activity : recentActivity_) {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(
                    now - activity.timestamp
                ).count();
                
                if (age <= 1 && activity.action == "MODIFY") {
                    recentMods++;
                }
            }
            
            // Check for rapid modifications (potential ransomware)
            if (recentMods >= BULK_ENCRYPTION_THRESHOLD) {
                std::cout << "⚠️  ML ALERT: Rapid file modifications detected (" 
                          << recentMods << " files/sec) - Possible ransomware!" << std::endl;
                eventBus_->publish("ANOMALY_DETECTED", std::string("RAPID_MODIFICATIONS"));
            }
            
            lastCheckTime_ = now;
        }
        
        void checkForRansomware() {
            // Multiple consecutive deletions could indicate malicious activity
            if (consecutiveDeletions_ >= RAPID_DELETION_THRESHOLD) {
                std::cout << "⚠️  ML ALERT: Multiple consecutive deletions (" 
                          << consecutiveDeletions_ << ") - Possible data destruction!" << std::endl;
                eventBus_->publish("ANOMALY_DETECTED", std::string("RAPID_DELETIONS"));
            }
        }
        
        void shutdown() override {
            std::cout << "MLPlugin shutdown" << std::endl;
            std::cout << "  Total activities recorded: " << recentActivity_.size() << std::endl;
        }

        std::string getName() const override {
            return "MLPlugin";
        }

        std::string getVersion() const override {
            return "1.0.0-anomaly-detection";
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

}