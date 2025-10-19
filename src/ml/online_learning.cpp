#include "online_learning.hpp"
#include <algorithm>
#include <random>
#include <numeric>
#include <cmath>
#include <iostream>

// OnlineLearner Implementation
OnlineLearner::OnlineLearner(const OnlineLearningConfig& config) 
    : config(config) {
}

bool OnlineLearner::processSample(const StreamingSample& sample) {
    addToBuffer(sample);
    processedSamples++;
    
    // Check if we should update the model
    if (processedSamples % config.updateFrequency == 0) {
        auto batch = getBatch();
        if (!batch.empty()) {
            return updateModel(batch);
        }
    }
    
    return true;
}

bool OnlineLearner::updateModel(const std::vector<StreamingSample>& batch) {
    // In a real implementation, this would:
    // 1. Perform online gradient descent or other online learning algorithm
    // 2. Update model parameters
    // 3. Track performance metrics
    
    // For this example, we'll just acknowledge success
    std::cout << "Updated model with batch of " << batch.size() << " samples" << std::endl;
    return true;
}

std::vector<double> OnlineLearner::predict(const std::vector<double>& features) {
    // In a real implementation, this would:
    // 1. Run inference on the current model
    // 2. Return predictions
    
    // For this example, return random predictions
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    std::vector<double> predictions(features.size() > 0 ? 1 : 1);
    predictions[0] = dist(gen);
    
    return predictions;
}

ConceptDrift OnlineLearner::detectConceptDrift() {
    ConceptDrift drift;
    
    // Simple drift detection based on performance degradation
    std::lock_guard<std::mutex> lock(bufferMutex);
    
    if (recentAccuracies.size() >= 10) {
        // Calculate recent vs older performance
        size_t halfSize = recentAccuracies.size() / 2;
        double recentAvg = 0.0;
        double olderAvg = 0.0;
        
        for (size_t i = 0; i < halfSize; ++i) {
            olderAvg += recentAccuracies[i];
            recentAvg += recentAccuracies[halfSize + i];
        }
        
        olderAvg /= halfSize;
        recentAvg /= halfSize;
        
        double performanceChange = olderAvg - recentAvg;
        
        if (performanceChange > config.driftThreshold) {
            drift.type = ConceptDrift::DriftType::GRADUAL_DRIFT;
            drift.magnitude = performanceChange;
            drift.description = "Gradual performance degradation detected";
        } else if (performanceChange < -config.driftThreshold) {
            drift.type = ConceptDrift::DriftType::RECURRING_CONCEPT;
            drift.magnitude = std::abs(performanceChange);
            drift.description = "Possible recurring concept detected";
        }
    }
    
    return drift;
}

void OnlineLearner::handleConceptDrift(const ConceptDrift& drift) {
    if (drift.type != ConceptDrift::DriftType::NO_DRIFT) {
        std::cout << "Handling concept drift: " << drift.description 
                  << " (Magnitude: " << drift.magnitude << ")" << std::endl;
        
        // Call drift callback if registered
        if (driftCallback) {
            driftCallback(drift);
        }
        
        // Adjust learning parameters based on drift type
        switch (drift.type) {
            case ConceptDrift::DriftType::SUDDEN_DRIFT:
                // Increase learning rate significantly
                config.learningRate *= 2.0;
                break;
            case ConceptDrift::DriftType::GRADUAL_DRIFT:
                // Gradually increase learning rate
                config.learningRate *= 1.2;
                break;
            case ConceptDrift::DriftType::RECURRING_CONCEPT:
                // Reset to previous known good state
                config.learningRate *= 0.8;
                break;
            default:
                break;
        }
    }
}

