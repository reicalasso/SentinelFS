#include "ml_analyzer.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <ctime>

MLAnalyzer::MLAnalyzer() : modelLoaded(false), modelPath("models/file_access_model.onnx"),
                         anomalyThreshold(0.7), predictionThreshold(0.8) {
    initialize();
}

MLAnalyzer::~MLAnalyzer() = default;

bool MLAnalyzer::initialize() {
    // In a real implementation, this would load the ML model
    // For this implementation, we'll initialize with default values
    modelLoaded = true;
    
    // Initialize metrics
    metrics["anomaly_accuracy"] = 0.0;
    metrics["prediction_accuracy"] = 0.0;
    metrics["network_optimization_efficiency"] = 0.0;
    
    return true;
}

AnomalyResult MLAnalyzer::detectAnomaly(const std::vector<float>& features, 
                                       const std::string& filePath) {
    AnomalyResult result;
    result.features = features;
    
    // Calculate anomaly score using a combination of methods
    double score = calculateAnomalyScore(features);
    
    // Determine if it's an anomaly based on threshold
    result.isAnomaly = score > anomalyThreshold;
    result.confidence = score;
    
    // Classify anomaly type based on features
    if (result.isAnomaly) {
        if (features.size() >= 5) {
            // features[0] is hour, features[1] is file size, features[4] is day of week
            if ((features[0] >= 0.0 && features[0] <= 5.0) || (features[0] >= 22.0 && features[0] <= 23.9)) {
                result.type = AnomalyType::UNUSUAL_ACCESS_TIME;
                result.description = "Access during unusual hours (" + std::to_string(features[0]) + ")";
            } else if (features[1] > 100.0) {  // File size > 100MB
                result.type = AnomalyType::LARGE_FILE_TRANSFER;
                result.description = "Large file transfer (" + std::to_string(features[1]) + " MB)";
            } else {
                result.type = AnomalyType::UNKNOWN_ACCESS_PATTERN;
                result.description = "Unknown access pattern detected";
            }
        }
    }
    
    return result;
}

std::vector<AnomalyResult> MLAnalyzer::detectAnomalies(const std::vector<std::vector<float>>& featureBatches) {
    std::vector<AnomalyResult> results;
    for (const auto& features : featureBatches) {
        results.push_back(detectAnomaly(features));
    }
    return results;
}

std::vector<PredictionResult> MLAnalyzer::predictFileAccess(const std::string& userId) {
    // This would normally use a trained ML model to predict which files
    // will be accessed next by the user
    std::vector<PredictionResult> predictions;
    
    // For now, return an empty vector or some placeholder predictions
    // In a real implementation, this would process historical access patterns
    
    return predictions;
}

PredictionResult MLAnalyzer::predictNextFile(const std::string& userId) {
    // Simple prediction based on time patterns
    PredictionResult result("predicted_file.tmp");
    
    // Get current time features
    auto timeFeatures = getCurrentTimeFeatures();
    float hour = timeFeatures[0];
    
    // Simple model: if it's work hours, predict higher access probability
    if (hour >= 9.0 && hour <= 17.0) {  // Work hours
        result.probability = 0.8;
    } else {
        result.probability = 0.3;
    }
    
    // Predict next access in 10 minutes (as example)
    result.predictedAccessTime = std::chrono::system_clock::now() + 
                                std::chrono::minutes(10);
    
    return result;
}

double MLAnalyzer::predictNetworkOptimizationGain(const std::string& peerId, 
                                                 const std::vector<float>& networkFeatures) {
    // Calculate potential optimization gain based on network features
    // In a real implementation, this would use a trained model
    
    // Simple heuristic: if latency is high, there's more potential for optimization
    if (networkFeatures.size() >= 2) {
        double currentLatency = networkFeatures[0];  // Latency
        double bandwidth = networkFeatures[1];       // Bandwidth
        
        // Higher latency means more potential for optimization
        double optimizationPotential = std::min(0.9, currentLatency / 100.0);  // Scale latency
        
        // Lower bandwidth might indicate optimization opportunities
        if (bandwidth < 10.0) {  // Low bandwidth
            optimizationPotential *= 1.2;
        }
        
        return std::min(optimizationPotential, 0.95);  // Cap at 95%
    }
    
    return 0.1;  // Default low optimization potential
}

