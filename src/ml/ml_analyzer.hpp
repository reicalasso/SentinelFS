#pragma once

#include <string>
#include <vector>
#include <memory>

class MLAnalyzer {
public:
    MLAnalyzer();
    ~MLAnalyzer();
    
    // Check for anomalies in file access patterns
    static bool checkAnomaly(const std::string& logFile);
    
    // Extract features from file access for ML analysis
    static std::vector<float> extractFeatures(const std::string& filePath, int peerId = 0, size_t fileSize = 0);
    
    // Detect anomalies in real-time from file events
    bool detectAnomaly(const std::vector<float>& features);
    
    // Train the model (if needed)
    bool trainModel(const std::string& dataFile);
    
private:
    // Internal state for the ML analyzer
    bool modelLoaded;
    std::string modelPath;
    
    // Get current time features
    static float getCurrentHour();
    static float getCurrentDayOfWeek();
};

#ifdef USE_ONNX
#include <onnxruntime_cxx_api.h>

class ONNXAnalyzer {
private:
    Ort::Env env;
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    bool modelLoaded;

public:
    ONNXAnalyzer(const std::string& modelPath = "models/file_access_model.onnx");
    ~ONNXAnalyzer();
    
    bool predict(const std::vector<float>& features);
    std::vector<bool> predictBatch(const std::vector<std::vector<float>>& features_batch);
    
    bool isModelLoaded() const { return modelLoaded; }
};
#endif