PerformanceMetrics OnlineLearner::getPerformanceMetrics() const {
    PerformanceMetrics metrics;
    
    std::lock_guard<std::mutex> lock(metricsMutex);
    metrics.samplesProcessed = processedSamples;
    metrics.accuracy = recentAccuracies.empty() ? 0.0 : 
                       (std::accumulate(recentAccuracies.begin(), recentAccuracies.end(), 0.0) / 
                        recentAccuracies.size());
    metrics.loss = cumulativeLoss / std::max(1.0, static_cast<double>(processedSamples));
    metrics.lastUpdate = std::chrono::system_clock::now();
    
    // Call performance callback if registered
    if (performanceCallback) {
        performanceCallback(metrics);
    }
    
    return metrics;
}

void OnlineLearner::reset() {
    std::lock_guard<std::mutex> lock(bufferMutex);
    sampleBuffer.clear();
    recentAccuracies.clear();
    processedSamples = 0;
    cumulativeLoss = 0.0;
    correctPredictions = 0.0;
    config.learningRate = 0.001;  // Reset to default
}

void OnlineLearner::addToBuffer(const StreamingSample& sample) {
    std::lock_guard<std::mutex> lock(bufferMutex);
    
    sampleBuffer.push_back(sample);
    
    // Maintain buffer size
    if (sampleBuffer.size() > config.bufferSize) {
        sampleBuffer.pop_front();
    }
}

std::vector<StreamingSample> OnlineLearner::getBatch() {
    std::lock_guard<std::mutex> lock(bufferMutex);
    
    std::vector<StreamingSample> batch;
    size_t batchSize = std::min(config.batchSize, sampleBuffer.size());
    
    // Take last batchSize samples (most recent)
    auto startIt = sampleBuffer.end() - batchSize;
    batch.insert(batch.end(), startIt, sampleBuffer.end());
    
    return batch;
}

void OnlineLearner::updatePerformanceMetrics(bool predictionCorrect, double loss) {
    {
        std::lock_guard<std::mutex> metricsLock(metricsMutex);
        cumulativeLoss += loss;
        if (predictionCorrect) {
            correctPredictions++;
        }
        processedSamples++;
    }
    
    // Update sliding window accuracy
    double currentAccuracy = predictionCorrect ? 1.0 : 0.0;
    
    {
        std::lock_guard<std::mutex> bufferLock(bufferMutex);
        recentAccuracies.push_back(currentAccuracy);
        
        if (recentAccuracies.size() > ACCURACY_WINDOW_SIZE) {
            recentAccuracies.pop_front();
        }
    }
}

double OnlineLearner::calculateAdaptiveLearningRate() {
    if (!config.adaptiveLearningRate) {
        return config.learningRate;
    }
    
    // Simple adaptive learning rate based on recent performance
    std::lock_guard<std::mutex> lock(bufferMutex);
    
    if (recentAccuracies.size() < 10) {
        return config.learningRate;
    }
    
    // Calculate recent performance trend
    double recentAvg = 0.0;
    size_t count = std::min(static_cast<size_t>(10), recentAccuracies.size());
    
    for (size_t i = recentAccuracies.size() - count; i < recentAccuracies.size(); ++i) {
        recentAvg += recentAccuracies[i];
    }
    recentAvg /= count;
    
    // Adjust learning rate based on performance
    double adjustedRate = config.learningRate;
    
    if (recentAvg < 0.7) {  // Poor performance
        adjustedRate *= 1.5;  // Increase learning rate
    } else if (recentAvg > 0.9) {  // Good performance
        adjustedRate *= 0.9;   // Decrease learning rate (more stable)
    }
    
    // Apply decay
    adjustedRate *= config.decayRate;
    
    // Ensure rate stays within reasonable bounds
    adjustedRate = std::max(0.0001, std::min(0.1, adjustedRate));
    
    return adjustedRate;
}

// OnlineAnomalyDetector Implementation
OnlineAnomalyDetector::OnlineAnomalyDetector(const OnlineLearningConfig& config)
    : OnlineLearner(config) {
    anomalyModel = std::make_unique<NeuralNetwork>();
    initializeStatisticalModels();
}

