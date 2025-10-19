#include "federated_learning.hpp"
#include <algorithm>
#include <random>
#include <iostream>
#include <sstream>
#include <cmath>
#include <cstring>

// FederatedLearning Implementation
FederatedLearning::FederatedLearning(const FederatedConfig& config) 
    : config(config), roundsCompleted(0), averageAccuracy(0.0) {
    startTime = std::chrono::system_clock::now();
}

bool FederatedLearning::initialize(NeuralNetwork& localModel) {
    this->localModel = std::make_unique<NeuralNetwork>();
    // In a real implementation, we would copy the model structure
    // For now, we'll just store a reference to indicate initialization
    return true;
}

void FederatedLearning::addPeer(const FederatedPeer& peer) {
    std::lock_guard<std::mutex> lock(peersMutex);
    // Check if peer already exists
    auto it = std::find_if(peers.begin(), peers.end(), 
                         [&peer](const FederatedPeer& p) { return p.id == peer.id; });
    
    if (it == peers.end()) {
        peers.push_back(peer);
    } else {
        *it = peer;  // Update existing peer
    }
}

void FederatedLearning::removePeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(peersMutex);
    peers.erase(std::remove_if(peers.begin(), peers.end(),
                               [&peerId](const FederatedPeer& p) { return p.id == peerId; }),
                peers.end());
}

void FederatedLearning::startFederatedLearning() {
    if (running.exchange(true)) {
        return;  // Already running
    }
    
    flThread = std::thread(&FederatedLearning::federatedLearningLoop, this);
}

void FederatedLearning::stopFederatedLearning() {
    if (running.exchange(false)) {
        if (flThread.joinable()) {
            flThread.join();
        }
    }
}

ModelUpdate FederatedLearning::createLocalUpdate(const std::vector<std::vector<double>>& trainingData,
                                                 const std::vector<std::vector<double>>& trainingLabels) {
    ModelUpdate update;
    
    // In a real implementation, we would:
    // 1. Train the local model on the provided data
    // 2. Extract the updated weights and biases
    // 3. Package them into a ModelUpdate structure
    
    // For this example, we'll create a mock update
    update.dataSize = trainingData.size();
    update.timestamp = std::chrono::system_clock::now();
    
    // Mock weights and biases (in a real implementation, these would come from the trained model)
    if (!peers.empty()) {
        update.peerId = peers[0].id;  // Use first peer ID for demo
    }
    
    // Add some mock weight data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    
    // Create mock layer weights and biases
    update.layerWeights.push_back({{{dist(gen), dist(gen)}, {dist(gen), dist(gen)}}});
    update.layerBiases.push_back({dist(gen), dist(gen)});
    
    // Apply differential privacy if enabled
    if (config.secureAggregation) {
        update = addDifferentialPrivacy(update);
    }
    
    // Call callback if registered
    if (updateCallback) {
        updateCallback(update);
    }
    
    return update;
}

AggregatedUpdate FederatedLearning::aggregateUpdates(const std::vector<ModelUpdate>& updates) {
    AggregatedUpdate aggregated;
    
    if (updates.empty()) {
        return aggregated;
    }
    
    // Federated Averaging (FedAvg)
    aggregated.numParticipants = updates.size();
    
    // Initialize with first update
    aggregated.averagedWeights = updates[0].layerWeights;
    aggregated.averagedBiases = updates[0].layerBiases;
    aggregated.totalDataSize = updates[0].dataSize;
    
    // Sum all updates (weighted by data size)
    for (size_t i = 1; i < updates.size(); ++i) {
        double weight1 = static_cast<double>(aggregated.totalDataSize) /
                        static_cast<double>(aggregated.totalDataSize + updates[i].dataSize);
        double weight2 = static_cast<double>(updates[i].dataSize) /
                        static_cast<double>(aggregated.totalDataSize + updates[i].dataSize);
        
        // Weighted average of weights
        for (size_t layer = 0; layer < aggregated.averagedWeights.size() && 
                                 layer < updates[i].layerWeights.size(); ++layer) {
            for (size_t out = 0; layer < aggregated.averagedWeights[layer].size() && 
                                   layer < updates[i].layerWeights[layer].size(); ++out) {
                for (size_t in = 0; in < aggregated.averagedWeights[layer][out].size() && 
                                   in < updates[i].layerWeights[layer][out].size(); ++in) {
                    aggregated.averagedWeights[layer][out][in] = 
                        weight1 * aggregated.averagedWeights[layer][out][in] +
                        weight2 * updates[i].layerWeights[layer][out][in];
                }
            }
        }
        
        // Weighted average of biases
        for (size_t layer = 0; layer < aggregated.averagedBiases.size() && 
                                 layer < updates[i].layerBiases.size(); ++layer) {
            for (size_t b = 0; b < aggregated.averagedBiases[layer].size() && 
                             b < updates[i].layerBiases[layer].size(); ++b) {
                aggregated.averagedBiases[layer][b] = 
                    weight1 * aggregated.averagedBiases[layer][b] +
                    weight2 * updates[i].layerBiases[layer][b];
            }
        }
        
        aggregated.totalDataSize += updates[i].dataSize;
    }
    
    // Call aggregation callback if registered
    if (aggregationCallback) {
        aggregationCallback(aggregated);
    }
    
    return aggregated;
}

