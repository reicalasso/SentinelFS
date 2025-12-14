#include "MLEngine.h"
#include <fstream>
#include <random>
#include <cstring>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#include <functional>
#include <array>
#include <atomic>

namespace SentinelFS {
namespace Zer0 {

// ============================================================================
// IsolationForest Implementation
// ============================================================================

double IsolationForest::c(int n) {
    if (n <= 1) return 0.0;
    double H = std::log(n - 1) + 0.5772156649; // Euler's constant
    return 2.0 * H - (2.0 * (n - 1.0) / n);
}

void IsolationForest::IsolationTree::build(const std::vector<std::vector<double>>& data, int maxDepth) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    std::function<std::unique_ptr<Node>(const std::vector<std::vector<double>>&, int)> buildNode;
    buildNode = [&](const std::vector<std::vector<double>>& subset, int depth) -> std::unique_ptr<Node> {
        auto node = std::make_unique<Node>();
        node->size = subset.size();
        
        if (depth >= maxDepth || subset.size() <= 1) {
            node->isLeaf = true;
            return node;
        }
        
        int numFeatures = subset[0].size();
        std::uniform_int_distribution<> featureDist(0, numFeatures - 1);
        node->splitFeature = featureDist(gen);
        
        double minVal = std::numeric_limits<double>::max();
        double maxVal = std::numeric_limits<double>::lowest();
        for (const auto& point : subset) {
            minVal = std::min(minVal, point[node->splitFeature]);
            maxVal = std::max(maxVal, point[node->splitFeature]);
        }
        
        if (minVal == maxVal) {
            node->isLeaf = true;
            return node;
        }
        
        std::uniform_real_distribution<> splitDist(minVal, maxVal);
        node->splitValue = splitDist(gen);
        
        std::vector<std::vector<double>> leftSubset, rightSubset;
        for (const auto& point : subset) {
            if (point[node->splitFeature] < node->splitValue) {
                leftSubset.push_back(point);
            } else {
                rightSubset.push_back(point);
            }
        }
        
        if (leftSubset.empty() || rightSubset.empty()) {
            node->isLeaf = true;
            return node;
        }
        
        node->left = buildNode(leftSubset, depth + 1);
        node->right = buildNode(rightSubset, depth + 1);
        
        return node;
    };
    
    int maxTreeDepth = static_cast<int>(std::ceil(std::log2(data.size())));
    root = buildNode(data, 0);
}

double IsolationForest::IsolationTree::pathLength(const std::vector<double>& point, Node* node, int depth) const {
    if (!node || node->isLeaf) {
        return depth + c(node ? node->size : 1);
    }
    
    if (point[node->splitFeature] < node->splitValue) {
        return pathLength(point, node->left.get(), depth + 1);
    } else {
        return pathLength(point, node->right.get(), depth + 1);
    }
}

void IsolationForest::fit(const std::vector<std::vector<double>>& data) {
    if (data.empty()) return;
    
    trees_.clear();
    trees_.resize(numTrees_);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    int actualSampleSize = std::min(sampleSize_, static_cast<int>(data.size()));
    avgPathLength_ = c(actualSampleSize);
    
    for (auto& tree : trees_) {
        std::vector<std::vector<double>> sample;
        std::vector<int> indices(data.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), gen);
        
        for (int i = 0; i < actualSampleSize; ++i) {
            sample.push_back(data[indices[i]]);
        }
        
        tree.build(sample, static_cast<int>(std::ceil(std::log2(actualSampleSize))));
    }
}

double IsolationForest::predict(const std::vector<double>& point) const {
    if (trees_.empty()) return 0.5;
    
    double avgPath = 0.0;
    for (const auto& tree : trees_) {
        avgPath += tree.pathLength(point, tree.root.get(), 0);
    }
    avgPath /= trees_.size();
    
    // Anomaly score: 2^(-avgPath / c(n))
    return std::pow(2.0, -avgPath / avgPathLength_);
}

// ============================================================================
// StatisticalModel Implementation
// ============================================================================

void StatisticalModel::update(const std::string& metric, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& stats = metrics_[metric];
    stats.sum += value;
    stats.sumSquares += value * value;
    stats.count++;
    stats.min = std::min(stats.min, value);
    stats.max = std::max(stats.max, value);
    
    stats.recentValues.push_back(value);
    if (stats.recentValues.size() > MetricStats::maxRecentValues) {
        stats.recentValues.pop_front();
    }
}