bool OnlineAnomalyDetector::processSample(const StreamingSample& sample) {
    // Update statistical models with new data
    updateStatisticalModels(sample.features);
    
    // Check if this is an anomaly
    bool isAnomalyDetected = isAnomaly(sample.features);
    
    // Update performance metrics
    updatePerformanceMetrics(isAnomalyDetected, 0.0);  // Loss not calculated in this example
    
    // Add to buffer for potential model updates
    addToBuffer(sample);
    
    processedSamples++;
    
    // Periodic model updates
    if (processedSamples % config.updateFrequency == 0) {
        auto batch = getBatch();
        if (!batch.empty()) {
            return updateModel(batch);
        }
    }
    
    return true;
}

std::vector<double> OnlineAnomalyDetector::predict(const std::vector<double>& features) {
    double anomalyScore = calculateAnomalyScore(features);
    return {anomalyScore};
}

ConceptDrift OnlineAnomalyDetector::detectConceptDrift() {
    auto baseDrift = OnlineLearner::detectConceptDrift();
    
    // Additional anomaly-specific drift detection
    std::lock_guard<std::mutex> lock(bufferMutex);
    
    if (sampleBuffer.size() >= 50) {
        // Check for sudden changes in anomaly frequency
        size_t recentAnomalies = 0;
        size_t olderAnomalies = 0;
        
        size_t halfSize = std::min(static_cast<size_t>(25), sampleBuffer.size() / 2);
        
        // Count recent anomalies
        for (size_t i = sampleBuffer.size() - halfSize; i < sampleBuffer.size(); ++i) {
            if (isAnomaly(sampleBuffer[i].features)) {
                recentAnomalies++;
            }
        }
        
        // Count older anomalies
        for (size_t i = sampleBuffer.size() - (2 * halfSize); i < sampleBuffer.size() - halfSize; ++i) {
            if (isAnomaly(sampleBuffer[i].features)) {
                olderAnomalies++;
            }
        }
        
        double recentRate = static_cast<double>(recentAnomalies) / halfSize;
        double olderRate = static_cast<double>(olderAnomalies) / halfSize;
        
        double rateChange = recentRate - olderRate;
        
        if (std::abs(rateChange) > 0.2) {  // Significant change in anomaly rate
            baseDrift.type = rateChange > 0 ? 
                ConceptDrift::DriftType::SUDDEN_DRIFT : 
                ConceptDrift::DriftType::GRADUAL_DRIFT;
            baseDrift.magnitude = std::abs(rateChange);
            baseDrift.description = "Anomaly rate change detected";
        }
    }
    
    return baseDrift;
}

double OnlineAnomalyDetector::calculateAnomalyScore(const std::vector<double>& features) {
    // Combine statistical and neural network approaches
    double statisticalScore = calculateStatisticalAnomalyScore(features);
    
    // In a real implementation, we would also get NN score
    double nnScore = 0.5;  // Placeholder
    
    // Weighted combination
    return 0.7 * statisticalScore + 0.3 * nnScore;
}

bool OnlineAnomalyDetector::isAnomaly(const std::vector<double>& features, double threshold) {
    double score = calculateAnomalyScore(features);
    return score > threshold;
}

std::vector<double> OnlineAnomalyDetector::extractAnomalyFeatures(const std::vector<float>& rawFeatures) {
    // Convert float to double
    std::vector<double> features(rawFeatures.begin(), rawFeatures.end());
    
    // In a real implementation, we would:
    // 1. Apply feature scaling/normalization
    // 2. Extract temporal features
    // 3. Apply domain-specific transformations
    
    return features;
}

void OnlineAnomalyDetector::adaptToNewPatterns(const std::vector<std::vector<double>>& normalPatterns) {
    // Update statistical models with new normal patterns
    for (const auto& pattern : normalPatterns) {
        updateStatisticalModels(pattern);
    }
    
    // Adjust anomaly threshold based on new patterns
    std::vector<double> scores;
    for (const auto& pattern : normalPatterns) {
        scores.push_back(calculateStatisticalAnomalyScore(pattern));
    }
    
    if (!scores.empty()) {
        // Set threshold to 95th percentile of normal scores
        std::sort(scores.begin(), scores.end());
        size_t index = static_cast<size_t>(scores.size() * 0.95);
        if (index < scores.size()) {
            anomalyThreshold = scores[index];
        }
    }
}