bool FederatedLearning::applyAggregatedUpdate(NeuralNetwork& localModel, const AggregatedUpdate& aggregated) {
    // In a real implementation, we would:
    // 1. Apply the aggregated weights and biases to the local model
    // 2. Update the model parameters
    // 3. Return success/failure
    
    // For this example, we'll just acknowledge success
    std::cout << "Applied federated update from " << aggregated.numParticipants 
              << " participants with total data size " << aggregated.totalDataSize << std::endl;
    
    return true;
}

std::string FederatedLearning::serializeUpdate(const ModelUpdate& update) {
    std::stringstream ss;
    
    // Serialize basic fields
    ss << update.peerId << "|" << update.dataSize << "|";
    
    // Serialize weights
    for (const auto& layerWeights : update.layerWeights) {
        for (const auto& outputWeights : layerWeights) {
            for (const auto& weight : outputWeights) {
                ss << weight << ",";
            }
            ss << ";";
        }
        ss << "|";
    }
    
    // Serialize biases
    for (const auto& layerBiases : update.layerBiases) {
        for (const auto& bias : layerBiases) {
            ss << bias << ",";
        }
        ss << "|";
    }
    
    return ss.str();
}

ModelUpdate FederatedLearning::deserializeUpdate(const std::string& serialized) {
    ModelUpdate update;
    
    std::stringstream ss(serialized);
    std::string token;
    
    // Deserialize basic fields (simplified for example)
    if (std::getline(ss, token, '|')) {
        update.peerId = token;
    }
    
    // In a real implementation, we would fully deserialize all fields
    // This is a simplified example
    
    return update;
}

ModelUpdate FederatedLearning::addDifferentialPrivacy(ModelUpdate update, double epsilon) {
    // Add noise to weights and biases for differential privacy
    std::random_device rd;
    std::mt19937 gen(rd());
    std::exponential_distribution<double> noiseDist(epsilon);
    
    // Add noise to weights
    for (auto& layerWeights : update.layerWeights) {
        for (auto& outputWeights : layerWeights) {
            for (auto& weight : outputWeights) {
                weight += noiseDist(gen) * 0.01;  // Scale noise
            }
        }
    }
    
    // Add noise to biases
    for (auto& layerBiases : update.layerBiases) {
        for (auto& bias : layerBiases) {
            bias += noiseDist(gen) * 0.01;  // Scale noise
        }
    }
    
    return update;
}

std::vector<FederatedPeer> FederatedLearning::selectPeersForRound() {
    std::lock_guard<std::mutex> lock(peersMutex);
    
    if (peers.empty()) {
        return {};
    }
    
    // Select a subset of peers based on reliability and participation
    std::vector<FederatedPeer> candidates;
    for (const auto& peer : peers) {
        if (peer.participating && peer.reliability > 0.5) {  // Only reliable peers
            candidates.push_back(peer);
        }
    }
    
    // Shuffle and select subset
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(candidates.begin(), candidates.end(), gen);
    
    size_t numToSelect = std::min(static_cast<size_t>(candidates.size()),
                                 static_cast<size_t>(config.maxPeers));
    
    candidates.resize(numToSelect);
    return candidates;
}