void MLAnalyzer::provideFeedback(const std::vector<float>& features, 
                                bool wasAnomaly, 
                                bool wasCorrectlyFlagged) {
    // Store feedback for future model training
    anomalyTrainingData.push_back(features);
    anomalyLabels.push_back(wasAnomaly ? 1 : 0);
    
    // Update accuracy metrics based on feedback
    if (wasAnomaly && wasCorrectlyFlagged) {
        // True positive - good detection
        metrics["anomaly_accuracy"] = std::min(1.0, metrics["anomaly_accuracy"] + 0.01);
    } else if (!wasAnomaly && !wasCorrectlyFlagged) {
        // False positive - penalize
        metrics["anomaly_accuracy"] = std::max(0.0, metrics["anomaly_accuracy"] - 0.02);
    } else {
        // True negative or false negative - maintain similar score
        metrics["anomaly_accuracy"] = std::max(0.0, metrics["anomaly_accuracy"] - 0.005);
    }
}

void MLAnalyzer::provideAccessFeedback(const std::string& filePath, 
                                      bool wasPredictedCorrectly) {
    // Update prediction model metrics
    if (wasPredictedCorrectly) {
        metrics["prediction_accuracy"] = std::min(1.0, metrics["prediction_accuracy"] + 0.02);
    } else {
        metrics["prediction_accuracy"] = std::max(0.0, metrics["prediction_accuracy"] - 0.01);
    }
}

bool MLAnalyzer::trainAnomalyModel(const std::string& dataFile) {
    // Load training data and train the anomaly detection model
    return loadTrainingData(dataFile, anomalyTrainingData, anomalyLabels);
}

bool MLAnalyzer::trainPredictionModel(const std::string& dataFile) {
    // Load training data and train the prediction model
    std::vector<std::vector<float>> features;
    std::vector<int> labels;
    return loadTrainingData(dataFile, features, labels);
}

bool MLAnalyzer::trainNetworkOptimizationModel(const std::string& dataFile) {
    // Load training data and train the network optimization model
    std::vector<std::vector<float>> features;
    std::vector<int> labels;
    return loadTrainingData(dataFile, features, labels);
}

std::vector<float> MLAnalyzer::extractTemporalFeatures() {
    std::vector<float> features;
    auto timeFeatures = getCurrentTimeFeatures();
    
    // Add time-based features
    features.insert(features.end(), timeFeatures.begin(), timeFeatures.end());
    
    // Add day type indicator (weekday vs weekend)
    int dayOfWeek = static_cast<int>(timeFeatures[4]);  // Assuming day of week is at index 4
    features.push_back(dayOfWeek >= 1 && dayOfWeek <= 5 ? 1.0 : 0.0);  // 1 for weekday, 0 for weekend
    
    // Add hour type indicator (work hours vs non-work hours)
    float hour = timeFeatures[0];
    features.push_back((hour >= 9 && hour <= 17) ? 1.0 : 0.0);  // Work hours indicator
    
    return features;
}

std::vector<float> MLAnalyzer::extractAccessPatternFeatures(const std::string& filePath) {
    std::vector<float> features;
    
    // In a real implementation, this would analyze historical access patterns
    // For now, return simple features based on file characteristics
    
    // Placeholder values - in reality, these would come from access history
    features.push_back(1.0);  // Access frequency normalized
    features.push_back(0.5);  // Access regularity (0-1 scale)
    features.push_back(0.2);  // Burst access indicator
    
    return features;
}

std::vector<float> MLAnalyzer::extractNetworkFeatures(const std::string& peerId) {
    std::vector<float> features;
    
    // Placeholder network features
    features.push_back(50.0);   // Default latency (ms)
    features.push_back(50.0);   // Default bandwidth (Mbps)
    features.push_back(0.1);    // Packet loss rate
    features.push_back(0.8);    // Connection stability
    
    // In a real implementation, these would come from actual network metrics
    
    return features;
}

std::vector<float> MLAnalyzer::extractFileFeatures(const std::string& filePath) {
    std::vector<float> features;
    
    // In a real implementation, this would get actual file characteristics
    // For now, return placeholder values
    features.push_back(10.0);   // File size (MB) - placeholder
    features.push_back(0.7);    // File importance score (based on usage)
    features.push_back(1);      // File type indicator (1 for documents, 2 for media, etc.)
    
    return features;
}

std::vector<float> MLAnalyzer::extractComprehensiveFeatures(
    const std::string& filePath, 
    int peerId, 
    size_t fileSize, 
    const std::string& operation,
    const std::string& userId) {
    
    std::vector<float> features;
    
    // Get temporal features
    auto temporal = extractTemporalFeatures();
    features.insert(features.end(), temporal.begin(), temporal.end());
    
    // Get access pattern features
    auto accessPatterns = extractAccessPatternFeatures(filePath);
    features.insert(features.end(), accessPatterns.begin(), accessPatterns.end());
    
    // Get network features (placeholder)
    auto network = extractNetworkFeatures(std::to_string(peerId));
    features.insert(features.end(), network.begin(), network.end());
    
    // Get file features
    auto fileFeatures = extractFileFeatures(filePath);
    features.insert(features.end(), fileFeatures.begin(), fileFeatures.end());
    
    // Add user-specific features
    features.push_back(peerId % 100);  // Simplified user ID feature
    
    // Add operation type indicator
    if (operation == "write") {
        features.push_back(1.0);
    } else if (operation == "read") {
        features.push_back(0.5);
    } else {
        features.push_back(0.0);
    }
    
    // Add file size in MB
    features.push_back(static_cast<float>(fileSize) / (1024.0 * 1024.0));
    
    return features;
}