PerformanceMetrics OnlineAnomalyDetector::getAnomalyMetrics() const {
    return getPerformanceMetrics();
}

void OnlineAnomalyDetector::initializeStatisticalModels() {
    // Initialize with default values
    featureMeans.clear();
    featureVariances.clear();
    featureMinValues.clear();
    featureMaxValues.clear();
    
    // Set reasonable defaults for demonstration
    size_t defaultFeatureCount = 10;
    featureMeans.assign(defaultFeatureCount, 0.0);
    featureVariances.assign(defaultFeatureCount, 1.0);
    featureMinValues.assign(defaultFeatureCount, -10.0);
    featureMaxValues.assign(defaultFeatureCount, 10.0);
}

void OnlineAnomalyDetector::updateStatisticalModels(const std::vector<double>& features) {
    if (featureMeans.empty()) {
        // Initialize models with first sample
        featureMeans = features;
        featureVariances.assign(features.size(), 1.0);
        featureMinValues = features;
        featureMaxValues = features;
        return;
    }
    
    // Ensure all vectors have the same size
    if (features.size() != featureMeans.size()) {
        // Resize if needed (in a real system, you might want to handle this differently)
        size_t newSize = std::max(features.size(), featureMeans.size());
        featureMeans.resize(newSize, 0.0);
        featureVariances.resize(newSize, 1.0);
        featureMinValues.resize(newSize, 0.0);
        featureMaxValues.resize(newSize, 0.0);
    }
    
    // Simple exponential moving average for online updates
    double alpha = 0.1;  // Learning rate for statistical updates
    
    for (size_t i = 0; i < std::min(features.size(), featureMeans.size()); ++i) {
        // Update mean
        featureMeans[i] = alpha * features[i] + (1.0 - alpha) * featureMeans[i];
        
        // Update min/max
        featureMinValues[i] = std::min(featureMinValues[i], features[i]);
        featureMaxValues[i] = std::max(featureMaxValues[i], features[i]);
        
        // Update variance (simplified)
        double diff = features[i] - featureMeans[i];
        featureVariances[i] = alpha * diff * diff + (1.0 - alpha) * featureVariances[i];
    }
}

double OnlineAnomalyDetector::calculateStatisticalAnomalyScore(const std::vector<double>& features) {
    if (featureMeans.empty() || features.empty()) {
        return 0.5;  // Neutral score if models not initialized
    }
    
    double totalScore = 0.0;
    size_t validFeatures = 0;
    
    for (size_t i = 0; i < std::min(features.size(), featureMeans.size()); ++i) {
        // Z-score calculation
        double variance = std::max(0.0001, featureVariances[i]);  // Avoid division by zero
        double zScore = std::abs(features[i] - featureMeans[i]) / std::sqrt(variance);
        
        // Normalize z-score (assuming most data is within 3 standard deviations)
        double normalizedScore = std::min(1.0, zScore / 3.0);
        
        // Range-based anomaly detection
        double range = featureMaxValues[i] - featureMinValues[i];
        if (range > 0.0001) {  // Avoid division by zero
            double normalizedValue = (features[i] - featureMinValues[i]) / range;
            double rangeScore = 0.0;
            
            // Values near boundaries are more anomalous
            if (normalizedValue < 0.1 || normalizedValue > 0.9) {
                rangeScore = 1.0;
            } else if (normalizedValue < 0.2 || normalizedValue > 0.8) {
                rangeScore = 0.5;
            }
            
            // Combine z-score and range-based scores
            totalScore += 0.7 * normalizedScore + 0.3 * rangeScore;
        } else {
            totalScore += normalizedScore;
        }
        
        validFeatures++;
    }
    
    return validFeatures > 0 ? totalScore / validFeatures : 0.5;
}