double StatisticalModel::getMean(const std::string& metric) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = metrics_.find(metric);
    if (it == metrics_.end() || it->second.count == 0) return 0.0;
    
    return it->second.sum / it->second.count;
}

double StatisticalModel::getStdDev(const std::string& metric) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = metrics_.find(metric);
    if (it == metrics_.end() || it->second.count < 2) return 0.0;
    
    double mean = it->second.sum / it->second.count;
    double variance = (it->second.sumSquares / it->second.count) - (mean * mean);
    return std::sqrt(std::max(0.0, variance));
}

double StatisticalModel::getZScore(const std::string& metric, double value) const {
    double mean = getMean(metric);
    double stdDev = getStdDev(metric);
    
    if (stdDev < 1e-10) return 0.0;
    return (value - mean) / stdDev;
}

bool StatisticalModel::isAnomaly(const std::string& metric, double value, double threshold) const {
    return std::abs(getZScore(metric, value)) > threshold;
}

// ============================================================================
// TimeSeriesAnalyzer Implementation
// ============================================================================

void TimeSeriesAnalyzer::addPoint(const std::string& seriesName, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& s = series_[seriesName];
    s.points.push_back({std::chrono::steady_clock::now(), value, ""});
    
    if (s.points.size() > Series::maxPoints) {
        s.points.pop_front();
    }
}

double TimeSeriesAnalyzer::detectTrend(const std::string& seriesName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = series_.find(seriesName);
    if (it == series_.end() || it->second.points.size() < 10) return 0.0;
    
    const auto& points = it->second.points;
    int n = points.size();
    
    // Simple linear regression
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (int i = 0; i < n; ++i) {
        sumX += i;
        sumY += points[i].value;
        sumXY += i * points[i].value;
        sumX2 += i * i;
    }
    
    double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    return slope;
}

double TimeSeriesAnalyzer::detectSeasonality(const std::string& seriesName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = series_.find(seriesName);
    if (it == series_.end() || it->second.points.size() < 100) return 0.0;
    
    // Simplified autocorrelation check
    const auto& points = it->second.points;
    std::vector<double> values;
    for (const auto& p : points) {
        values.push_back(p.value);
    }
    
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double variance = 0.0;
    for (double v : values) {
        variance += (v - mean) * (v - mean);
    }
    variance /= values.size();
    
    if (variance < 1e-10) return 0.0;
    
    // Check for daily pattern (assuming ~1 point per second, 86400 seconds per day)
    int lag = std::min(static_cast<int>(values.size() / 2), 3600); // 1 hour lag
    double autocorr = 0.0;
    for (size_t i = 0; i < values.size() - lag; ++i) {
        autocorr += (values[i] - mean) * (values[i + lag] - mean);
    }
    autocorr /= (values.size() - lag) * variance;
    
    return autocorr;
}

bool TimeSeriesAnalyzer::detectSpike(const std::string& seriesName, double threshold) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = series_.find(seriesName);
    if (it == series_.end() || it->second.points.size() < 10) return false;
    
    const auto& points = it->second.points;
    
    // Calculate mean and std of recent values
    std::vector<double> recent;
    int windowSize = std::min(static_cast<int>(points.size()), 100);
    for (int i = points.size() - windowSize; i < static_cast<int>(points.size()) - 1; ++i) {
        recent.push_back(points[i].value);
    }
    
    double mean = std::accumulate(recent.begin(), recent.end(), 0.0) / recent.size();
    double variance = 0.0;
    for (double v : recent) {
        variance += (v - mean) * (v - mean);
    }
    double stdDev = std::sqrt(variance / recent.size());
    
    if (stdDev < 1e-10) return false;
    
    double latestValue = points.back().value;
    double zScore = (latestValue - mean) / stdDev;
    
    return std::abs(zScore) > threshold;
}

std::vector<double> TimeSeriesAnalyzer::forecast(const std::string& seriesName, int steps) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = series_.find(seriesName);
    if (it == series_.end() || it->second.points.size() < 10) {
        return std::vector<double>(steps, 0.0);
    }
    
    // Simple exponential smoothing forecast
    const auto& points = it->second.points;
    std::vector<double> values;
    for (const auto& p : points) {
        values.push_back(p.value);
    }
    
    double alpha = 0.3;
    std::vector<double> smoothed = exponentialSmoothing(values, alpha);
    
    std::vector<double> result(steps, smoothed.back());
    return result;
}

