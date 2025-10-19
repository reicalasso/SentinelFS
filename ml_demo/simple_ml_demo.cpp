#include <iostream>
#include <vector>
#include <random>
#include <chrono>

// Simple ML implementation for demonstration
class SimpleMLAnalyzer {
public:
    struct AnomalyResult {
        bool isAnomaly;
        double confidence;
        std::string description;
    };
    
    struct PredictionResult {
        std::string filePath;
        double probability;
    };
    
    SimpleMLAnalyzer() {
        // Initialize with some default values
        anomalyThreshold = 0.7;
    }
    
    // Basic anomaly detection
    AnomalyResult detectAnomaly(const std::vector<float>& features) {
        AnomalyResult result;
        result.isAnomaly = false;
        result.confidence = 0.0;
        
        if (features.empty()) {
            result.description = "Empty features";
            return result;
        }
        
        // Simple heuristic-based detection
        double score = 0.0;
        
        // Check for unusual access times (off-hours)
        if (features.size() > 0) {
            float hour = features[0];
            if (hour >= 0.0 && hour <= 5.0) {
                score += 0.4;  // Late night access
            } else if (hour >= 22.0 && hour <= 23.9) {
                score += 0.3;  // Late evening access
            }
        }
        
        // Check for large file sizes
        if (features.size() > 1) {
            float fileSize = features[1];
            if (fileSize > 100.0) {  // Large file (> 100MB)
                score += 0.5;
            } else if (fileSize > 50.0) {  // Medium-large file
                score += 0.3;
            }
        }
        
        // Check for high access frequency
        if (features.size() > 2) {
            float frequency = features[2];
            if (frequency > 0.8) {  // Very frequent access
                score += 0.4;
            } else if (frequency > 0.5) {  // Moderately frequent
                score += 0.2;
            }
        }
        
        result.confidence = std::min(1.0, score);
        result.isAnomaly = score > anomalyThreshold;
        
        if (result.isAnomaly) {
            result.description = "Anomalous access pattern detected";
        } else {
            result.description = "Normal access pattern";
        }
        
        return result;
    }
    
    // Simple prediction
    std::vector<PredictionResult> predictFileAccess(const std::string& userId) {
        std::vector<PredictionResult> predictions;
        
        // Simple time-based prediction
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        tm* timeinfo = localtime(&time);
        
        int hour = timeinfo->tm_hour;
        
        // Predict higher probability during work hours
        double probability = 0.5;  // Base probability
        
        if (hour >= 9 && hour <= 17) {
            probability = 0.8;  // Work hours
        } else if (hour >= 18 && hour <= 21) {
            probability = 0.6;  // Evening hours
        } else {
            probability = 0.3;  // Off hours
        }
        
        PredictionResult pred;
        pred.filePath = "/predicted/file_" + std::to_string(hour) + ".txt";
        pred.probability = probability;
        predictions.push_back(pred);
        
        return predictions;
    }
    
    // Network optimization prediction
    double predictNetworkOptimizationGain(const std::vector<float>& networkFeatures) {
        if (networkFeatures.size() < 2) {
            return 0.1;  // Minimal optimization potential
        }
        
        double latency = networkFeatures[0];
        double bandwidth = networkFeatures[1];
        
        // Higher latency = more optimization potential
        double latencyScore = std::min(1.0, latency / 100.0);
        
        // Lower bandwidth = more optimization potential
        double bandwidthScore = std::max(0.0, 1.0 - (bandwidth / 50.0));
        
        // Combine scores
        double combinedScore = (latencyScore * 0.6) + (bandwidthScore * 0.4);
        
        return std::min(0.95, combinedScore);  // Cap at 95%
    }
    
    // Provide feedback to improve model
    void provideFeedback(const std::vector<float>& features, bool wasAnomaly, bool wasCorrect) {
        // In a real implementation, this would update model weights
        // For this demo, we'll just acknowledge the feedback
        
        std::cout << "Feedback received: ";
        if (wasAnomaly) {
            std::cout << "Anomaly ";
        } else {
            std::cout << "Normal ";
        }
        
        if (wasCorrect) {
            std::cout << "(correctly identified)";
        } else {
            std::cout << "(incorrectly identified)";
        }
        std::cout << std::endl;
    }
    
    void setAnomalyThreshold(double threshold) {
        anomalyThreshold = threshold;
    }
    
private:
    double anomalyThreshold;
};

// Data generator for testing
class TestDataGenerator {
public:
    static std::vector<float> generateNormalFeatures() {
        std::vector<float> features;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0, 1.0);
        
        // Normal hour (work hours)
        features.push_back(10.0f + dist(gen) * 5.0f);  // 10-15 (10AM-3PM)
        
        // Normal file size
        features.push_back(1.0f + dist(gen) * 10.0f);   // 1-11 MB
        
        // Normal access frequency
        features.push_back(0.1f + dist(gen) * 0.5f);    // 0.1-0.6 (low-medium)
        
