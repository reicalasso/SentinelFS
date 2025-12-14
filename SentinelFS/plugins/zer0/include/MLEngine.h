#pragma once

/**
 * @file MLEngine.h
 * @brief Machine Learning Engine for Zer0 Threat Detection
 * 
 * Features:
 * - Anomaly detection using statistical models
 * - Behavioral pattern classification
 * - Time-series analysis for threat prediction
 * - Feature extraction from file and process data
 */

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <deque>
#include <functional>

namespace SentinelFS {
namespace Zer0 {

/**
 * @brief Feature vector for ML analysis
 */
struct FeatureVector {
    // File features
    double entropy{0.0};
    double fileSize{0.0};
    double compressionRatio{0.0};
    double asciiRatio{0.0};
    double printableRatio{0.0};
    
    // Behavioral features
    double modificationRate{0.0};      // Files modified per minute
    double creationRate{0.0};          // Files created per minute
    double deletionRate{0.0};          // Files deleted per minute
    double renameRate{0.0};            // Files renamed per minute
    double accessPatternScore{0.0};    // How unusual is the access pattern
    
    // Process features
    double cpuUsage{0.0};
    double memoryUsage{0.0};
    double networkActivity{0.0};
    double childProcessCount{0.0};
    
    // Time features
    double hourOfDay{0.0};             // 0-23 normalized to 0-1
    double dayOfWeek{0.0};             // 0-6 normalized to 0-1
    bool isBusinessHours{false};
    
    // Convert to raw vector for ML operations
    std::vector<double> toVector() const {
        return {
            entropy, fileSize, compressionRatio, asciiRatio, printableRatio,
            modificationRate, creationRate, deletionRate, renameRate, accessPatternScore,
            cpuUsage, memoryUsage, networkActivity, childProcessCount,
            hourOfDay, dayOfWeek, isBusinessHours ? 1.0 : 0.0
        };
    }
    
    static size_t featureCount() { return 17; }
};

/**
 * @brief Anomaly detection result
 */
struct AnomalyResult {
    double anomalyScore{0.0};          // 0.0 = normal, 1.0 = highly anomalous
    double confidence{0.0};            // Confidence in the prediction
    std::string category;              // Category of anomaly
    std::vector<std::string> reasons;  // Why it's anomalous
    FeatureVector features;            // The analyzed features
};

/**
 * @brief Time series data point
 */
struct TimeSeriesPoint {
    std::chrono::steady_clock::time_point timestamp;
    double value;
    std::string label;
};

/**
 * @brief Isolation Forest for anomaly detection
 */
class IsolationForest {
public:
    IsolationForest(int numTrees = 100, int sampleSize = 256)
        : numTrees_(numTrees), sampleSize_(sampleSize) {}
    
    void fit(const std::vector<std::vector<double>>& data);
    double predict(const std::vector<double>& point) const;
    
private:
    struct IsolationTree {
        struct Node {
            int splitFeature{-1};
            double splitValue{0.0};
            std::unique_ptr<Node> left;
            std::unique_ptr<Node> right;
            int size{0};
            bool isLeaf{false};
        };
        
        std::unique_ptr<Node> root;
        
        void build(const std::vector<std::vector<double>>& data, int maxDepth);
        double pathLength(const std::vector<double>& point, Node* node, int depth) const;
    };
    
    std::vector<IsolationTree> trees_;
    int numTrees_;
    int sampleSize_;
    double avgPathLength_{0.0};
    
    static double c(int n);
};

/**
 * @brief Statistical model for baseline behavior
 */
class StatisticalModel {
public:
    void update(const std::string& metric, double value);
    double getZScore(const std::string& metric, double value) const;
    double getMean(const std::string& metric) const;
    double getStdDev(const std::string& metric) const;
    bool isAnomaly(const std::string& metric, double value, double threshold = 3.0) const;
    
private:
    struct MetricStats {
        double sum{0.0};
        double sumSquares{0.0};
        int count{0};
        double min{std::numeric_limits<double>::max()};
        double max{std::numeric_limits<double>::lowest()};
        std::deque<double> recentValues;
        static constexpr int maxRecentValues = 1000;
    };
    
    mutable std::mutex mutex_;
    std::map<std::string, MetricStats> metrics_;
};

/**
 * @brief Time series analyzer for pattern detection
 */
class TimeSeriesAnalyzer {
public:
    void addPoint(const std::string& series, double value);
    
    // Analysis methods
    double detectTrend(const std::string& series) const;
    double detectSeasonality(const std::string& series) const;
    bool detectSpike(const std::string& series, double threshold = 3.0) const;
    std::vector<double> forecast(const std::string& series, int steps) const;
    