void OnlineAnomalyDetector::adjustThreshold(double newScore, bool isActualAnomaly) {
    // Simple threshold adjustment based on feedback
    if (isActualAnomaly && newScore < anomalyThreshold) {
        // Missed anomaly - decrease threshold
        std::lock_guard<std::mutex> lock(thresholdMutex);
        anomalyThreshold *= 0.95;
    } else if (!isActualAnomaly && newScore > anomalyThreshold) {
        // False positive - increase threshold
        std::lock_guard<std::mutex> lock(thresholdMutex);
        anomalyThreshold *= 1.05;
    }
    
    // Keep threshold within reasonable bounds
    std::lock_guard<std::mutex> lock(thresholdMutex);
    anomalyThreshold = std::max(0.1, std::min(0.99, anomalyThreshold));
}

// OnlinePredictionModel Implementation
OnlinePredictionModel::OnlinePredictionModel(const OnlineLearningConfig& config)
    : OnlineLearner(config) {
    predictionModel = std::make_unique<NeuralNetwork>();
    sequenceModel = std::make_unique<LstmNetwork>(10, 50, 1);  // Example: 10 input, 50 hidden, 1 output
}

bool OnlinePredictionModel::processSample(const StreamingSample& sample) {
    // Add to pattern database for similarity search
    updatePatternDatabase(sample.features, sample.labels);
    
    // Update user profiles if sample has user context
    if (!sample.sourceId.empty()) {
        updateUserProfile(sample.sourceId, sample.features);
    }
    
    // Add to buffer for model updates
    addToBuffer(sample);
    processedSamples++;
    
    // Periodic model updates
    if (processedSamples % config.updateFrequency == 0) {
        auto batch = getBatch();
        if (!batch.empty()) {
            return updateModel(batch);
        }
    }
    
    return true;
}

std::vector<double> OnlinePredictionModel::predict(const std::vector<double>& features) {
    // Use neural network for prediction
    auto nnPrediction = predictionModel->predict(features);
    
    // Incorporate pattern similarity
    auto similarPatterns = findSimilarPatterns(features);
    if (!similarPatterns.empty()) {
        // Weighted average of similar patterns
        double similaritySum = 0.0;
        std::vector<double> similarityWeightedPrediction(nnPrediction.size(), 0.0);
        
        for (const auto& pattern : similarPatterns) {
            similaritySum += pattern.second;
            for (size_t i = 0; i < std::min(nnPrediction.size(), pattern.first.size()); ++i) {
                similarityWeightedPrediction[i] += pattern.first[i] * pattern.second;
            }
        }
        
        if (similaritySum > 0.0) {
            for (auto& val : similarityWeightedPrediction) {
                val /= similaritySum;
            }
            
            // Combine NN and similarity-based predictions
            for (size_t i = 0; i < nnPrediction.size(); ++i) {
                nnPrediction[i] = 0.7 * nnPrediction[i] + 0.3 * similarityWeightedPrediction[i];
            }
        }
    }
    
    return nnPrediction;
}

ConceptDrift OnlinePredictionModel::detectConceptDrift() {
    auto baseDrift = OnlineLearner::detectConceptDrift();
    
    // Additional prediction-specific drift detection
    std::lock_guard<std::mutex> lock(patternMutex);
    
    if (patternDatabase.size() >= 20) {
        // Check for changes in pattern distribution
        // This is a simplified check - in reality, you'd use statistical tests
        
        baseDrift.type = ConceptDrift::DriftType::NO_DRIFT;  // Assume no drift for now
    }
    
    return baseDrift;
}

std::vector<double> OnlinePredictionModel::predictFileAccess(const std::vector<double>& userFeatures) {
    // Predict file access probability
    return predict(userFeatures);
}

