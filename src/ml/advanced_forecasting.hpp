#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <chrono>
#include <deque>
#include "neural_network.hpp"
#include "online_learning.hpp"
#include "../models.hpp"

// Advanced Forecasting Configuration
struct ForecastingConfig {
    size_t sequenceLength = 50;      // Number of time steps to consider
    size_t predictionHorizon = 10;   // Number of steps to predict ahead
    double learningRate = 0.001;
    size_t hiddenUnits = 128;        // LSTM hidden units
    size_t batchSize = 32;
    size_t epochs = 50;
    bool useAttention = true;        // Use attention mechanism
    bool useDropout = true;
    double dropoutRate = 0.2;
    std::string optimizer = "adam";   // adam, sgd, rmsprop
    bool earlyStopping = true;
    size_t patience = 10;            // Epochs to wait before early stopping
    
    ForecastingConfig() = default;
};

// Time Series Data Point
struct TimeSeriesPoint {
    std::chrono::system_clock::time_point timestamp;
    std::vector<double> features;    // Input features
    std::vector<double> targets;     // Target values to predict
    std::string context;              // Optional context information
    
    TimeSeriesPoint() = default;
    
    TimeSeriesPoint(const std::chrono::system_clock::time_point& ts,
                   const std::vector<double>& feats,
                   const std::vector<double>& tgts = {},
                   const std::string& ctx = "")
        : timestamp(ts), features(feats), targets(tgts), context(ctx) {}
};

// Forecast Result
struct ForecastResult {
    std::vector<std::vector<double>> predictions;  // [time_steps][features]
    std::vector<double> confidence;              // Confidence scores for each prediction
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    double modelUncertainty;                     // Overall uncertainty of model
    
    ForecastResult() : modelUncertainty(0.0) {
        startTime = std::chrono::system_clock::now();
        endTime = std::chrono::system_clock::now();
    }
};

// Attention Mechanism for Sequence Models
class AttentionMechanism {
public:
    AttentionMechanism(size_t hiddenSize);
    ~AttentionMechanism() = default;
    
    // Compute attention weights
    std::vector<double> computeAttentionWeights(const std::vector<std::vector<double>>& encoderStates);
    
    // Apply attention to get context vector
    std::vector<double> applyAttention(const std::vector<std::vector<double>>& encoderStates,
                                      const std::vector<double>& attentionWeights);
    
private:
    size_t hiddenSize;
    std::vector<double> attentionWeights;
    std::vector<double> queryVector;
    
    // Initialize attention parameters
    void initializeParameters();
};

// Advanced LSTM with Attention
class AttentionLSTM {
public:
    AttentionLSTM(size_t inputSize, size_t hiddenSize, size_t outputSize,
                  const ForecastingConfig& config = ForecastingConfig());
    ~AttentionLSTM() = default;
    
    // Train the model
    bool train(const std::vector<std::vector<TimeSeriesPoint>>& sequences,
               const std::vector<std::vector<std::vector<double>>>& targets);
    
    // Predict future values
    ForecastResult predict(const std::vector<TimeSeriesPoint>& sequence);
    
    // Predict multiple steps ahead
    ForecastResult predictMultiStep(const std::vector<TimeSeriesPoint>& sequence,
                                   size_t stepsAhead);
    
    // Evaluate model performance
    double evaluate(const std::vector<std::vector<TimeSeriesPoint>>& testSequences,
                   const std::vector<std::vector<std::vector<double>>>& testTargets);
    
    // Save/load model
    bool saveModel(const std::string& filepath);
    bool loadModel(const std::string& filepath);
    
    // Get model information
    size_t getModelSize() const;
    std::map<std::string, double> getTrainingMetrics() const;
    
private:
    size_t inputSize;
    size_t hiddenSize;
    size_t outputSize;
    ForecastingConfig config;
    
    // LSTM layers
    std::vector<std::unique_ptr<LSTMCell>> lstmLayers;
    std::unique_ptr<NeuralLayer> outputLayer;
    std::unique_ptr<AttentionMechanism> attention;
    
    // Training state
    std::vector<double> trainingLosses;
    std::vector<double> validationLosses;
    bool isTrained;
    