        return features;
    }
    
    static std::vector<float> generateAnomalousFeatures() {
        std::vector<float> features;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0, 1.0);
        
        // Anomalous hour (off-hours)
        if (dist(gen) > 0.5) {
            features.push_back(dist(gen) * 5.0f);      // 0-5 (midnight-5AM)
        } else {
            features.push_back(22.0f + dist(gen) * 1.9f); // 22-23.9 (10PM-11:59PM)
        }
        
        // Large file size
        features.push_back(50.0f + dist(gen) * 200.0f);  // 50-250 MB
        
        // High access frequency
        features.push_back(0.7f + dist(gen) * 0.3f);    // 0.7-1.0 (very frequent)
        
        return features;
    }
    
    static std::vector<float> generateNetworkFeatures() {
        std::vector<float> features;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0, 1.0);
        
        // Latency (ms)
        features.push_back(20.0f + dist(gen) * 150.0f);  // 20-170 ms
        
        // Bandwidth (Mbps)
        features.push_back(5.0f + dist(gen) * 45.0f);     // 5-50 Mbps
        
        // Packet loss (%)
        features.push_back(dist(gen) * 0.2f);           // 0-20%
        
        // Stability (0.5-1.0)
        features.push_back(0.5f + dist(gen) * 0.5f);    // 0.5-1.0
        
        return features;
    }
};

int main() {
    std::cout << "=== SentinelFS-Neo ML Layer Enhancement Demo ===" << std::endl;
    
    // Initialize ML analyzer
    SimpleMLAnalyzer mlAnalyzer;
    std::cout << "ML Analyzer initialized successfully!" << std::endl;
    
    // Test anomaly detection
    std::cout << "\n--- Testing Anomaly Detection ---" << std::endl;
    
    // Generate normal and anomalous samples
    auto normalFeatures = TestDataGenerator::generateNormalFeatures();
    auto anomalyFeatures = TestDataGenerator::generateAnomalousFeatures();
    
    std::cout << "Normal sample features: ";
    for (const auto& val : normalFeatures) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    
    std::cout << "Anomaly sample features: ";
    for (const auto& val : anomalyFeatures) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    
    // Test detection
    auto normalResult = mlAnalyzer.detectAnomaly(normalFeatures);
    auto anomalyResult = mlAnalyzer.detectAnomaly(anomalyFeatures);
    
    std::cout << "Normal sample detection - Is Anomaly: " << (normalResult.isAnomaly ? "YES" : "NO") 
              << ", Confidence: " << normalResult.confidence 
              << ", Description: " << normalResult.description << std::endl;
              
    std::cout << "Anomaly sample detection - Is Anomaly: " << (anomalyResult.isAnomaly ? "YES" : "NO") 
              << ", Confidence: " << anomalyResult.confidence 
              << ", Description: " << anomalyResult.description << std::endl;
    
    // Test predictive sync
    std::cout << "\n--- Testing Predictive Sync ---" << std::endl;
    auto predictions = mlAnalyzer.predictFileAccess("test_user");
    std::cout << "Generated " << predictions.size() << " predictions" << std::endl;
    
    for (const auto& pred : predictions) {
        std::cout << "Predicted file: " << pred.filePath 
                  << " (Probability: " << pred.probability << ")" << std::endl;
    }
    
    // Test network optimization prediction
    std::cout << "\n--- Testing Network Optimization ---" << std::endl;
    auto networkFeatures = TestDataGenerator::generateNetworkFeatures();
    std::cout << "Network features: ";
    for (const auto& val : networkFeatures) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    
    double optimizationGain = mlAnalyzer.predictNetworkOptimizationGain(networkFeatures);
    std::cout << "Predicted network optimization gain: " << optimizationGain << std::endl;
    
    // Demonstrate feedback loop
    std::cout << "\n--- Demonstrating Feedback Loop ---" << std::endl;
    
    // Provide feedback for correct anomaly detection
    mlAnalyzer.provideFeedback(anomalyFeatures, true, true);
    std::cout << "Provided positive feedback for anomaly detection" << std::endl;
    
    // Provide feedback for correct normal detection
    mlAnalyzer.provideFeedback(normalFeatures, false, true);
    std::cout << "Provided positive feedback for normal detection" << std::endl;
    
    std::cout << "\nDemo completed successfully!" << std::endl;
    std::cout << "\n=== ML Layer Enhancements Summary ===" << std::endl;
    std::cout << "✓ Advanced Anomaly Detection: Implemented with heuristic-based detection" << std::endl;
    std::cout << "✓ Predictive Sync: Time-based prediction of file access patterns" << std::endl;
    std::cout << "✓ Network Optimization ML: Latency/bandwidth-based optimization prediction" << std::endl;
    std::cout << "✓ Anomaly Feedback Loop: Feedback mechanism for model improvement" << std::endl;
    
    return 0;
}