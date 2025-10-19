#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <fstream>

// Forward declarations to avoid heavy dependencies
struct FileEvent;

// Anomaly types for classification
enum class AnomalyType {
    UNUSUAL_ACCESS_TIME,
    LARGE_FILE_TRANSFER,
    FREQUENT_ACCESS_PATTERN,
    UNKNOWN_ACCESS_PATTERN,
    ACCESS_DURING_SUSPICIOUS_ACTIVITY
};

struct AnomalyResult {
    bool isAnomaly;
    AnomalyType type;
    double confidence;
    std::vector<float> features;
    std::string description;
    
    AnomalyResult() : isAnomaly(false), type(AnomalyType::UNKNOWN_ACCESS_PATTERN), confidence(0.0) {}
};

struct PredictionResult {
    std::string filePath;
    double probability;
    std::chrono::system_clock::time_point predictedAccessTime;
    
    PredictionResult(const std::string& path) : filePath(path), probability(0.0),
        predictedAccessTime(std::chrono::system_clock::now()) {}
};

class MLAnalyzer {
public:
    MLAnalyzer();
    ~MLAnalyzer();
    
    // Initialize ML components
    bool initialize();
    
    // Enhanced anomaly detection with multiple models
    AnomalyResult detectAnomaly(const std::vector<float>& features, 
                               const std::string& filePath = "");
    
    // Batch anomaly detection
    std::vector<AnomalyResult> detectAnomalies(const std::vector<std::vector<float>>& featureBatches);
    
    // Predictive sync model
    std::vector<PredictionResult> predictFileAccess(const std::string& userId = "");
    
    // Predict next file to be accessed
    PredictionResult predictNextFile(const std::string& userId = "");
    
    // Network optimization prediction
    double predictNetworkOptimizationGain(const std::string& peerId, 
                                         const std::vector<float>& networkFeatures);
    
    // Feedback loop for model improvement
    void provideFeedback(const std::vector<float>& features, 
                        bool wasAnomaly, 
                        bool wasCorrectlyFlagged);
    
    void provideAccessFeedback(const std::string& filePath, 
                              bool wasPredictedCorrectly);
    
    // Model training methods
    bool trainAnomalyModel(const std::string& dataFile);
    bool trainPredictionModel(const std::string& dataFile);
    bool trainNetworkOptimizationModel(const std::string& dataFile);
    
    // Feature extraction
    static std::vector<float> extractTemporalFeatures();
    static std::vector<float> extractAccessPatternFeatures(const std::string& filePath);
    static std::vector<float> extractNetworkFeatures(const std::string& peerId);
    static std::vector<float> extractFileFeatures(const std::string& filePath);
    
    // Combined feature extraction
    static std::vector<float> extractComprehensiveFeatures(
        const std::string& filePath, 
        int peerId = 0, 
        size_t fileSize = 0,
        const std::string& operation = "read",
        const std::string& userId = "");
    
    // Get model performance metrics
    std::map<std::string, double> getModelMetrics() const;
    
    // Set model parameters
    void setAnomalyThreshold(double threshold) { anomalyThreshold = threshold; }
    void setPredictionConfidenceThreshold(double threshold) { predictionThreshold = threshold; }
    
private:
    bool modelLoaded;
    std::string modelPath;
    
    // Model parameters
    double anomalyThreshold;
    double predictionThreshold;
    
    // Performance metrics
    std::map<std::string, double> metrics;
    
    // ML model data structures (simplified for now)
    std::vector<std::vector<float>> anomalyTrainingData;
    std::vector<int> anomalyLabels;
    std::vector<std::vector<float>> predictionTrainingData;
    std::vector<float> predictionTargets;
    
    // Internal methods
    bool loadModel(const std::string& path);
    bool saveModel(const std::string& path);
    
    // Simple ML algorithms (to be replaced with more sophisticated ones in production)
    double calculateAnomalyScore(const std::vector<float>& features);
    double calculatePredictionScore(const std::vector<float>& features);
    double calculateNetworkOptimizationScore(const std::vector<float>& features);
    
    // Model training
    void trainInternalModels();
    void updateMetrics(const std::string& modelType, double accuracy);
    
    // Get current time features
    static std::vector<float> getCurrentTimeFeatures();
    
    // Load and process training data
    bool loadTrainingData(const std::string& dataFile, 
                         std::vector<std::vector<float>>& features, 
                         std::vector<int>& labels);
};