    // Internal methods
    std::vector<std::vector<double>> preprocessSequence(const std::vector<TimeSeriesPoint>& sequence);
    std::vector<std::vector<double>> generateTargets(const std::vector<TimeSeriesPoint>& sequence);
    std::vector<double> forwardPass(const std::vector<std::vector<double>>& inputs);
    void backwardPass(const std::vector<std::vector<double>>& inputs,
                      const std::vector<std::vector<double>>& targets,
                      double learningRate);
    
    // Optimization methods
    void adamOptimizer(const std::vector<std::vector<double>>& gradients,
                      std::vector<std::vector<double>>& parameters,
                      std::vector<std::vector<double>>& moment1,
                      std::vector<std::vector<double>>& moment2,
                      size_t timestep);
    
    // Early stopping
    bool shouldStopEarly();
};

// Multi-Modal Forecasting Model
class MultiModalForecaster {
public:
    MultiModalForecaster(const ForecastingConfig& config = ForecastingConfig());
    ~MultiModalForecaster() = default;
    
    // Add different data modalities
    void addModality(const std::string& modalityName,
                    const std::vector<TimeSeriesPoint>& data);
    
    // Remove modality
    void removeModality(const std::string& modalityName);
    
    // Train model on all modalities
    bool trainModel();
    
    // Predict using all modalities
    ForecastResult predictMultiModal();
    
    // Predict for specific modality
    ForecastResult predictModality(const std::string& modalityName);
    
    // Fusion methods
    ForecastResult fusePredictions(const std::map<std::string, ForecastResult>& modalityPredictions);
    
    // Get modality importance weights
    std::map<std::string, double> getModalityWeights() const;
    
private:
    ForecastingConfig config;
    std::map<std::string, std::vector<TimeSeriesPoint>> modalities;
    std::map<std::string, std::unique_ptr<AttentionLSTM>> modalityModels;
    std::map<std::string, double> modalityWeights;
    std::unique_ptr<AttentionLSTM> fusionModel;
    
    mutable std::mutex modalitiesMutex;
    
    // Internal methods
    void calculateModalityWeights();
    std::vector<std::vector<double>> alignModalities(const std::string& referenceModality);
};

// Hierarchical Forecasting System
class HierarchicalForecaster {
public:
    HierarchicalForecaster(const ForecastingConfig& config = ForecastingConfig());
    ~HierarchicalForecaster() = default;
    
    // Add hierarchical level data
    void addLevelData(size_t level, const std::vector<TimeSeriesPoint>& data);
    
    // Train hierarchical model
    bool trainHierarchicalModel();
    
    // Predict at specific level
    ForecastResult predictAtLevel(size_t level);
    
    // Predict across all levels with coherence
    std::map<size_t, ForecastResult> predictCoherent();
    
    // Reconcile forecasts across hierarchy
    std::map<size_t, ForecastResult> reconcileForecasts(const std::map<size_t, ForecastResult>& forecasts);
    
    // Get hierarchy information
    std::vector<size_t> getHierarchyLevels() const;
    size_t getMaxLevel() const;
    
private:
    ForecastingConfig config;
    std::map<size_t, std::vector<TimeSeriesPoint>> levelData;
    std::map<size_t, std::unique_ptr<AttentionLSTM>> levelModels;
    std::map<size_t, std::vector<size_t>> levelChildren;  // Parent-child relationships
    
    mutable std::mutex hierarchyMutex;
    
    // Internal methods
    void buildHierarchy();
    std::vector<std::vector<double>> aggregateToLowerLevel(const std::vector<std::vector<double>>& higherLevelData,
                                                          size_t fromLevel, size_t toLevel);
    std::vector<std::vector<double>> disaggregateToUpperLevel(const std::vector<std::vector<double>>& lowerLevelData,
                                                             size_t fromLevel, size_t toLevel);
};

// Ensemble Forecasting System
class EnsembleForecaster {
public:
    EnsembleForecaster(const ForecastingConfig& baseConfig = ForecastingConfig());
    ~EnsembleForecaster() = default;
    
    // Add forecaster to ensemble
    void addForecaster(std::unique_ptr<AttentionLSTM> forecaster, const std::string& name);
    
    // Remove forecaster from ensemble
    void removeForecaster(const std::string& name);
    
