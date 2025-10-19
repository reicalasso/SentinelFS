#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include "neural_network.hpp"
#include "../models.hpp"

// Federated Learning Configuration
struct FederatedConfig {
    double learningRate = 0.01;
    size_t numRounds = 100;
    size_t localEpochs = 5;
    double sampleFraction = 0.1;  // Fraction of peers to sample each round
    std::string aggregationMethod = "fedavg";  // Federated averaging
    bool secureAggregation = true;  // Use secure aggregation
    size_t maxPeers = 10;  // Maximum number of peers to aggregate from
    
    FederatedConfig() = default;
};

// Model Update Structure
struct ModelUpdate {
    std::vector<std::vector<std::vector<double>>> layerWeights;  // [layer][output][input]
    std::vector<std::vector<double>> layerBiases;                // [layer][bias]
    size_t dataSize;
    std::string peerId;
    std::chrono::system_clock::time_point timestamp;
    
    ModelUpdate() : dataSize(0), timestamp(std::chrono::system_clock::now()) {}
};

// Aggregated Model Update
struct AggregatedUpdate {
    std::vector<std::vector<std::vector<double>>> averagedWeights;
    std::vector<std::vector<double>> averagedBiases;
    size_t totalDataSize;
    size_t numParticipants;
    
    AggregatedUpdate() : totalDataSize(0), numParticipants(0) {}
};

// Peer Information for Federated Learning
struct FederatedPeer {
    std::string id;
    std::string address;
    int port;
    double reliability;  // Peer reliability score (0.0 - 1.0)
    std::chrono::system_clock::time_point lastActive;
    bool participating;  // Whether peer is participating in FL
    
    FederatedPeer() : port(0), reliability(1.0), participating(true) {}
    FederatedPeer(const std::string& peerId, const std::string& addr, int p) 
        : id(peerId), address(addr), port(p), reliability(1.0), 
          lastActive(std::chrono::system_clock::now()), participating(true) {}
};

// Federated Learning Manager
class FederatedLearning {
public:
    FederatedLearning(const FederatedConfig& config = FederatedConfig());
    ~FederatedLearning() = default;
    
    // Initialize federated learning with local model
    bool initialize(NeuralNetwork& localModel);
    
    // Add peer to federated learning network
    void addPeer(const FederatedPeer& peer);
    void removePeer(const std::string& peerId);
    
    // Start/stop federated learning rounds
    void startFederatedLearning();
    void stopFederatedLearning();
    
    // Participate in federated learning round
    ModelUpdate createLocalUpdate(const std::vector<std::vector<double>>& trainingData,
                                 const std::vector<std::vector<double>>& trainingLabels);
    
    // Aggregate model updates from peers
    AggregatedUpdate aggregateUpdates(const std::vector<ModelUpdate>& updates);
    
    // Apply aggregated update to local model
    bool applyAggregatedUpdate(NeuralNetwork& localModel, const AggregatedUpdate& aggregated);
    
    // Secure communication methods
    std::string serializeUpdate(const ModelUpdate& update);
    ModelUpdate deserializeUpdate(const std::string& serialized);
    
    // Differential privacy for secure aggregation
    ModelUpdate addDifferentialPrivacy(ModelUpdate update, double epsilon = 1.0);
    
    // Peer selection for efficient aggregation
    std::vector<FederatedPeer> selectPeersForRound();
    
    // Get federated learning statistics
    std::map<std::string, double> getStatistics() const;
    
    // Set/get configuration
    void setConfig(const FederatedConfig& config) { this->config = config; }
    FederatedConfig getConfig() const { return config; }
    
    // Callback registration for distributed learning
    using UpdateCallback = std::function<void(const ModelUpdate&)>;
    using AggregationCallback = std::function<void(const AggregatedUpdate&)>;
    
    void setUpdateCallback(UpdateCallback callback) { updateCallback = callback; }
    void setAggregationCallback(AggregationCallback callback) { aggregationCallback = callback; }
    
private:
    FederatedConfig config;
    std::vector<FederatedPeer> peers;
    mutable std::mutex peersMutex;
    
    bool running{false};
    mutable std::mutex runningMutex;  // Protect running state access
    std::thread flThread;
    
    // Model state
    std::unique_ptr<NeuralNetwork> localModel;
    
    // Performance metrics
    size_t roundsCompleted;
    double averageAccuracy;
    std::chrono::system_clock::time_point startTime;
    
    // Callbacks
    UpdateCallback updateCallback;
    AggregationCallback aggregationCallback;
    
    // Internal methods
    void federatedLearningLoop();
    std::vector<ModelUpdate> collectPeerUpdates();
    void updatePeerReliability(const std::string& peerId, bool successful);
    
    // Secure aggregation helpers
    std::vector<double> addNoise(const std::vector<double>& data, double epsilon);
    double calculateL2Norm(const std::vector<std::vector<double>>& weights);
    
    // Communication simulation
    bool sendUpdateToPeer(const ModelUpdate& update, const FederatedPeer& peer);
    ModelUpdate receiveUpdateFromPeer(const FederatedPeer& peer);
};

// Federated Anomaly Detector
class FederatedAnomalyDetector {
public:
    FederatedAnomalyDetector(const FederatedConfig& config = FederatedConfig());
    ~FederatedAnomalyDetector() = default;
    
    // Initialize with federated learning
    bool initialize();
    
    // Participate in federated anomaly detection
    ModelUpdate createAnomalyDetectionUpdate(const std::vector<std::vector<float>>& features,
                                            const std::vector<int>& labels);
    
    // Apply federated model update
    bool applyFederatedUpdate(const AggregatedUpdate& update);
    
    // Detect anomalies using federated model
    std::vector<double> detectAnomalies(const std::vector<std::vector<float>>& features);
    
    // Get federated learning manager
    FederatedLearning& getFederatedLearning() { return *flManager; }
    
private:
    std::unique_ptr<FederatedLearning> flManager;
    std::unique_ptr<NeuralNetwork> anomalyModel;
    
    // Feature scaling parameters
    std::vector<double> featureMeans;
    std::vector<double> featureStdDevs;
    
    // Internal methods
    void normalizeFeatures(std::vector<std::vector<float>>& features);
    std::vector<std::vector<double>> convertFeatures(const std::vector<std::vector<float>>& features);
};

// Federated Prediction Model
class FederatedPredictionModel {
public:
    FederatedPredictionModel(const FederatedConfig& config = FederatedConfig());
    ~FederatedPredictionModel() = default;
    
    // Initialize with federated learning
    bool initialize();
    
    // Participate in federated prediction model training
    ModelUpdate createPredictionUpdate(const std::vector<std::vector<float>>& features,
                                      const std::vector<std::vector<float>>& targets);
    
    // Apply federated model update
    bool applyFederatedUpdate(const AggregatedUpdate& update);
    
    // Predict file access patterns
    std::vector<std::vector<double>> predict(const std::vector<std::vector<float>>& features);
    
    // Get federated learning manager
    FederatedLearning& getFederatedLearning() { return *flManager; }
    
private:
    std::unique_ptr<FederatedLearning> flManager;
    std::unique_ptr<NeuralNetwork> predictionModel;
    
    // Sequence processing for time-series prediction
    std::unique_ptr<LstmNetwork> lstmModel;
    
    // Internal methods
    std::vector<std::vector<double>> processFeatures(const std::vector<std::vector<float>>& features);
};