bool TimeSeriesAnalyzer::detectRansomwarePattern() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check for sudden spike in file modifications
    auto modIt = series_.find("file_modifications");
    auto renameIt = series_.find("file_renames");
    
    bool modSpike = false;
    bool renameSpike = false;
    
    if (modIt != series_.end() && modIt->second.points.size() >= 10) {
        const auto& points = modIt->second.points;
        double recentSum = 0;
        int recentCount = std::min(10, static_cast<int>(points.size()));
        for (int i = points.size() - recentCount; i < static_cast<int>(points.size()); ++i) {
            recentSum += points[i].value;
        }
        modSpike = recentSum > 50; // More than 50 modifications in recent window
    }
    
    if (renameIt != series_.end() && renameIt->second.points.size() >= 10) {
        const auto& points = renameIt->second.points;
        double recentSum = 0;
        int recentCount = std::min(10, static_cast<int>(points.size()));
        for (int i = points.size() - recentCount; i < static_cast<int>(points.size()); ++i) {
            recentSum += points[i].value;
        }
        renameSpike = recentSum > 20; // More than 20 renames in recent window
    }
    
    return modSpike && renameSpike;
}

bool TimeSeriesAnalyzer::detectExfiltrationPattern() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check for unusual network upload activity
    auto uploadIt = series_.find("network_upload");
    auto accessIt = series_.find("file_access");
    
    if (uploadIt == series_.end() || accessIt == series_.end()) return false;
    
    // Detect correlation between file access and network upload
    // This is a simplified check
    const auto& uploads = uploadIt->second.points;
    const auto& accesses = accessIt->second.points;
    
    if (uploads.size() < 10 || accesses.size() < 10) return false;
    
    // Check if both are elevated simultaneously
    double recentUploads = 0, recentAccesses = 0;
    int window = 10;
    
    for (int i = uploads.size() - window; i < static_cast<int>(uploads.size()); ++i) {
        recentUploads += uploads[i].value;
    }
    for (int i = accesses.size() - window; i < static_cast<int>(accesses.size()); ++i) {
        recentAccesses += accesses[i].value;
    }
    
    return recentUploads > 1000000 && recentAccesses > 100; // 1MB upload and 100 file accesses
}

bool TimeSeriesAnalyzer::detectBruteForcePattern() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto authIt = series_.find("auth_failures");
    if (authIt == series_.end() || authIt->second.points.size() < 10) return false;
    
    const auto& points = authIt->second.points;
    double recentFailures = 0;
    int window = std::min(60, static_cast<int>(points.size())); // Last minute
    
    for (int i = points.size() - window; i < static_cast<int>(points.size()); ++i) {
        recentFailures += points[i].value;
    }
    
    return recentFailures > 10; // More than 10 auth failures in a minute
}

std::vector<double> TimeSeriesAnalyzer::movingAverage(const std::vector<double>& data, int window) const {
    std::vector<double> result;
    if (data.size() < static_cast<size_t>(window)) return data;
    
    double sum = 0;
    for (int i = 0; i < window; ++i) {
        sum += data[i];
    }
    result.push_back(sum / window);
    
    for (size_t i = window; i < data.size(); ++i) {
        sum = sum - data[i - window] + data[i];
        result.push_back(sum / window);
    }
    
    return result;
}

std::vector<double> TimeSeriesAnalyzer::exponentialSmoothing(const std::vector<double>& data, double alpha) const {
    if (data.empty()) return {};
    
    std::vector<double> result;
    result.push_back(data[0]);
    
    for (size_t i = 1; i < data.size(); ++i) {
        result.push_back(alpha * data[i] + (1 - alpha) * result.back());
    }
    
    return result;
}

// ============================================================================
// SimpleNeuralNetwork Implementation
// ============================================================================

SimpleNeuralNetwork::SimpleNeuralNetwork(const std::vector<int>& layerSizes) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dist(0.0, 0.5);
    
    for (size_t i = 1; i < layerSizes.size(); ++i) {
        Layer layer;
        int inputSize = layerSizes[i - 1];
        int outputSize = layerSizes[i];
        
        layer.weights.resize(outputSize);
        layer.biases.resize(outputSize);
        layer.outputs.resize(outputSize);
        
        for (int j = 0; j < outputSize; ++j) {
            layer.weights[j].resize(inputSize);
            for (int k = 0; k < inputSize; ++k) {
                layer.weights[j][k] = dist(gen);
            }
            layer.biases[j] = dist(gen);
        }
        
        layers_.push_back(std::move(layer));
    }
}

