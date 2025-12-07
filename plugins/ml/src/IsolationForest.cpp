#include "IsolationForest.h"
#include <ctime>
#include <set>
#include <map>
#include <filesystem>
#include <limits>

namespace SentinelFS {

// c(n) - Average path length of unsuccessful search in a Binary Search Tree
double IsolationForest::c(size_t n) {
    if (n <= 1) return 0.0;
    if (n == 2) return 1.0;
    
    // H(n-1) = ln(n-1) + Euler-Mascheroni constant
    double H = std::log(static_cast<double>(n - 1)) + 0.5772156649;
    return 2.0 * H - (2.0 * (n - 1.0) / n);
}

// ITree implementation
void IsolationForest::ITree::build(const std::vector<std::vector<double>>& samples,
                                     int heightLimit, std::mt19937& rng) {
    root = buildNode(samples, 0, heightLimit, rng);
}

std::unique_ptr<IsolationForest::ITreeNode> IsolationForest::ITree::buildNode(
    const std::vector<std::vector<double>>& samples,
    int currentHeight, int heightLimit, std::mt19937& rng) {
    
    auto node = std::make_unique<ITreeNode>();
    
    // External node conditions
    if (currentHeight >= heightLimit || samples.size() <= 1) {
        node->isExternal = true;
        node->size = samples.size();
        return node;
    }
    
    size_t numFeatures = samples[0].size();
    if (numFeatures == 0) {
        node->isExternal = true;
        node->size = samples.size();
        return node;
    }
    
    // Randomly select attribute
    std::uniform_int_distribution<size_t> attrDist(0, numFeatures - 1);
    node->splitAttribute = attrDist(rng);
    
    // Find min and max for selected attribute
    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();
    
    for (const auto& sample : samples) {
        if (sample[node->splitAttribute] < minVal) {
            minVal = sample[node->splitAttribute];
        }
        if (sample[node->splitAttribute] > maxVal) {
            maxVal = sample[node->splitAttribute];
        }
    }
    
    // If all values are the same, make external node
    if (std::abs(maxVal - minVal) < 1e-10) {
        node->isExternal = true;
        node->size = samples.size();
        return node;
    }
    
    // Randomly select split value
    std::uniform_real_distribution<double> splitDist(minVal, maxVal);
    node->splitValue = splitDist(rng);
    
    // Split samples
    std::vector<std::vector<double>> leftSamples, rightSamples;
    for (const auto& sample : samples) {
        if (sample[node->splitAttribute] < node->splitValue) {
            leftSamples.push_back(sample);
        } else {
            rightSamples.push_back(sample);
        }
    }
    
    // Build children
    node->left = buildNode(leftSamples, currentHeight + 1, heightLimit, rng);
    node->right = buildNode(rightSamples, currentHeight + 1, heightLimit, rng);
    
    return node;
}

double IsolationForest::ITree::pathLength(const std::vector<double>& sample) const {
    if (!root) return 0.0;
    return pathLengthRecursive(root.get(), sample, 0);
}

double IsolationForest::ITree::pathLengthRecursive(const ITreeNode* node,
                                                     const std::vector<double>& sample,
                                                     int currentLength) const {
    if (!node || node->isExternal) {
        // Add average path length for remaining samples
        double adjustment = (node && node->size > 1) ? c(node->size) : 0.0;
        return static_cast<double>(currentLength) + adjustment;
    }
    
    if (node->splitAttribute >= sample.size()) {
        return static_cast<double>(currentLength);
    }
    
    if (sample[node->splitAttribute] < node->splitValue) {
        return pathLengthRecursive(node->left.get(), sample, currentLength + 1);
    } else {
        return pathLengthRecursive(node->right.get(), sample, currentLength + 1);
    }
}

// IsolationForest implementation
IsolationForest::IsolationForest() : config_(Config{}) {
    trees_.reserve(config_.numTrees);
}

IsolationForest::IsolationForest(const Config& config) : config_(config) {
    trees_.reserve(config_.numTrees);
}

IsolationForest::~IsolationForest() = default;

void IsolationForest::fit(const std::vector<std::vector<double>>& samples) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (samples.empty()) return;
    
    numFeatures_ = samples[0].size();
    int heightLimit = static_cast<int>(std::ceil(std::log2(config_.sampleSize)));
    
    std::mt19937 rng(config_.randomSeed);
    
    trees_.clear();
    trees_.reserve(config_.numTrees);
    