std::vector<std::string> OnlinePredictionModel::recommendFiles(const std::vector<double>& userContext, 
                                                              size_t topK) {
    // In a real implementation, this would:
    // 1. Use user context and profile
    // 2. Find similar users/files
    // 3. Rank recommendations
    // 4. Return top-K recommendations
    
    std::vector<std::string> recommendations;
    
    // For demonstration, return mock recommendations
    for (size_t i = 0; i < topK; ++i) {
        recommendations.push_back("recommended_file_" + std::to_string(i) + ".txt");
    }
    
    return recommendations;
}

std::vector<double> OnlinePredictionModel::predictSequence(const std::vector<std::vector<double>>& sequence) {
    // Use LSTM for sequence prediction
    return sequenceModel->predictNext(sequence);
}

std::vector<double> OnlinePredictionModel::extractPredictionFeatures(const std::vector<float>& rawFeatures,
                                                                   const std::string& context) {
    // Convert and engineer features for prediction
    std::vector<double> features(rawFeatures.begin(), rawFeatures.end());
    
    // Add context-based features
    if (!context.empty()) {
        // Extract temporal features from context
        // This is a simplified example
        std::hash<std::string> hasher;
        size_t contextHash = hasher(context);
        features.push_back(static_cast<double>(contextHash % 1000) / 1000.0);  // Normalized context feature
    }
    
    return features;
}

std::vector<std::pair<std::vector<double>, double>> OnlinePredictionModel::findSimilarPatterns(
    const std::vector<double>& queryPattern) {
    
    std::lock_guard<std::mutex> lock(patternMutex);
    
    std::vector<std::pair<std::vector<double>, double>> similarities;
    
    for (const auto& patternPair : patternDatabase) {
        double similarity = calculatePatternSimilarity(queryPattern, patternPair.first);
        if (similarity > 0.7) {  // Only return highly similar patterns
            similarities.push_back({patternPair.second, similarity});
        }
    }
    
    // Sort by similarity (descending)
    std::sort(similarities.begin(), similarities.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return similarities;
}

PerformanceMetrics OnlinePredictionModel::getPredictionMetrics() const {
    return getPerformanceMetrics();
}

void OnlinePredictionModel::updatePatternDatabase(const std::vector<double>& features,
                                                 const std::vector<double>& predictions) {
    std::lock_guard<std::mutex> lock(patternMutex);
    
    patternDatabase.push_back({features, predictions});
    
    // Maintain database size
    if (patternDatabase.size() > 1000) {  // Max 1000 patterns
        patternDatabase.erase(patternDatabase.begin());
    }
}

double OnlinePredictionModel::calculatePatternSimilarity(const std::vector<double>& pattern1,
                                                        const std::vector<double>& pattern2) {
    if (pattern1.empty() || pattern2.empty()) {
        return 0.0;
    }
    
    size_t minSize = std::min(pattern1.size(), pattern2.size());
    
    // Cosine similarity
    double dotProduct = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;
    
    for (size_t i = 0; i < minSize; ++i) {
        dotProduct += pattern1[i] * pattern2[i];
        norm1 += pattern1[i] * pattern1[i];
        norm2 += pattern2[i] * pattern2[i];
    }
    
    if (norm1 == 0.0 || norm2 == 0.0) {
        return 0.0;
    }
    
    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2));
}

void OnlinePredictionModel::updateUserProfile(const std::string& userId, 
                                             const std::vector<double>& features) {
    std::lock_guard<std::mutex> lock(patternMutex);
    
    // Simple exponential moving average for user profile
    double alpha = 0.1;
    
    if (userProfiles.find(userId) == userProfiles.end()) {
        // Initialize user profile
        userProfiles[userId] = features;
    } else {
        // Update existing profile
        auto& profile = userProfiles[userId];
        profile.resize(std::max(profile.size(), features.size()), 0.0);
        
        for (size_t i = 0; i < std::min(features.size(), profile.size()); ++i) {
            profile[i] = alpha * features[i] + (1.0 - alpha) * profile[i];
        }
    }
    
    lastAccessTimes[userId] = std::chrono::system_clock::now();
}