std::vector<double> SimpleNeuralNetwork::forward(const std::vector<double>& input) {
    std::vector<double> current = input;
    
    for (size_t l = 0; l < layers_.size(); ++l) {
        auto& layer = layers_[l];
        std::vector<double> next(layer.weights.size());
        
        for (size_t j = 0; j < layer.weights.size(); ++j) {
            double sum = layer.biases[j];
            for (size_t k = 0; k < current.size(); ++k) {
                sum += layer.weights[j][k] * current[k];
            }
            // Use sigmoid for output layer, ReLU for hidden
            if (l == layers_.size() - 1) {
                next[j] = sigmoid(sum);
            } else {
                next[j] = relu(sum);
            }
            layer.outputs[j] = next[j];
        }
        
        current = next;
    }
    
    return current;
}

void SimpleNeuralNetwork::train(
    const std::vector<std::pair<std::vector<double>, std::vector<double>>>& data,
    int epochs, double learningRate) {
    
    for (int epoch = 0; epoch < epochs; ++epoch) {
        double totalError = 0.0;
        
        for (const auto& [input, target] : data) {
            // Forward pass
            std::vector<double> output = forward(input);
            
            // Calculate error
            for (size_t i = 0; i < output.size(); ++i) {
                totalError += (target[i] - output[i]) * (target[i] - output[i]);
            }
            
            // Backpropagation (simplified)
            std::vector<double> errors(output.size());
            for (size_t i = 0; i < output.size(); ++i) {
                errors[i] = (target[i] - output[i]) * sigmoidDerivative(output[i]);
            }
            
            // Update weights for output layer
            auto& outputLayer = layers_.back();
            std::vector<double> prevOutputs = layers_.size() > 1 ? 
                layers_[layers_.size() - 2].outputs : input;
            
            for (size_t j = 0; j < outputLayer.weights.size(); ++j) {
                for (size_t k = 0; k < outputLayer.weights[j].size(); ++k) {
                    outputLayer.weights[j][k] += learningRate * errors[j] * prevOutputs[k];
                }
                outputLayer.biases[j] += learningRate * errors[j];
            }
        }
    }
}

void SimpleNeuralNetwork::saveWeights(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) return;
    
    size_t numLayers = layers_.size();
    file.write(reinterpret_cast<const char*>(&numLayers), sizeof(numLayers));
    
    for (const auto& layer : layers_) {
        size_t numNeurons = layer.weights.size();
        file.write(reinterpret_cast<const char*>(&numNeurons), sizeof(numNeurons));
        
        for (const auto& neuronWeights : layer.weights) {
            size_t numWeights = neuronWeights.size();
            file.write(reinterpret_cast<const char*>(&numWeights), sizeof(numWeights));
            file.write(reinterpret_cast<const char*>(neuronWeights.data()), 
                      numWeights * sizeof(double));
        }
        
        file.write(reinterpret_cast<const char*>(layer.biases.data()), 
                  layer.biases.size() * sizeof(double));
    }
}

bool SimpleNeuralNetwork::loadWeights(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    
    size_t numLayers;
    file.read(reinterpret_cast<char*>(&numLayers), sizeof(numLayers));
    
    if (numLayers != layers_.size()) return false;
    
    for (auto& layer : layers_) {
        size_t numNeurons;
        file.read(reinterpret_cast<char*>(&numNeurons), sizeof(numNeurons));
        
        if (numNeurons != layer.weights.size()) return false;
        
        for (auto& neuronWeights : layer.weights) {
            size_t numWeights;
            file.read(reinterpret_cast<char*>(&numWeights), sizeof(numWeights));
            
            if (numWeights != neuronWeights.size()) return false;
            
            file.read(reinterpret_cast<char*>(neuronWeights.data()), 
                     numWeights * sizeof(double));
        }
        
        file.read(reinterpret_cast<char*>(layer.biases.data()), 
                 layer.biases.size() * sizeof(double));
    }
    
    return true;
}

// ============================================================================
// KMeansClustering Implementation
// ============================================================================

KMeansClustering::KMeansClustering(int k, int maxIterations)
    : k_(k), maxIterations_(maxIterations) {}

