#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>

class MLDataGenerator {
public:
    // Generate synthetic training data for anomaly detection
    static bool generateAnomalyTrainingData(const std::string& outputPath, size_t sampleCount = 1000);
    
    // Generate synthetic training data for predictive sync
    static bool generatePredictionTrainingData(const std::string& outputPath, size_t sampleCount = 1000);
    
    // Generate synthetic training data for network optimization
    static bool generateNetworkOptimizationData(const std::string& outputPath, size_t sampleCount = 1000);
    
    // Generate sample features for testing
    static std::vector<float> generateSampleFeatures(bool isAnomaly = false);
    
    // Generate sample network features
    static std::vector<float> generateSampleNetworkFeatures();
    
    // Generate sample file access patterns
    static std::vector<float> generateSampleAccessPatternFeatures();
    
private:
    static std::mt19937 rng;
};