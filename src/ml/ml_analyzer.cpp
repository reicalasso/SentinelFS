#include "ml_analyzer.hpp"
#include <ctime>
#include <cmath>
#include <cstdlib>

MLAnalyzer::MLAnalyzer() : modelLoaded(false), modelPath("models/file_access_model.onnx") {}

MLAnalyzer::~MLAnalyzer() = default;

bool MLAnalyzer::checkAnomaly(const std::string& logFile) {
    // For this basic implementation, we'll use a simple heuristic
    // In a real implementation, this would call a Python script or ONNX model
    // Simulate anomaly detection by generating a random result for now
    
    // In a real implementation, we would:
    // 1. Extract features from the log file
    // 2. Run the model
    // 3. Return true if anomaly detected
    
    // For now, return false to indicate no anomaly
    return false;
}

std::vector<float> MLAnalyzer::extractFeatures(const std::string& filePath, int peerId, size_t fileSize) {
    std::vector<float> features;
    
    // Current hour (0-23)
    time_t now = time(0);
    tm* timeinfo = localtime(&now);
    features.push_back(static_cast<float>(timeinfo->tm_hour));
    
    // File size in MB
    features.push_back(static_cast<float>(fileSize) / (1024.0f * 1024.0f));
    
    // Peer ID
    features.push_back(static_cast<float>(peerId));
    
    // Just a placeholder - in a real system, we'd track access count
    features.push_back(1.0f);
    
    // Day of week (0-6)
    features.push_back(static_cast<float>(timeinfo->tm_wday));
    
    return features;
}

bool MLAnalyzer::detectAnomaly(const std::vector<float>& features) {
    // Simple heuristic: if file access happens between midnight and 5 AM, flag as potential anomaly
    if (!features.empty()) {
        float hour = features[0];  // First element is hour
        if (hour >= 0.0f && hour <= 5.0f) {
            return true;  // Potential anomaly - access during unusual hours
        }
    }
    
    // Additional heuristic: extremely large files accessed at unusual times
    if (features.size() >= 2) {
        float hour = features[0];
        float fileSizeMB = features[1];
        
        // If large file (>100MB) accessed during unusual hours
        if ((hour >= 0.0f && hour <= 5.0f) && fileSizeMB > 100.0f) {
            return true;
        }
    }
    
    return false;
}

bool MLAnalyzer::trainModel(const std::string& dataFile) {
    // In a real implementation, this would train the model
    // For now, return true to indicate success
    return true;
}

float MLAnalyzer::getCurrentHour() {
    time_t now = time(0);
    tm* timeinfo = localtime(&now);
    return static_cast<float>(timeinfo->tm_hour);
}

float MLAnalyzer::getCurrentDayOfWeek() {
    time_t now = time(0);
    tm* timeinfo = localtime(&now);
    return static_cast<float>(timeinfo->tm_wday);
}

#ifdef USE_ONNX

ONNXAnalyzer::ONNXAnalyzer(const std::string& modelPath) 
    : env(ORT_LOGGING_LEVEL_WARNING, "SentinelFS"), modelLoaded(false) {
    
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    
    try {
        // Note: This would require the ONNXRuntime development libraries to be properly linked
        // For now, we'll simulate the loading process
        this->modelPath = modelPath;
        modelLoaded = true;  // In real implementation, check if model loads successfully
    } catch (...) {
        modelLoaded = false;
    }
}

ONNXAnalyzer::~ONNXAnalyzer() = default;

bool ONNXAnalyzer::predict(const std::vector<float>& features) {
    if (!modelLoaded) {
        return false;
    }
    
    // In a real implementation, this would call the ONNX model
    // For now, use the same simple heuristic as above
    if (!features.empty()) {
        float hour = features[0];  // First element is hour
        if (hour >= 0.0f && hour <= 5.0f) {
            return true;  // Potential anomaly - access during unusual hours
        }
    }
    
    return false;
}

std::vector<bool> ONNXAnalyzer::predictBatch(const std::vector<std::vector<float>>& features_batch) {
    std::vector<bool> results;
    for (const auto& features : features_batch) {
        results.push_back(predict(features));
    }
    return results;
}

#endif