double KMeansClustering::euclideanDistance(const std::vector<double>& a, const std::vector<double>& b) {
    double sum = 0.0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        double diff = a[i] - b[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

void KMeansClustering::fit(const std::vector<std::vector<double>>& data) {
    if (data.empty() || k_ <= 0) return;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Initialize centroids randomly
    std::vector<int> indices(data.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), gen);
    
    centroids_.clear();
    for (int i = 0; i < k_ && i < static_cast<int>(data.size()); ++i) {
        centroids_.push_back(data[indices[i]]);
    }
    
    std::vector<int> assignments(data.size());
    
    for (int iter = 0; iter < maxIterations_; ++iter) {
        // Assign points to nearest centroid
        bool changed = false;
        for (size_t i = 0; i < data.size(); ++i) {
            int nearest = 0;
            double minDist = euclideanDistance(data[i], centroids_[0]);
            
            for (int j = 1; j < k_; ++j) {
                double dist = euclideanDistance(data[i], centroids_[j]);
                if (dist < minDist) {
                    minDist = dist;
                    nearest = j;
                }
            }
            
            if (assignments[i] != nearest) {
                assignments[i] = nearest;
                changed = true;
            }
        }
        
        if (!changed) break;
        
        // Update centroids
        std::vector<std::vector<double>> newCentroids(k_);
        std::vector<int> counts(k_, 0);
        
        for (int j = 0; j < k_; ++j) {
            newCentroids[j].resize(data[0].size(), 0.0);
        }
        
        for (size_t i = 0; i < data.size(); ++i) {
            int cluster = assignments[i];
            counts[cluster]++;
            for (size_t d = 0; d < data[i].size(); ++d) {
                newCentroids[cluster][d] += data[i][d];
            }
        }
        
        for (int j = 0; j < k_; ++j) {
            if (counts[j] > 0) {
                for (size_t d = 0; d < newCentroids[j].size(); ++d) {
                    newCentroids[j][d] /= counts[j];
                }
                centroids_[j] = newCentroids[j];
            }
        }
    }
}

int KMeansClustering::predict(const std::vector<double>& point) const {
    if (centroids_.empty()) return -1;
    
    int nearest = 0;
    double minDist = euclideanDistance(point, centroids_[0]);
    
    for (size_t i = 1; i < centroids_.size(); ++i) {
        double dist = euclideanDistance(point, centroids_[i]);
        if (dist < minDist) {
            minDist = dist;
            nearest = i;
        }
    }
    
    return nearest;
}

double KMeansClustering::distanceToNearestCentroid(const std::vector<double>& point) const {
    if (centroids_.empty()) return std::numeric_limits<double>::max();
    
    double minDist = euclideanDistance(point, centroids_[0]);
    for (size_t i = 1; i < centroids_.size(); ++i) {
        minDist = std::min(minDist, euclideanDistance(point, centroids_[i]));
    }
    
    return minDist;
}

// ============================================================================
// MLEngine Implementation
// ============================================================================

class MLEngine::Impl {
public:
    std::unique_ptr<IsolationForest> isolationForest;
    std::unique_ptr<StatisticalModel> statisticalModel;
    std::unique_ptr<TimeSeriesAnalyzer> timeSeriesAnalyzer;
    std::unique_ptr<SimpleNeuralNetwork> neuralNetwork;
    std::unique_ptr<KMeansClustering> clustering;
    
    std::vector<std::vector<double>> trainingData;
    ModelStats stats;
    mutable std::mutex mutex;
    
    bool initialized{false};
};

MLEngine::MLEngine() : impl_(std::make_unique<Impl>()) {}

MLEngine::~MLEngine() = default;

bool MLEngine::initialize(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    impl_->isolationForest = std::make_unique<IsolationForest>(100, 256);
    impl_->statisticalModel = std::make_unique<StatisticalModel>();
    impl_->timeSeriesAnalyzer = std::make_unique<TimeSeriesAnalyzer>();
    impl_->neuralNetwork = std::make_unique<SimpleNeuralNetwork>(
        std::vector<int>{static_cast<int>(FeatureVector::featureCount()), 32, 16, 1}
    );
    impl_->clustering = std::make_unique<KMeansClustering>(5);
    
    if (!modelPath.empty() && std::filesystem::exists(modelPath)) {
        loadModel(modelPath);
    }
    
    impl_->initialized = true;
    impl_->stats.lastUpdate = std::chrono::steady_clock::now();
    
    return true;
}

void MLEngine::shutdown() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->initialized = false;
}

