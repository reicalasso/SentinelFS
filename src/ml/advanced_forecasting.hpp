#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include "../models.hpp"
#include "../fs/delta_engine.hpp"

// Simple forecasting configuration
struct ForecastingConfig {
    size_t sequenceLength = 50;
    size_t predictionHorizon = 10;
    double learningRate = 0.001;
    size_t hiddenUnits = 128;
    size_t batchSize = 32;
    size_t epochs = 50;
    
    ForecastingConfig() = default;
};

// Simplified forecast result
struct ForecastResult {
    std::vector<std::vector<double>> predictions;
    std::vector<double> confidence;
    double modelUncertainty = 0.0;
};

// Simple time series point
struct TimeSeriesPoint {
    std::chrono::system_clock::time_point timestamp;
    std::vector<double> features;
    std::vector<double> targets;
    
    TimeSeriesPoint() = default;
    TimeSeriesPoint(const std::vector<double>& feats) : features(feats) {
        timestamp = std::chrono::system_clock::now();
    }
};

// Simplified forecasting manager
class AdvancedForecastingManager {
public:
    AdvancedForecastingManager(const ForecastingConfig& config = ForecastingConfig());
    ~AdvancedForecastingManager() = default;
    
    // Initialize forecasting system
    bool initialize();
    
    // Add time series data
    void addTimeSeriesData(const std::string& seriesName,
                          const std::vector<TimeSeriesPoint>& data);
    
    // Train forecasting models
    bool trainModels();
    
    // Predict future values
    ForecastResult predictFuture(const std::string& seriesName,
                               size_t stepsAhead = 10);
    
    // Get model metrics
    std::map<std::string, double> getModelMetrics(const std::string& seriesName) const;
    
private:
    ForecastingConfig config;
    std::map<std::string, std::vector<TimeSeriesPoint>> timeSeriesData;
    mutable std::mutex dataMutex;
    
    // Internal methods
    std::vector<std::vector<double>> extractFeatures(const std::vector<TimeSeriesPoint>& points);
};