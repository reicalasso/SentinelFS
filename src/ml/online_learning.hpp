#pragma once

#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include "neural_network.hpp"
#include "federated_learning.hpp"
#include "../models.hpp"

// Online Learning Configuration
struct OnlineLearningConfig {
    double learningRate = 0.001;
    size_t bufferSize = 1000;     // Number of samples to keep in buffer
    size_t batchSize = 32;        // Batch size for online updates
    double decayRate = 0.95;     // Learning rate decay
    bool adaptiveLearningRate = true;
    size_t updateFrequency = 10; // Update model every N samples
    bool enableDriftDetection = true;
    double driftThreshold = 0.1; // Threshold for concept drift detection
    
    OnlineLearningConfig() = default;
};

// Concept Drift Detection
struct ConceptDrift {
    enum class DriftType {
        NO_DRIFT,
        GRADUAL_DRIFT,
        SUDDEN_DRIFT,
        RECURRING_CONCEPT
    };
    
    DriftType type;
    double magnitude;
    std::chrono::system_clock::time_point timestamp;
    std::string description;
    
    ConceptDrift() : type(DriftType::NO_DRIFT), magnitude(0.0), 
                     timestamp(std::chrono::system_clock::now()) {}
};

// Note: StreamingSample is now defined in models.hpp

// Model Performance Metrics
struct PerformanceMetrics {
    double accuracy;
    double precision;
    double recall;
    double f1Score;
    double loss;
    size_t samplesProcessed;
    std::chrono::system_clock::time_point lastUpdate;
    
    PerformanceMetrics() : accuracy(0.0), precision(0.0), recall(0.0), 
                          f1Score(0.0), loss(0.0), samplesProcessed(0) {
        lastUpdate = std::chrono::system_clock::now();
    }
};

// Online Learning Base Class
class OnlineLearner {
public:
    OnlineLearner(const OnlineLearningConfig& config = OnlineLearningConfig());
    virtual ~OnlineLearner() = default;
    
    // Process streaming sample
    virtual bool processSample(const StreamingSample& sample);
    
    // Update model with batch of samples
    virtual bool updateModel(const std::vector<StreamingSample>& batch);
    
    // Predict on new sample
    virtual std::vector<double> predict(const std::vector<double>& features);
    
    // Detect concept drift
    virtual ConceptDrift detectConceptDrift();
    
    // Handle concept drift
    virtual void handleConceptDrift(const ConceptDrift& drift);
    
    // Get performance metrics
    virtual PerformanceMetrics getPerformanceMetrics() const;
    
    // Reset model
    virtual void reset();
    
    // Get/Set configuration
    void setConfig(const OnlineLearningConfig& config) { this->config = config; }
    OnlineLearningConfig getConfig() const { return config; }
    
    // Callback registration
    using DriftCallback = std::function<void(const ConceptDrift&)>;
    using PerformanceCallback = std::function<void(const PerformanceMetrics&)>;
    
    void setDriftCallback(DriftCallback callback) { driftCallback = callback; }
    void setPerformanceCallback(PerformanceCallback callback) { performanceCallback = callback; }
    
protected:
    OnlineLearningConfig config;
    std::deque<StreamingSample> sampleBuffer;
    mutable std::mutex bufferMutex;
    
    size_t processedSamples{0};
    double cumulativeLoss{0.0};
    double correctPredictions{0.0};
    mutable std::mutex metricsMutex;  // Protect metrics with mutex
    
    // Performance tracking
    std::deque<double> recentAccuracies;  // Sliding window of accuracies
    static constexpr size_t ACCURACY_WINDOW_SIZE = 50;
    
    // Callbacks
    DriftCallback driftCallback;
    PerformanceCallback performanceCallback;
    
    // Internal methods
    virtual void addToBuffer(const StreamingSample& sample);
    virtual std::vector<StreamingSample> getBatch();
    virtual void updatePerformanceMetrics(bool predictionCorrect, double loss);
    virtual double calculateAdaptiveLearningRate();
};

// Online Anomaly Detector
class OnlineAnomalyDetector : public OnlineLearner {
public:
    OnlineAnomalyDetector(const OnlineLearningConfig& config = OnlineLearningConfig());
    ~OnlineAnomalyDetector() override = default;
    
    // Override base methods with anomaly-specific implementations
    bool processSample(const StreamingSample& sample) override;
    std::vector<double> predict(const std::vector<double>& features) override;
    ConceptDrift detectConceptDrift() override;
    
    // Anomaly-specific methods
    double calculateAnomalyScore(const std::vector<double>& features);
    bool isAnomaly(const std::vector<double>& features, double threshold = 0.5);
    
    // Feature engineering for anomaly detection
    std::vector<double> extractAnomalyFeatures(const std::vector<float>& rawFeatures);
    
    // Model adaptation
    void adaptToNewPatterns(const std::vector<std::vector<double>>& normalPatterns);
    
    // Get anomaly detector metrics
    PerformanceMetrics getAnomalyMetrics() const;
    
private:
    std::unique_ptr<NeuralNetwork> anomalyModel;
    
    // Statistical models for baseline comparison
    std::vector<double> featureMeans;
    std::vector<double> featureVariances;
    std::vector<double> featureMinValues;
    std::vector<double> featureMaxValues;
    
    // Adaptive threshold
    double anomalyThreshold{0.7};
    mutable std::mutex thresholdMutex;  // Protect threshold with mutex
    
    // Internal methods
    void initializeStatisticalModels();
    void updateStatisticalModels(const std::vector<double>& features);
    double calculateStatisticalAnomalyScore(const std::vector<double>& features);
    void adjustThreshold(double newScore, bool isActualAnomaly);
};