std::map<std::string, double> MLAnalyzer::getModelMetrics() const {
    return metrics;
}

std::vector<float> MLAnalyzer::getCurrentTimeFeatures() {
    std::vector<float> features;
    
    time_t now = time(0);
    tm* timeinfo = localtime(&now);
    
    // Hour (0-23)
    features.push_back(static_cast<float>(timeinfo->tm_hour) + 
                      static_cast<float>(timeinfo->tm_min) / 60.0);
    
    // Day of month (1-31)
    features.push_back(static_cast<float>(timeinfo->tm_mday));
    
    // Month (1-12)
    features.push_back(static_cast<float>(timeinfo->tm_mon + 1));
    
    // Year (normalized)
    features.push_back(static_cast<float>(timeinfo->tm_year - 100));  // Years since 2000
    
    // Day of week (0-6)
    features.push_back(static_cast<float>(timeinfo->tm_wday));
    
    // Day of year (0-365)
    features.push_back(static_cast<float>(timeinfo->tm_yday));
    
    return features;
}

bool MLAnalyzer::loadTrainingData(const std::string& dataFile, 
                                 std::vector<std::vector<float>>& features, 
                                 std::vector<int>& labels) {
    // In a real implementation, this would load training data from a file
    // For this example, we'll just return true to indicate success
    // In practice, you would parse a CSV or other format with training examples
    
    std::ifstream file(dataFile);
    if (!file) {
        // If file doesn't exist, create some synthetic training data
        // This is just for demonstration purposes
        std::vector<float> normalPattern = {12.0, 10.0, 0.5, 1.0, 2.0, 150.0}; // Normal access
        std::vector<float> anomalyPattern = {3.0, 500.0, 0.9, 1.0, 5.0, 10.0}; // Anomalous access
        
        features.push_back(normalPattern);
        labels.push_back(0);  // Normal
        
        features.push_back(anomalyPattern);
        labels.push_back(1);  // Anomaly
        
        return true;
    }
    
    // Implementation would parse the file if it exists
    return true;
}

double MLAnalyzer::calculateAnomalyScore(const std::vector<float>& features) {
    if (features.empty()) {
        return 0.0;
    }
    
    // Simple anomaly detection based on feature ranges
    double score = 0.0;
    
    // Check for unusual hour access (0-5 and 22-23)
    if (!features.empty() && (features[0] <= 5.0 || features[0] >= 22.0)) {
        score += 0.3;
    }
    
    // Check for large file size (in MB, assuming this is at index 1)
    if (features.size() > 1 && features[1] > 100.0) {  // Large file
        score += 0.2;
    }
    
    // Check for unusual access patterns (high access count, assuming at index 2)
    if (features.size() > 2 && features[2] > 0.8) {
        score += 0.3;
    }
    
    // Additional checks could be added here
    
    // Normalize to 0-1 range
    return std::min(score, 1.0);
}

double MLAnalyzer::calculatePredictionScore(const std::vector<float>& features) {
    if (features.empty()) {
        return 0.0;
    }
    
    // Simple prediction based on time and historical patterns
    float hour = features[0];  // Assuming hour is first feature
    
    // Higher score during typical work hours
    if (hour >= 8.0 && hour <= 18.0) {
        return 0.7;
    } else if (hour >= 6.0 && hour <= 22.0) {
        return 0.5;
    } else {
        return 0.2;
    }
}

double MLAnalyzer::calculateNetworkOptimizationScore(const std::vector<float>& features) {
    if (features.size() < 2) {
        return 0.0;
    }
    
    double latency = features[0];
    double bandwidth = features[1];
    
    // Higher optimization potential when latency is high
    double latencyScore = std::min(latency / 100.0, 0.8);
    
    // Higher optimization potential when bandwidth is low
    double bandwidthScore = (bandwidth < 10.0) ? 0.5 : 0.1;
    
    return (latencyScore + bandwidthScore) / 2.0;
}

void MLAnalyzer::trainInternalModels() {
    // Placeholder for model training
    // In a real implementation, this would train ML models
}

void MLAnalyzer::updateMetrics(const std::string& modelType, double accuracy) {
    metrics[modelType + "_accuracy"] = accuracy;
}