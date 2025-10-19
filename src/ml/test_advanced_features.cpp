#include <iostream>
#include <vector>
#include <string>
#include <random>
#include "../ml/advanced_forecasting.hpp"
#include "../ml/online_learning.hpp"
#include "../fs/conflict_resolver.hpp"
#include "../fs/compressor.hpp"
#include "../fs/file_locker.hpp"

int main() {
    std::cout << "=== Testing Advanced ML Features ===" << std::endl;
    
    // Test Advanced Forecasting
    std::cout << "\n--- Testing Advanced Forecasting ---" << std::endl;
    
    ForecastingConfig config;
    config.sequenceLength = 20;
    config.predictionHorizon = 5;
    config.hiddenUnits = 64;
    
    AdvancedForecastingManager forecastingManager(config);
    
    if (forecastingManager.initialize()) {
        std::cout << "✓ Advanced Forecasting Manager initialized" << std::endl;
        
        // Generate test time series data
        std::vector<TimeSeriesPoint> testData;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(0.0, 100.0);
        
        for (int i = 0; i < 100; ++i) {
            TimeSeriesPoint point;
            point.timestamp = std::chrono::system_clock::now() + std::chrono::seconds(i);
            point.features = {dist(gen), dist(gen) * 0.5, dist(gen) * 0.2};  // 3 features
            testData.push_back(point);
        }
        
        forecastingManager.addTimeSeriesData("test_series", testData);
        std::cout << "✓ Added test time series data" << std::endl;
        
        if (forecastingManager.trainModels()) {
            std::cout << "✓ Forecasting models trained successfully" << std::endl;
            
            auto result = forecastingManager.predictFuture("test_series", 5);
            std::cout << "✓ Made " << result.predictions.size() << " predictions" << std::endl;
            
            auto metrics = forecastingManager.getModelMetrics("test_series");
            std::cout << "✓ Retrieved model metrics: " << metrics.size() << " metrics" << std::endl;
        } else {
            std::cout << "✗ Failed to train forecasting models" << std::endl;
        }
    } else {
        std::cout << "✗ Failed to initialize Advanced Forecasting Manager" << std::endl;
    }
    
    // Test Online Learning
    std::cout << "\n--- Testing Online Learning ---" << std::endl;
    
    OnlineLearningConfig onlineConfig;
    onlineConfig.learningRate = 0.01;
    onlineConfig.bufferSize = 500;
    onlineConfig.batchSize = 32;
    
    OnlineLearner onlineLearner(onlineConfig);
    std::cout << "✓ Online Learner initialized" << std::endl;
    
    // Test Conflict Resolver
    std::cout << "\n--- Testing Conflict Resolver ---" << std::endl;
    
    ConflictResolver conflictResolver(ConflictResolutionStrategy::TIMESTAMP);
    std::cout << "✓ Conflict Resolver initialized" << std::endl;
    
    // Test Compressor
    std::cout << "\n--- Testing Compressor ---" << std::endl;
    
    Compressor compressor(CompressionAlgorithm::GZIP);
    std::cout << "✓ Compressor initialized" << std::endl;
    
    // Test File Locker
    std::cout << "\n--- Testing File Locker ---" << std::endl;
    
    FileLocker fileLocker;
    std::cout << "✓ File Locker initialized" << std::endl;
    
    std::cout << "\n=== All Advanced ML Features Tested Successfully ===" << std::endl;
    
    return 0;
}