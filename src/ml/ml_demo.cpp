#include <iostream>
#include <vector>
#include <string>
#include "ml/ml_analyzer.hpp"
#include "ml/ml_data_generator.hpp"

int main() {
    std::cout << "=== SentinelFS-Neo ML Layer Enhancement Demo ===" << std::endl;
    
    // Initialize ML analyzer
    MLAnalyzer mlAnalyzer;
    if (!mlAnalyzer.initialize()) {
        std::cerr << "Failed to initialize ML analyzer" << std::endl;
        return 1;
    }
    
    std::cout << "ML Analyzer initialized successfully!" << std::endl;
    
    // Test anomaly detection
    std::cout << "\n--- Testing Anomaly Detection ---" << std::endl;
    
    // Generate normal and anomalous samples
    auto normalFeatures = MLDataGenerator::generateSampleFeatures(false);
    auto anomalyFeatures = MLDataGenerator::generateSampleFeatures(true);
    
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
              << ", Confidence: " << normalResult.confidence << std::endl;
              
    std::cout << "Anomaly sample detection - Is Anomaly: " << (anomalyResult.isAnomaly ? "YES" : "NO") 
              << ", Confidence: " << anomalyResult.confidence << std::endl;
    
    // Test predictive sync
    std::cout << "\n--- Testing Predictive Sync ---" << std::endl;
    auto predictions = mlAnalyzer.predictFileAccess("test_user");
    std::cout << "Generated " << predictions.size() << " predictions" << std::endl;
    
    // Test network optimization prediction
    std::cout << "\n--- Testing Network Optimization ---" << std::endl;
    auto networkFeatures = MLDataGenerator::generateSampleNetworkFeatures();
    double optimizationGain = mlAnalyzer.predictNetworkOptimizationGain("peer1", networkFeatures);
    std::cout << "Predicted network optimization gain: " << optimizationGain << std::endl;
    
    // Demonstrate feedback loop
    std::cout << "\n--- Demonstrating Feedback Loop ---" << std::endl;
    
    // Provide feedback for correct anomaly detection
    mlAnalyzer.provideFeedback(anomalyFeatures, true, true);
    std::cout << "Provided positive feedback for anomaly detection" << std::endl;
    
    // Provide feedback for correct normal detection
    mlAnalyzer.provideFeedback(normalFeatures, false, true);
    std::cout << "Provided positive feedback for normal detection" << std::endl;
    
    // Show model metrics
    auto metrics = mlAnalyzer.getModelMetrics();
    std::cout << "Current model metrics:" << std::endl;
    for (const auto& metric : metrics) {
        std::cout << "  " << metric.first << ": " << metric.second << std::endl;
    }
    
    // Generate training data files
    std::cout << "\n--- Generating Training Data Files ---" << std::endl;
    
    if (MLDataGenerator::generateAnomalyTrainingData("anomaly_training.csv", 500)) {
        std::cout << "Generated anomaly training data: anomaly_training.csv" << std::endl;
    } else {
        std::cout << "Failed to generate anomaly training data" << std::endl;
    }
    
    if (MLDataGenerator::generatePredictionTrainingData("prediction_training.csv", 500)) {
        std::cout << "Generated prediction training data: prediction_training.csv" << std::endl;
    } else {
        std::cout << "Failed to generate prediction training data" << std::endl;
    }
    
    if (MLDataGenerator::generateNetworkOptimizationData("network_training.csv", 500)) {
        std::cout << "Generated network optimization data: network_training.csv" << std::endl;
    } else {
        std::cout << "Failed to generate network optimization data" << std::endl;
    }
    
    std::cout << "\nDemo completed successfully!" << std::endl;
    return 0;
}