std::map<std::string, double> FederatedLearning::getStatistics() const {
    std::map<std::string, double> stats;
    
    stats["rounds_completed"] = static_cast<double>(roundsCompleted);
    stats["average_accuracy"] = averageAccuracy;
    stats["peers_count"] = static_cast<double>(peers.size());
    
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::hours>(now - startTime).count();
    stats["uptime_hours"] = static_cast<double>(duration);
    
    return stats;
}

void FederatedLearning::federatedLearningLoop() {
    while (running.load()) {
        // Select peers for this round
        auto selectedPeers = selectPeersForRound();
        
        if (!selectedPeers.empty()) {
            std::cout << "Starting federated learning round with " 
                      << selectedPeers.size() << " peers" << std::endl;
            
            // Collect updates from peers
            auto updates = collectPeerUpdates();
            
            if (!updates.empty()) {
                // Aggregate updates
                auto aggregated = aggregateUpdates(updates);
                
                // Apply to local model (in a real implementation)
                if (localModel && !aggregated.averagedWeights.empty()) {
                    applyAggregatedUpdate(*localModel, aggregated);
                }
                
                roundsCompleted++;
            }
        }
        
        // Wait before next round
        std::this_thread::sleep_for(std::chrono::seconds(30));  // 30-second intervals
    }
}

std::vector<ModelUpdate> FederatedLearning::collectPeerUpdates() {
    std::vector<ModelUpdate> updates;
    
    // In a real implementation, this would:
    // 1. Contact selected peers
    // 2. Request their model updates
    // 3. Collect and validate updates
    
    // For this example, return mock updates
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> countDist(1, 5);
    
    int numUpdates = countDist(gen);
    for (int i = 0; i < numUpdates; ++i) {
        ModelUpdate update;
        update.peerId = "peer_" + std::to_string(i);
        update.dataSize = 100 + i * 50;
        updates.push_back(update);
    }
    
    return updates;
}

void FederatedLearning::updatePeerReliability(const std::string& peerId, bool successful) {
    std::lock_guard<std::mutex> lock(peersMutex);
    
    auto it = std::find_if(peers.begin(), peers.end(),
                         [&peerId](const FederatedPeer& p) { return p.id == peerId; });
    
    if (it != peers.end()) {
        if (successful) {
            it->reliability = std::min(1.0, it->reliability + 0.1);
        } else {
            it->reliability = std::max(0.0, it->reliability - 0.2);
        }
        it->lastActive = std::chrono::system_clock::now();
    }
}

// FederatedAnomalyDetector Implementation
FederatedAnomalyDetector::FederatedAnomalyDetector(const FederatedConfig& config) {
    flManager = std::make_unique<FederatedLearning>(config);
    anomalyModel = std::make_unique<NeuralNetwork>();
}

bool FederatedAnomalyDetector::initialize() {
    return flManager->initialize(*anomalyModel);
}

ModelUpdate FederatedAnomalyDetector::createAnomalyDetectionUpdate(
    const std::vector<std::vector<float>>& features,
    const std::vector<int>& labels) {
    
    // Convert features to double precision
    std::vector<std::vector<double>> doubleFeatures;
    std::vector<std::vector<double>> doubleLabels;
    
    for (const auto& featureRow : features) {
        std::vector<double> doubleRow(featureRow.begin(), featureRow.end());
        doubleFeatures.push_back(doubleRow);
    }
    
    for (int label : labels) {
        doubleLabels.push_back({static_cast<double>(label)});
    }
    
    // Create local update
    return flManager->createLocalUpdate(doubleFeatures, doubleLabels);
}

bool FederatedAnomalyDetector::applyFederatedUpdate(const AggregatedUpdate& update) {
    return flManager->applyAggregatedUpdate(*anomalyModel, update);
}

std::vector<double> FederatedAnomalyDetector::detectAnomalies(
    const std::vector<std::vector<float>>& features) {
    
    // Normalize features
    auto normalizedFeatures = features;
    normalizeFeatures(normalizedFeatures);
    
    // Convert to double
    auto doubleFeatures = convertFeatures(normalizedFeatures);
    
    // In a real implementation, we would run inference on the federated model
    // For this example, return mock probabilities
    std::vector<double> probabilities(features.size());
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (auto& prob : probabilities) {
        prob = dist(gen);
    }
    
    return probabilities;
}