    // Pattern detection
    bool detectRansomwarePattern() const;
    bool detectExfiltrationPattern() const;
    bool detectBruteForcePattern() const;
    
private:
    struct Series {
        std::deque<TimeSeriesPoint> points;
        static constexpr int maxPoints = 10000;
    };
    
    mutable std::mutex mutex_;
    std::map<std::string, Series> series_;
    
    std::vector<double> movingAverage(const std::vector<double>& data, int window) const;
    std::vector<double> exponentialSmoothing(const std::vector<double>& data, double alpha) const;
};

/**
 * @brief Main ML Engine class
 */
class MLEngine {
public:
    MLEngine();
    ~MLEngine();
    
    /**
     * @brief Initialize the ML engine
     */
    bool initialize(const std::string& modelPath = "");
    
    /**
     * @brief Shutdown the ML engine
     */
    void shutdown();
    
    /**
     * @brief Analyze features for anomalies
     */
    AnomalyResult analyzeFeatures(const FeatureVector& features);
    
    /**
     * @brief Record behavioral event for learning
     */
    void recordEvent(const std::string& eventType, const std::map<std::string, double>& metrics);
    
    /**
     * @brief Update baseline with normal behavior
     */
    void updateBaseline(const FeatureVector& features);
    
    /**
     * @brief Check for behavioral anomalies
     */
    AnomalyResult checkBehavioralAnomalies();
    
    /**
     * @brief Get threat prediction score
     */
    double getThreatPrediction(const FeatureVector& features);
    
    /**
     * @brief Train the model with labeled data
     */
    void train(const std::vector<std::pair<FeatureVector, bool>>& labeledData);
    
    /**
     * @brief Train model from directory (unsupervised baseline learning)
     * Scans all files in directory and builds baseline model
     * @param directoryPath Path to directory to scan
     * @param recursive Whether to scan subdirectories
     * @param progressCallback Optional callback for progress updates
     * @return Number of files processed
     */
    int trainFromDirectory(const std::string& directoryPath, 
                           bool recursive = true,
                           std::function<void(int, int, const std::string&)> progressCallback = nullptr);
    
    /**
     * @brief Get training status
     */
    struct TrainingStatus {
        bool isTraining{false};
        int filesProcessed{0};
        int totalFiles{0};
        std::string currentFile;
        double progress{0.0};
    };
    TrainingStatus getTrainingStatus() const;
    
    /**
     * @brief Save model to file
     */
    bool saveModel(const std::string& path) const;
    
    /**
     * @brief Load model from file
     */
    bool loadModel(const std::string& path);
    
    /**
     * @brief Get model statistics
     */
    struct ModelStats {
        int samplesProcessed{0};
        int anomaliesDetected{0};
        double avgAnomalyScore{0.0};
        double falsePositiveRate{0.0};
        std::chrono::steady_clock::time_point lastUpdate;
    };
    ModelStats getStats() const;
    
    /**
     * @brief Feature extraction helpers
     */
    static FeatureVector extractFileFeatures(const std::string& path);
    static FeatureVector extractProcessFeatures(pid_t pid);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Neural network for classification (simple feedforward)
 */
class SimpleNeuralNetwork {
public:
    SimpleNeuralNetwork(const std::vector<int>& layerSizes);
    
    std::vector<double> forward(const std::vector<double>& input);
    void train(const std::vector<std::pair<std::vector<double>, std::vector<double>>>& data,
               int epochs, double learningRate);
    
    void saveWeights(const std::string& path) const;
    bool loadWeights(const std::string& path);
    
private:
    struct Layer {
        std::vector<std::vector<double>> weights;
        std::vector<double> biases;
        std::vector<double> outputs;
    };
    
    std::vector<Layer> layers_;
    
    static double sigmoid(double x) { return 1.0 / (1.0 + std::exp(-x)); }
    static double sigmoidDerivative(double x) { return x * (1.0 - x); }
    static double relu(double x) { return std::max(0.0, x); }
    static double reluDerivative(double x) { return x > 0 ? 1.0 : 0.0; }
};

/**
 * @brief K-Means clustering for behavior grouping
 */
class KMeansClustering {
public:
    KMeansClustering(int k, int maxIterations = 100);
    
    void fit(const std::vector<std::vector<double>>& data);
    int predict(const std::vector<double>& point) const;
    double distanceToNearestCentroid(const std::vector<double>& point) const;
    
    const std::vector<std::vector<double>>& getCentroids() const { return centroids_; }
    
private:
    int k_;
    int maxIterations_;
    std::vector<std::vector<double>> centroids_;
    
    static double euclideanDistance(const std::vector<double>& a, const std::vector<double>& b);
};

} // namespace Zer0
} // namespace SentinelFS
