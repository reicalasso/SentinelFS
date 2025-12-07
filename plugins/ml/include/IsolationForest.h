#pragma once

#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include <cmath>
#include <mutex>

namespace SentinelFS {

/**
 * @brief Simple Isolation Forest implementation for anomaly detection
 * 
 * Isolation Forest is an unsupervised learning algorithm that isolates anomalies
 * by randomly selecting a feature and then randomly selecting a split value.
 * Anomalies are easier to isolate (require fewer splits) than normal points.
 * 
 * This implementation:
 * - Uses feature vectors (activity rate, entropy, file count changes, etc.)
 * - Builds multiple isolation trees
 * - Calculates anomaly scores based on average path length
 */
class IsolationForest {
public:
    struct Sample {
        std::vector<double> features;
        double anomalyScore{0.0};
    };

    /**
     * @brief Configuration for the Isolation Forest
     */
    struct Config {
        int numTrees = 100;                  // Number of isolation trees
        int sampleSize = 256;                // Subsample size for each tree
        double contaminationRate = 0.1;      // Expected proportion of anomalies
        unsigned int randomSeed = 42;
    };

    IsolationForest();
    explicit IsolationForest(const Config& config);
    ~IsolationForest();

    /**
     * @brief Fit the model on training data
     * @param samples Vector of feature vectors
     */
    void fit(const std::vector<std::vector<double>>& samples);

    /**
     * @brief Predict anomaly score for a single sample
     * @param sample Feature vector
     * @return Anomaly score (0 = normal, 1 = anomaly)
     */
    double predict(const std::vector<double>& sample) const;

    /**
     * @brief Predict anomaly scores for multiple samples
     */
    std::vector<double> predictBatch(const std::vector<std::vector<double>>& samples) const;

    /**
     * @brief Check if a sample is an anomaly
     * @param sample Feature vector
     * @return true if anomaly
     */
    bool isAnomaly(const std::vector<double>& sample) const;

    /**
     * @brief Get the threshold for anomaly detection
     */
    double getThreshold() const { return threshold_; }

    /**
     * @brief Set custom threshold
     */
    void setThreshold(double threshold) { threshold_ = threshold; }

    /**
     * @brief Check if model is trained
     */
    bool isTrained() const { return trained_; }

    /**
     * @brief Get number of features
     */
    size_t getNumFeatures() const { return numFeatures_; }

private:
    // Internal tree node
    struct ITreeNode {
        bool isExternal{false};
        size_t size{0};                     // For external nodes: number of samples that reached here
        size_t splitAttribute{0};           // Feature index to split on
        double splitValue{0.0};             // Split threshold
        std::unique_ptr<ITreeNode> left;
        std::unique_ptr<ITreeNode> right;
    };

    // Isolation Tree
    struct ITree {
        std::unique_ptr<ITreeNode> root;
        
        // Build tree from samples
        void build(const std::vector<std::vector<double>>& samples, 
                   int heightLimit, std::mt19937& rng);
        
        // Get path length for a sample
        double pathLength(const std::vector<double>& sample) const;
        
    private:
        std::unique_ptr<ITreeNode> buildNode(
            const std::vector<std::vector<double>>& samples,
            int currentHeight, int heightLimit, std::mt19937& rng);
        
        double pathLengthRecursive(const ITreeNode* node, 
                                    const std::vector<double>& sample,
                                    int currentLength) const;
    };

    Config config_;
    std::vector<std::unique_ptr<ITree>> trees_;
    double threshold_{0.5};
    bool trained_{false};
    size_t numFeatures_{0};
    mutable std::mutex mutex_;

    // Helper functions
    static double c(size_t n);  // Average path length of unsuccessful search in BST
    double averagePathLength(const std::vector<double>& sample) const;
};

/**
 * @brief Feature extractor for file system activity
 * 
 * Extracts numerical features from file system events for ML analysis:
 * - Activity rate (events per minute)
 * - File size changes
 * - Entropy metrics
 * - Time-based features
 */
class FeatureExtractor {
public:
    struct ActivityFeatures {
        double activityRate{0.0};           // Events per minute
        double createRatio{0.0};            // Create / total events
        double modifyRatio{0.0};            // Modify / total events
        double deleteRatio{0.0};            // Delete / total events
        double avgFileSize{0.0};            // Average file size (normalized)
        double avgEntropy{0.0};             // Average file entropy
        double uniqueDirs{0.0};             // Number of unique directories (normalized)
        double extensionDiversity{0.0};     // Unique extensions / total files
        double hourOfDay{0.0};              // Normalized (0-1)
        double dayOfWeek{0.0};              // Normalized (0-1)
        
        std::vector<double> toVector() const {
            return {activityRate, createRatio, modifyRatio, deleteRatio,
                    avgFileSize, avgEntropy, uniqueDirs, extensionDiversity,
                    hourOfDay, dayOfWeek};
        }
        
        static constexpr size_t FEATURE_COUNT = 10;
    };

    /**
     * @brief Extract features from activity window
     * @param events List of (action, path, size, entropy) tuples
     * @return Feature vector
     */
    static ActivityFeatures extractFeatures(
        const std::vector<std::tuple<std::string, std::string, size_t, double>>& events);

    /**
     * @brief Normalize a feature vector
     */
    static std::vector<double> normalize(const std::vector<double>& features,
                                          const std::vector<double>& mins,
                                          const std::vector<double>& maxs);
};

} // namespace SentinelFS