// Online Prediction Model
class OnlinePredictionModel : public OnlineLearner {
public:
    OnlinePredictionModel(const OnlineLearningConfig& config = OnlineLearningConfig());
    ~OnlinePredictionModel() override = default;
    
    // Override base methods with prediction-specific implementations
    bool processSample(const StreamingSample& sample) override;
    std::vector<double> predict(const std::vector<double>& features) override;
    ConceptDrift detectConceptDrift() override;
    
    // Prediction-specific methods
    std::vector<double> predictFileAccess(const std::vector<double>& userFeatures);
    std::vector<std::string> recommendFiles(const std::vector<double>& userContext, size_t topK = 5);
    
    // Sequence prediction (using LSTM)
    std::vector<double> predictSequence(const std::vector<std::vector<double>>& sequence);
    
    // Feature engineering for prediction
    std::vector<double> extractPredictionFeatures(const std::vector<float>& rawFeatures,
                                                 const std::string& context = "");
    
    // Pattern recognition
    std::vector<std::pair<std::vector<double>, double>> findSimilarPatterns(
        const std::vector<double>& queryPattern);
    
    // Get prediction model metrics
    PerformanceMetrics getPredictionMetrics() const;
    
private:
    std::unique_ptr<NeuralNetwork> predictionModel;
    std::unique_ptr<LstmNetwork> sequenceModel;
    
    // Pattern database for similarity search
    std::vector<std::pair<std::vector<double>, std::vector<double>>> patternDatabase;
    mutable std::mutex patternMutex;
    
    // User behavior models
    std::map<std::string, std::vector<double>> userProfiles;
    std::map<std::string, std::chrono::system_clock::time_point> lastAccessTimes;
    
    // Internal methods
    void updatePatternDatabase(const std::vector<double>& features, 
                              const std::vector<double>& predictions);
    double calculatePatternSimilarity(const std::vector<double>& pattern1,
                                     const std::vector<double>& pattern2);
    void updateUserProfile(const std::string& userId, const std::vector<double>& features);
};

// Adaptive Ensemble Learner
class AdaptiveEnsembleLearner {
public:
    AdaptiveEnsembleLearner(const OnlineLearningConfig& config = OnlineLearningConfig());
    ~AdaptiveEnsembleLearner() = default;
    
    // Add learner to ensemble
    void addLearner(std::unique_ptr<OnlineLearner> learner, const std::string& name);
    
    // Remove learner from ensemble
    void removeLearner(const std::string& name);
    
    // Process sample through ensemble
    bool processSample(const StreamingSample& sample);
    
    // Ensemble prediction (weighted average)
    std::vector<double> predict(const std::vector<double>& features);
    
    // Update ensemble weights based on performance
    void updateEnsembleWeights();
    
    // Get ensemble performance
    PerformanceMetrics getEnsemblePerformance() const;
    
    // Dynamic model selection
    std::vector<std::string> selectBestModels(const std::vector<double>& features);
    
    // Model pruning
    void prunePoorPerformers();
    
private:
    std::map<std::string, std::unique_ptr<OnlineLearner>> learners;
    std::map<std::string, double> learnerWeights;  // Performance-based weights
    std::map<std::string, PerformanceMetrics> recentMetrics;
    mutable std::mutex ensembleMutex;
    
    OnlineLearningConfig config;
    
    // Internal methods
    void calculateLearnerWeights();
    double calculateLearnerPerformance(const std::string& learnerName);
    std::vector<double> weightedAveragePredictions(const std::vector<double>& features);
};

// Real-time Adaptation Manager
class RealTimeAdaptationManager {
public:
    RealTimeAdaptationManager(const OnlineLearningConfig& config = OnlineLearningConfig());
    ~RealTimeAdaptationManager() = default;
    
    // Initialize adaptation system
    bool initialize();
    
    // Process incoming data stream
    bool processDataStream(const std::vector<StreamingSample>& samples);
    
    // Adaptive learning loop
    void startAdaptiveLearning();
    void stopAdaptiveLearning();
    
    // Concept drift handling
    void handleConceptDrift(const ConceptDrift& drift);
    
    // Model evolution
    void evolveModelStructure();
    
    // Performance monitoring
    void startPerformanceMonitoring();
    void stopPerformanceMonitoring();
    
    // Get current system status
    std::map<std::string, double> getSystemStatus() const;
    
    // Export/import model state
    bool exportModelState(const std::string& filepath);
    bool importModelState(const std::string& filepath);
    
    // Getters for components
    OnlineAnomalyDetector& getAnomalyDetector() { return *anomalyDetector; }
    OnlinePredictionModel& getPredictionModel() { return *predictionModel; }
    AdaptiveEnsembleLearner& getEnsembleLearner() { return *ensembleLearner; }
    
private:
    std::unique_ptr<OnlineAnomalyDetector> anomalyDetector;
    std::unique_ptr<OnlinePredictionModel> predictionModel;
    std::unique_ptr<AdaptiveEnsembleLearner> ensembleLearner;
    
    OnlineLearningConfig config;
    
    std::atomic<bool> adaptationRunning{false};
    std::atomic<bool> monitoringRunning{false};
    std::thread adaptationThread;
    std::thread monitoringThread;
    
    // Performance tracking
    std::deque<PerformanceMetrics> performanceHistory;
    mutable std::mutex performanceMutex;
    
    // Internal methods
    void adaptationLoop();
    void monitoringLoop();
    void updateSystemPerformance();
    void adjustSystemParameters();
    
    // Thread-safe operations
    void safeAddSample(const StreamingSample& sample);
    std::vector<double> safePredict(const std::vector<double>& features);
};