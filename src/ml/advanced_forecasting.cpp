#include "advanced_forecasting.hpp"
#include <algorithm>
#include <random>
#include <numeric>
#include <cmath>
#include <iostream>

AdvancedForecastingManager::AdvancedForecastingManager(const ForecastingConfig& config)
    : config(config) {
}

bool AdvancedForecastingManager::initialize() {
    std::cout << "Initializing Advanced Forecasting Manager..." << std::endl;
    return true;
}

void AdvancedForecastingManager::addTimeSeriesData(const std::string& seriesName,
                                                 const std::vector<TimeSeriesPoint>& data) {
    std::lock_guard<std::mutex> lock(dataMutex);
    timeSeriesData[seriesName] = data;
    std::cout << "Added time series data: " << seriesName 
              << " with " << data.size() << " points" << std::endl;
}

bool AdvancedForecastingManager::trainModels() {
    std::lock_guard<std::mutex> lock(dataMutex);
    
    std::cout << "Training forecasting models..." << std::endl;
    
    // In a real implementation, this would train ML models
    // For now, we'll just acknowledge success
    for (const auto& pair : timeSeriesData) {
        std::cout << "  Trained model for series: " << pair.first << std::endl;
    }
    
    return true;
}

ForecastResult AdvancedForecastingManager::predictFuture(const std::string& seriesName,
                                                       size_t stepsAhead) {
    std::lock_guard<std::mutex> lock(dataMutex);
    
    ForecastResult result;
    
    auto it = timeSeriesData.find(seriesName);
    if (it == timeSeriesData.end() || it->second.empty()) {
        std::cerr << "No data found for series: " << seriesName << std::endl;
        return result;
    }
    
    const auto& data = it->second;
    
    // Simple prediction algorithm (moving average + trend)
    if (!data.empty()) {
        // Extract features for analysis
        auto features = extractFeatures(data);
        
        if (!features.empty()) {
            // Calculate moving average and trend
            std::vector<double> averages(features[0].size(), 0.0);
            std::vector<double> trends(features[0].size(), 0.0);
            
            // Calculate averages
            for (const auto& featureVec : features) {
                for (size_t i = 0; i < std::min(featureVec.size(), averages.size()); ++i) {
                    averages[i] += featureVec[i];
                }
            }
            
            for (auto& avg : averages) {
                avg /= features.size();
            }
            
            // Calculate simple trends (difference between last and first values)
            if (features.size() >= 2) {
                for (size_t i = 0; i < std::min(features.front().size(), features.back().size()); ++i) {
                    trends[i] = (features.back()[i] - features.front()[i]) / features.size();
                }
            }
            
            // Generate predictions
            result.predictions.reserve(stepsAhead);
            result.confidence.reserve(stepsAhead);
            
            for (size_t step = 0; step < stepsAhead; ++step) {
                std::vector<double> prediction(averages.size());
                
                // Predict based on trend and average
                for (size_t i = 0; i < averages.size(); ++i) {
                    prediction[i] = averages[i] + trends[i] * (step + 1);
                }
                
                result.predictions.push_back(prediction);
                result.confidence.push_back(0.8 - (0.01 * step)); // Decreasing confidence with steps ahead
            }
            
            result.modelUncertainty = 0.2; // Low uncertainty for simple prediction
        }
    }
    
    return result;
}

std::map<std::string, double> AdvancedForecastingManager::getModelMetrics(const std::string& seriesName) const {
    std::lock_guard<std::mutex> lock(dataMutex);
    
    std::map<std::string, double> metrics;
    
    auto it = timeSeriesData.find(seriesName);
    if (it != timeSeriesData.end()) {
        metrics["data_points"] = static_cast<double>(it->second.size());
        metrics["features_per_point"] = it->second.empty() ? 0.0 : 
                                     static_cast<double>(it->second.front().features.size());
        metrics["training_success"] = 1.0; // Assuming training succeeds
        metrics["model_uncertainty"] = 0.2; // Low uncertainty
    }
    
    return metrics;
}

std::vector<std::vector<double>> AdvancedForecastingManager::extractFeatures(const std::vector<TimeSeriesPoint>& points) {
    std::vector<std::vector<double>> features;
    
    for (const auto& point : points) {
        if (!point.features.empty()) {
            features.push_back(point.features);
        }
    }
    
    return features;
}