    // Train all forecasters
    bool trainEnsemble(const std::vector<TimeSeriesPoint>& trainingData);
    
    // Ensemble prediction with uncertainty quantification
    ForecastResult predictEnsemble(const std::vector<TimeSeriesPoint>& sequence);
    
    // Bayesian model averaging
    ForecastResult bayesianModelAveraging(const std::vector<TimeSeriesPoint>& sequence);
    
    // Quantile regression for uncertainty
    std::pair<ForecastResult, ForecastResult> predictWithConfidenceIntervals(
        const std::vector<TimeSeriesPoint>& sequence,
        double lowerQuantile = 0.05,
        double upperQuantile = 0.95);
    
    // Get ensemble diversity metrics
    std::map<std::string, double> getEnsembleDiversity() const;
    
    // Dynamic model selection
    std::vector<std::string> selectBestModels(const std::vector<TimeSeriesPoint>& validationData);
    
private:
    ForecastingConfig baseConfig;
    std::map<std::string, std::unique_ptr<AttentionLSTM>> forecasters;
    std::map<std::string, double> forecasterWeights;
    std::map<std::string, std::vector<double>> forecasterPerformances;
    
    mutable std::mutex ensembleMutex;
    
    // Internal methods
    void calculateForecasterWeights();
    std::vector<double> weightedEnsemblePrediction(const std::map<std::string, std::vector<std::vector<double>>>& predictions);
    double calculateForecasterDiversity(const std::string& name1, const std::string& name2);
    void updateForecasterPerformances(const std::string& name, double performance);
};

// Advanced Forecasting Manager
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
    
    // Multi-step prediction with uncertainty
    std::pair<ForecastResult, std::vector<double>> predictWithUncertainty(
        const std::string& seriesName,
        size_t stepsAhead = 10,
        double confidenceLevel = 0.95);
    
    // Cross-validation for model selection
    std::map<std::string, double> crossValidate(const std::string& seriesName,
                                              size_t folds = 5);
    
    // Model selection based on validation
    std::string selectBestModel(const std::string& seriesName);
    
    // Adaptive forecasting (adjusts to changing patterns)
    void enableAdaptiveForecasting(bool enable = true);
    
    // Real-time model updates
    void updateModelOnline(const TimeSeriesPoint& newPoint);
    
    // Get forecasting metrics
    std::map<std::string, double> getModelMetrics(const std::string& seriesName) const;
    
    // Export/import trained models
    bool exportModel(const std::string& seriesName, const std::string& filepath);
    bool importModel(const std::string& seriesName, const std::string& filepath);
    
    // Getters for components
    MultiModalForecaster& getMultiModalForecaster() { return *multiModalForecaster; }
    HierarchicalForecaster& getHierarchicalForecaster() { return *hierarchicalForecaster; }
    EnsembleForecaster& getEnsembleForecaster() { return *ensembleForecaster; }
    
private:
    ForecastingConfig config;
    
    std::unique_ptr<MultiModalForecaster> multiModalForecaster;
    std::unique_ptr<HierarchicalForecaster> hierarchicalForecaster;
    std::unique_ptr<EnsembleForecaster> ensembleForecaster;
    
    std::map<std::string, std::vector<TimeSeriesPoint>> timeSeriesData;
    std::map<std::string, std::unique_ptr<AttentionLSTM>> forecastingModels;
    
    std::atomic<bool> adaptiveForecastingEnabled{false};
    std::atomic<bool> onlineLearningEnabled{false};
    
    mutable std::mutex dataMutex;
    
    // Internal methods
    void preprocessTimeSeries(const std::string& seriesName);
    std::vector<std::vector<double>> extractFeaturesFromPoints(const std::vector<TimeSeriesPoint>& points);
    void detectSeasonalityAndTrends(const std::string& seriesName);
    void handleMissingData(const std::string& seriesName);
    
    // Model evaluation and selection
    double evaluateModel(const std::string& modelName,
                        const std::vector<TimeSeriesPoint>& testData);
    void autoTuneHyperparameters(const std::string& seriesName);
    
    // Performance optimization
    void optimizeModelForLatency(const std::string& seriesName);
    void pruneRedundantFeatures(const std::string& seriesName);
};