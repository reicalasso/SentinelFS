#pragma once

#include <string>
#include <vector>
#include <map>
#include <ctime>

// Shared data structures used across modules
struct FileInfo {
    std::string path;
    std::string hash;
    std::string lastModified;
    size_t size;
    std::string deviceId;
    int version;                    // Version number for conflict resolution
    std::string conflictStatus;     // "none", "conflicted", "resolved"
    
    FileInfo() : size(0), version(1), conflictStatus("none") {}
    FileInfo(const std::string& p, const std::string& h, size_t s, const std::string& id) 
        : path(p), hash(h), size(s), deviceId(id), version(1), conflictStatus("none") {
        // Set current timestamp
        time_t now = time(0);
        lastModified = std::string(ctime(&now));
        // Remove newline from ctime
        lastModified.pop_back();
    }
};

struct PeerInfo {
    std::string id;
    std::string address;
    int port;
    double latency; // in milliseconds
    bool active;
    std::string lastSeen;
    
    PeerInfo() : port(0), latency(0.0), active(true) {}
    PeerInfo(const std::string& i, const std::string& addr, int p) 
        : id(i), address(addr), port(p), latency(0.0), active(true) {}
};

// ============================================================================
// ADVANCED ML DATA STRUCTURES
// ============================================================================

// Streaming Data Sample for Online Learning
struct StreamingSample {
    std::vector<double> features;
    std::vector<double> labels;
    long long timestamp;
    double weight;  // Sample importance weight
    std::string sourceId;  // Peer or source identifier
    
    StreamingSample() : timestamp(0), weight(1.0) {}
    StreamingSample(const std::vector<double>& f, const std::vector<double>& l, const std::string& src = "") 
        : features(f), labels(l), timestamp(0), weight(1.0), sourceId(src) {}
};

// Time Series Data for Forecasting
struct TimeSeriesData {
    std::vector<double> values;
    std::vector<long long> timestamps;
    std::string metric;  // Name of the metric being tracked
    
    TimeSeriesData() = default;
    TimeSeriesData(const std::string& m) : metric(m) {}
    
    void addPoint(double value, long long timestamp) {
        values.push_back(value);
        timestamps.push_back(timestamp);
    }
};

// Forecast Configuration
struct ForecastConfig {
    int horizon;           // How many steps to predict
    double confidence;     // Confidence level (0.0 to 1.0)
    int sequenceLength;    // How many past points to use
    std::string algorithm; // "ARIMA", "LSTM", "simple"
    
    ForecastConfig() : horizon(10), confidence(0.95), sequenceLength(50), algorithm("simple") {}
    ForecastConfig(int h, double c) : horizon(h), confidence(c), sequenceLength(50), algorithm("simple") {}
};

// ML Model Metadata
struct MLModelMetadata {
    std::string modelId;
    std::string modelType;  // "online", "federated", "forecasting", "neural"
    int version;
    double accuracy;
    long long lastTrainedTimestamp;
    size_t sampleCount;
    
    MLModelMetadata() : version(1), accuracy(0.0), lastTrainedTimestamp(0), sampleCount(0) {}
};