AnomalyResult MLEngine::analyzeFeatures(const FeatureVector& features) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    AnomalyResult result;
    result.features = features;
    
    if (!impl_->initialized) {
        return result;
    }
    
    auto featureVec = features.toVector();
    
    // Isolation Forest score
    double iforestScore = impl_->isolationForest->predict(featureVec);
    
    // Statistical anomaly checks
    double entropyZScore = impl_->statisticalModel->getZScore("entropy", features.entropy);
    double modRateZScore = impl_->statisticalModel->getZScore("modification_rate", features.modificationRate);
    
    // Combine scores
    result.anomalyScore = 0.4 * iforestScore + 
                          0.3 * std::min(1.0, std::abs(entropyZScore) / 5.0) +
                          0.3 * std::min(1.0, std::abs(modRateZScore) / 5.0);
    
    result.confidence = std::min(1.0, impl_->stats.samplesProcessed / 1000.0);
    
    // Determine category and reasons
    if (result.anomalyScore > 0.8) {
        result.category = "HIGH_RISK";
        if (iforestScore > 0.7) result.reasons.push_back("Unusual feature combination");
        if (entropyZScore > 3) result.reasons.push_back("Abnormally high entropy");
        if (modRateZScore > 3) result.reasons.push_back("Unusual modification rate");
    } else if (result.anomalyScore > 0.5) {
        result.category = "SUSPICIOUS";
    } else {
        result.category = "NORMAL";
    }
    
    impl_->stats.samplesProcessed++;
    if (result.anomalyScore > 0.5) {
        impl_->stats.anomaliesDetected++;
    }
    impl_->stats.avgAnomalyScore = 
        (impl_->stats.avgAnomalyScore * (impl_->stats.samplesProcessed - 1) + result.anomalyScore) / 
        impl_->stats.samplesProcessed;
    
    return result;
}

void MLEngine::recordEvent(const std::string& eventType, const std::map<std::string, double>& metrics) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    if (!impl_->initialized) return;
    
    for (const auto& [name, value] : metrics) {
        impl_->statisticalModel->update(name, value);
        impl_->timeSeriesAnalyzer->addPoint(eventType + "_" + name, value);
    }
}

void MLEngine::updateBaseline(const FeatureVector& features) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    if (!impl_->initialized) return;
    
    impl_->trainingData.push_back(features.toVector());
    
    // Update statistical model
    impl_->statisticalModel->update("entropy", features.entropy);
    impl_->statisticalModel->update("file_size", features.fileSize);
    impl_->statisticalModel->update("modification_rate", features.modificationRate);
    impl_->statisticalModel->update("creation_rate", features.creationRate);
    
    // Retrain isolation forest periodically
    if (impl_->trainingData.size() % 100 == 0 && impl_->trainingData.size() >= 256) {
        impl_->isolationForest->fit(impl_->trainingData);
        impl_->clustering->fit(impl_->trainingData);
    }
    
    impl_->stats.lastUpdate = std::chrono::steady_clock::now();
}

AnomalyResult MLEngine::checkBehavioralAnomalies() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    AnomalyResult result;
    
    if (!impl_->initialized) return result;
    
    // Check time series patterns
    if (impl_->timeSeriesAnalyzer->detectRansomwarePattern()) {
        result.anomalyScore = 0.95;
        result.category = "RANSOMWARE";
        result.reasons.push_back("Ransomware-like file modification pattern detected");
        result.confidence = 0.9;
    } else if (impl_->timeSeriesAnalyzer->detectExfiltrationPattern()) {
        result.anomalyScore = 0.85;
        result.category = "EXFILTRATION";
        result.reasons.push_back("Potential data exfiltration pattern detected");
        result.confidence = 0.8;
    } else if (impl_->timeSeriesAnalyzer->detectBruteForcePattern()) {
        result.anomalyScore = 0.75;
        result.category = "BRUTE_FORCE";
        result.reasons.push_back("Brute force attack pattern detected");
        result.confidence = 0.85;
    }
    
    return result;
}

double MLEngine::getThreatPrediction(const FeatureVector& features) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    if (!impl_->initialized) return 0.0;
    
    auto output = impl_->neuralNetwork->forward(features.toVector());
    return output.empty() ? 0.0 : output[0];
}

void MLEngine::train(const std::vector<std::pair<FeatureVector, bool>>& labeledData) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    if (!impl_->initialized || labeledData.empty()) return;
    
    // Prepare training data for neural network
    std::vector<std::pair<std::vector<double>, std::vector<double>>> nnData;
    std::vector<std::vector<double>> normalData;
    
    for (const auto& [features, isThreat] : labeledData) {
        nnData.push_back({features.toVector(), {isThreat ? 1.0 : 0.0}});
        if (!isThreat) {
            normalData.push_back(features.toVector());
        }
    }
    
    // Train neural network
    impl_->neuralNetwork->train(nnData, 100, 0.01);
    
    // Train isolation forest on normal data only
    if (!normalData.empty()) {
        impl_->isolationForest->fit(normalData);
        impl_->clustering->fit(normalData);
    }
}