void FederatedAnomalyDetector::normalizeFeatures(std::vector<std::vector<float>>& features) {
    if (features.empty()) return;
    
    size_t numFeatures = features[0].size();
    
    // Initialize if needed
    if (featureMeans.empty()) {
        featureMeans.assign(numFeatures, 0.0);
        featureStdDevs.assign(numFeatures, 1.0);
    }
    
    // Calculate means
    std::vector<double> sums(numFeatures, 0.0);
    for (const auto& row : features) {
        for (size_t i = 0; i < numFeatures && i < row.size(); ++i) {
            sums[i] += static_cast<double>(row[i]);
        }
    }
    
    for (size_t i = 0; i < numFeatures; ++i) {
        featureMeans[i] = sums[i] / features.size();
    }
    
    // Calculate standard deviations
    std::vector<double> sumSquares(numFeatures, 0.0);
    for (const auto& row : features) {
        for (size_t i = 0; i < numFeatures && i < row.size(); ++i) {
            double diff = static_cast<double>(row[i]) - featureMeans[i];
            sumSquares[i] += diff * diff;
        }
    }
    
    for (size_t i = 0; i < numFeatures; ++i) {
        featureStdDevs[i] = std::sqrt(sumSquares[i] / features.size());
        if (featureStdDevs[i] == 0.0) {
            featureStdDevs[i] = 1.0;  // Avoid division by zero
        }
    }
    
    // Normalize features
    for (auto& row : features) {
        for (size_t i = 0; i < numFeatures && i < row.size(); ++i) {
            row[i] = static_cast<float>((static_cast<double>(row[i]) - featureMeans[i]) / featureStdDevs[i]);
        }
    }
}

std::vector<std::vector<double>> FederatedAnomalyDetector::convertFeatures(
    const std::vector<std::vector<float>>& features) {
    
    std::vector<std::vector<double>> doubleFeatures;
    for (const auto& featureRow : features) {
        std::vector<double> doubleRow(featureRow.begin(), featureRow.end());
        doubleFeatures.push_back(doubleRow);
    }
    
    return doubleFeatures;
}

// FederatedPredictionModel Implementation
FederatedPredictionModel::FederatedPredictionModel(const FederatedConfig& config) {
    flManager = std::make_unique<FederatedLearning>(config);
    predictionModel = std::make_unique<NeuralNetwork>();
}

bool FederatedPredictionModel::initialize() {
    return flManager->initialize(*predictionModel);
}

ModelUpdate FederatedPredictionModel::createPredictionUpdate(
    const std::vector<std::vector<float>>& features,
    const std::vector<std::vector<float>>& targets) {
    
    // Convert features and targets to double
    auto doubleFeatures = processFeatures(features);
    std::vector<std::vector<double>> doubleTargets;
    
    for (const auto& targetRow : targets) {
        std::vector<double> doubleRow(targetRow.begin(), targetRow.end());
        doubleTargets.push_back(doubleRow);
    }
    
    // Create local update
    return flManager->createLocalUpdate(doubleFeatures, doubleTargets);
}

bool FederatedPredictionModel::applyFederatedUpdate(const AggregatedUpdate& update) {
    return flManager->applyAggregatedUpdate(*predictionModel, update);
}

std::vector<std::vector<double>> FederatedPredictionModel::predict(
    const std::vector<std::vector<float>>& features) {
    
    auto processedFeatures = processFeatures(features);
    
    // In a real implementation, we would run inference on the federated model
    // For this example, return mock predictions
    std::vector<std::vector<double>> predictions(features.size());
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (auto& pred : predictions) {
        pred = {dist(gen)};  // Single probability output
    }
    
    return predictions;
}

std::vector<std::vector<double>> FederatedPredictionModel::processFeatures(
    const std::vector<std::vector<float>>& features) {
    
    std::vector<std::vector<double>> doubleFeatures;
    for (const auto& featureRow : features) {
        std::vector<double> doubleRow(featureRow.begin(), featureRow.end());
        doubleFeatures.push_back(doubleRow);
    }
    
    return doubleFeatures;
}