    for (int i = 0; i < config_.numTrees; ++i) {
        // Subsample
        std::vector<std::vector<double>> subsample;
        size_t sampleSize = std::min(static_cast<size_t>(config_.sampleSize), samples.size());
        
        std::vector<size_t> indices(samples.size());
        for (size_t j = 0; j < indices.size(); ++j) indices[j] = j;
        std::shuffle(indices.begin(), indices.end(), rng);
        
        for (size_t j = 0; j < sampleSize; ++j) {
            subsample.push_back(samples[indices[j]]);
        }
        
        // Build tree
        auto tree = std::make_unique<ITree>();
        tree->build(subsample, heightLimit, rng);
        trees_.push_back(std::move(tree));
    }
    
    // Calculate threshold based on contamination rate
    // Score all samples and find the percentile
    std::vector<double> scores;
    scores.reserve(samples.size());
    for (const auto& sample : samples) {
        scores.push_back(predict(sample));
    }
    
    std::sort(scores.begin(), scores.end());
    size_t thresholdIdx = static_cast<size_t>((1.0 - config_.contaminationRate) * scores.size());
    if (thresholdIdx >= scores.size()) thresholdIdx = scores.size() - 1;
    threshold_ = scores[thresholdIdx];
    
    trained_ = true;
}

double IsolationForest::predict(const std::vector<double>& sample) const {
    if (!trained_ || trees_.empty()) return 0.0;
    
    double avgPath = averagePathLength(sample);
    double cn = c(config_.sampleSize);
    
    if (cn == 0.0) return 0.5;
    
    // Anomaly score: s(x,n) = 2^(-E(h(x))/c(n))
    return std::pow(2.0, -avgPath / cn);
}

std::vector<double> IsolationForest::predictBatch(const std::vector<std::vector<double>>& samples) const {
    std::vector<double> scores;
    scores.reserve(samples.size());
    
    for (const auto& sample : samples) {
        scores.push_back(predict(sample));
    }
    
    return scores;
}

bool IsolationForest::isAnomaly(const std::vector<double>& sample) const {
    return predict(sample) > threshold_;
}

double IsolationForest::averagePathLength(const std::vector<double>& sample) const {
    if (trees_.empty()) return 0.0;
    
    double totalPath = 0.0;
    for (const auto& tree : trees_) {
        totalPath += tree->pathLength(sample);
    }
    
    return totalPath / static_cast<double>(trees_.size());
}

// FeatureExtractor implementation
FeatureExtractor::ActivityFeatures FeatureExtractor::extractFeatures(
    const std::vector<std::tuple<std::string, std::string, size_t, double>>& events) {
    
    ActivityFeatures features;
    
    if (events.empty()) return features;
    
    int creates = 0, modifies = 0, deletes = 0;
    size_t totalSize = 0;
    double totalEntropy = 0.0;
    std::set<std::string> uniqueDirs;
    std::set<std::string> uniqueExtensions;
    
    for (const auto& [action, path, size, entropy] : events) {
        if (action == "CREATE") creates++;
        else if (action == "MODIFY") modifies++;
        else if (action == "DELETE") deletes++;
        
        totalSize += size;
        totalEntropy += entropy;
        
        std::filesystem::path p(path);
        uniqueDirs.insert(p.parent_path().string());
        if (p.has_extension()) {
            uniqueExtensions.insert(p.extension().string());
        }
    }
    
    int total = static_cast<int>(events.size());
    
    // Calculate features
    features.activityRate = static_cast<double>(total);  // Will be normalized later
    features.createRatio = static_cast<double>(creates) / total;
    features.modifyRatio = static_cast<double>(modifies) / total;
    features.deleteRatio = static_cast<double>(deletes) / total;
    features.avgFileSize = (total > 0) ? static_cast<double>(totalSize) / total : 0.0;
    features.avgEntropy = (total > 0) ? totalEntropy / total : 0.0;
    features.uniqueDirs = static_cast<double>(uniqueDirs.size());
    features.extensionDiversity = (total > 0) ? 
        static_cast<double>(uniqueExtensions.size()) / total : 0.0;
    
    // Time features
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    features.hourOfDay = static_cast<double>(tm->tm_hour) / 24.0;
    features.dayOfWeek = static_cast<double>(tm->tm_wday) / 7.0;
    
    return features;
}

std::vector<double> FeatureExtractor::normalize(const std::vector<double>& features,
                                                  const std::vector<double>& mins,
                                                  const std::vector<double>& maxs) {
    std::vector<double> normalized;
    normalized.reserve(features.size());
    
    for (size_t i = 0; i < features.size(); ++i) {
        double min = (i < mins.size()) ? mins[i] : 0.0;
        double max = (i < maxs.size()) ? maxs[i] : 1.0;
        
        if (std::abs(max - min) < 1e-10) {
            normalized.push_back(0.5);
        } else {
            double val = (features[i] - min) / (max - min);
            normalized.push_back(std::clamp(val, 0.0, 1.0));
        }
    }
    
    return normalized;
}

} // namespace SentinelFS