bool MLEngine::saveModel(const std::string& path) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    impl_->neuralNetwork->saveWeights(path + ".nn");
    
    // Save training data for isolation forest
    std::ofstream file(path + ".data", std::ios::binary);
    if (!file) return false;
    
    size_t numSamples = impl_->trainingData.size();
    file.write(reinterpret_cast<const char*>(&numSamples), sizeof(numSamples));
    
    for (const auto& sample : impl_->trainingData) {
        size_t sampleSize = sample.size();
        file.write(reinterpret_cast<const char*>(&sampleSize), sizeof(sampleSize));
        file.write(reinterpret_cast<const char*>(sample.data()), sampleSize * sizeof(double));
    }
    
    return true;
}

bool MLEngine::loadModel(const std::string& path) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    impl_->neuralNetwork->loadWeights(path + ".nn");
    
    std::ifstream file(path + ".data", std::ios::binary);
    if (!file) return false;
    
    size_t numSamples;
    file.read(reinterpret_cast<char*>(&numSamples), sizeof(numSamples));
    
    impl_->trainingData.clear();
    for (size_t i = 0; i < numSamples; ++i) {
        size_t sampleSize;
        file.read(reinterpret_cast<char*>(&sampleSize), sizeof(sampleSize));
        
        std::vector<double> sample(sampleSize);
        file.read(reinterpret_cast<char*>(sample.data()), sampleSize * sizeof(double));
        impl_->trainingData.push_back(sample);
    }
    
    if (!impl_->trainingData.empty()) {
        impl_->isolationForest->fit(impl_->trainingData);
        impl_->clustering->fit(impl_->trainingData);
    }
    
    return true;
}

MLEngine::ModelStats MLEngine::getStats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stats;
}

FeatureVector MLEngine::extractFileFeatures(const std::string& path) {
    FeatureVector features;
    
    std::ifstream file(path, std::ios::binary);
    if (!file) return features;
    
    // Read file content
    std::vector<uint8_t> content((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
    
    features.fileSize = content.size();
    
    // Calculate entropy
    std::array<uint64_t, 256> freq{};
    int asciiCount = 0, printableCount = 0;
    
    for (uint8_t byte : content) {
        freq[byte]++;
        if (byte < 128) asciiCount++;
        if (byte >= 32 && byte < 127) printableCount++;
    }
    
    double entropy = 0.0;
    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) {
            double p = static_cast<double>(freq[i]) / content.size();
            entropy -= p * std::log2(p);
        }
    }
    
    features.entropy = entropy;
    features.asciiRatio = content.empty() ? 0.0 : static_cast<double>(asciiCount) / content.size();
    features.printableRatio = content.empty() ? 0.0 : static_cast<double>(printableCount) / content.size();
    
    // Time features
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto* tm = std::localtime(&time);
    
    features.hourOfDay = tm->tm_hour / 24.0;
    features.dayOfWeek = tm->tm_wday / 7.0;
    features.isBusinessHours = (tm->tm_hour >= 9 && tm->tm_hour < 17) && 
                               (tm->tm_wday >= 1 && tm->tm_wday <= 5);
    
    return features;
}

FeatureVector MLEngine::extractProcessFeatures(pid_t pid) {
    FeatureVector features;
    
#ifdef __linux__
    // Read /proc/[pid]/stat for CPU usage
    std::string statPath = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream statFile(statPath);
    if (statFile) {
        std::string line;
        std::getline(statFile, line);
        // Parse stat file for CPU time
        // Simplified - would need proper parsing
    }
    
    // Read /proc/[pid]/status for memory
    std::string statusPath = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream statusFile(statusPath);
    if (statusFile) {
        std::string line;
        while (std::getline(statusFile, line)) {
            if (line.find("VmRSS:") == 0) {
                // Parse memory usage
                size_t pos = line.find_first_of("0123456789");
                if (pos != std::string::npos) {
                    features.memoryUsage = std::stod(line.substr(pos)) * 1024; // Convert to bytes
                }
            }
        }
    }
    
    // Count child processes
    std::string taskPath = "/proc/" + std::to_string(pid) + "/task";
    if (std::filesystem::exists(taskPath)) {
        features.childProcessCount = std::distance(
            std::filesystem::directory_iterator(taskPath),
            std::filesystem::directory_iterator{}
        );
    }
#endif
    
    return features;
}

// Training status for async operations
static std::atomic<bool> s_isTraining{false};
static std::atomic<int> s_filesProcessed{0};
static std::atomic<int> s_totalFiles{0};
static std::string s_currentFile;
static std::mutex s_statusMutex;

int MLEngine::trainFromDirectory(const std::string& directoryPath, 
                                  bool recursive,
                                  std::function<void(int, int, const std::string&)> progressCallback) {
    if (!impl_->initialized) {
        return 0;
    }
    
    if (!std::filesystem::exists(directoryPath)) {
        return 0;
    }
    
    // Collect all files first
    std::vector<std::string> files;
    
    auto collectFiles = [&](const std::filesystem::path& dir) {
        try {
            if (recursive) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
                    if (entry.is_regular_file()) {
                        // Skip very large files (>100MB) and hidden files
                        if (entry.file_size() < 100 * 1024 * 1024 && 
                            entry.path().filename().string()[0] != '.') {
                            files.push_back(entry.path().string());
                        }
                    }
                }
            } else {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.is_regular_file()) {
                        if (entry.file_size() < 100 * 1024 * 1024 &&
                            entry.path().filename().string()[0] != '.') {
                            files.push_back(entry.path().string());
                        }
                    }
                }
            }
        } catch (const std::exception&) {
            // Skip directories we can't access
        }
    };
    
    collectFiles(directoryPath);
    
    if (files.empty()) {
        return 0;
    }
    
    // Set training status
    s_isTraining = true;
    s_totalFiles = static_cast<int>(files.size());
    s_filesProcessed = 0;
    
    std::vector<std::vector<double>> trainingFeatures;
    trainingFeatures.reserve(files.size());
    
    // Extract features from each file
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& filePath = files[i];
        
        {
            std::lock_guard<std::mutex> lock(s_statusMutex);
            s_currentFile = filePath;
        }
        
        try {
            auto features = extractFileFeatures(filePath);
            
            // Only add valid features (non-empty files)
            if (features.fileSize > 0) {
                trainingFeatures.push_back(features.toVector());
                
                // Update statistical model with this file's features
                {
                    std::lock_guard<std::mutex> lock(impl_->mutex);
                    impl_->statisticalModel->update("entropy", features.entropy);
                    impl_->statisticalModel->update("file_size", features.fileSize);
                    impl_->statisticalModel->update("ascii_ratio", features.asciiRatio);
                    impl_->statisticalModel->update("printable_ratio", features.printableRatio);
                }
            }
        } catch (const std::exception&) {
            // Skip files we can't process
        }
        
        s_filesProcessed = static_cast<int>(i + 1);
        
        if (progressCallback) {
            progressCallback(s_filesProcessed, s_totalFiles, filePath);
        }
    }
    
    // Train models with collected features
    if (trainingFeatures.size() >= 10) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        
        // Add to existing training data
        impl_->trainingData.insert(impl_->trainingData.end(), 
                                    trainingFeatures.begin(), 
                                    trainingFeatures.end());
        
        // Limit training data size (keep most recent)
        const size_t maxTrainingSize = 10000;
        if (impl_->trainingData.size() > maxTrainingSize) {
            impl_->trainingData.erase(
                impl_->trainingData.begin(),
                impl_->trainingData.begin() + (impl_->trainingData.size() - maxTrainingSize)
            );
        }
        
        // Train Isolation Forest for anomaly detection
        impl_->isolationForest->fit(impl_->trainingData);
        
        // Train K-Means clustering for behavior grouping
        impl_->clustering->fit(impl_->trainingData);
        
        impl_->stats.lastUpdate = std::chrono::steady_clock::now();
    }
    
    s_isTraining = false;
    
    return static_cast<int>(trainingFeatures.size());
}

MLEngine::TrainingStatus MLEngine::getTrainingStatus() const {
    TrainingStatus status;
    status.isTraining = s_isTraining;
    status.filesProcessed = s_filesProcessed;
    status.totalFiles = s_totalFiles;
    status.progress = s_totalFiles > 0 ? 
        static_cast<double>(s_filesProcessed) / s_totalFiles : 0.0;
    
    {
        std::lock_guard<std::mutex> lock(s_statusMutex);
        status.currentFile = s_currentFile;
    }
    
    return status;
}

} // namespace Zer0
} // namespace SentinelFS
