#pragma once

#include <vector>
#include <memory>
#include <string>
#include <random>
#include <cmath>
#include <fstream>
#include <iostream>

// Activation functions
namespace ActivationFunctions {
    inline double sigmoid(double x) {
        return 1.0 / (1.0 + std::exp(-x));
    }
    
    inline double sigmoidDerivative(double x) {
        double s = sigmoid(x);
        return s * (1.0 - s);
    }
    
    inline double relu(double x) {
        return std::max(0.0, x);
    }
    
    inline double reluDerivative(double x) {
        return x > 0 ? 1.0 : 0.0;
    }
    
    inline double tanhDerivative(double x) {
        double t = std::tanh(x);
        return 1.0 - t * t;
    }
}

// Neural Network Layer
class NeuralLayer {
public:
    NeuralLayer(size_t inputSize, size_t outputSize, const std::string& activation = "relu");
    ~NeuralLayer() = default;
    
    // Forward propagation
    std::vector<double> forward(const std::vector<double>& input);
    
    // Backward propagation (for training)
    std::vector<double> backward(const std::vector<double>& gradOutput, double learningRate);
    
    // Getters
    const std::vector<std::vector<double>>& getWeights() const { return weights; }
    const std::vector<double>& getBiases() const { return biases; }
    
    // Setters
    void setWeights(const std::vector<std::vector<double>>& newWeights) { weights = newWeights; }
    void setBiases(const std::vector<double>& newBiases) { biases = newBiases; }
    
private:
    size_t inputSize;
    size_t outputSize;
    std::string activationFunction;
    
    // Layer parameters
    std::vector<std::vector<double>> weights;  // [outputSize][inputSize]
    std::vector<double> biases;                // [outputSize]
    
    // For backpropagation
    std::vector<double> lastInput;
    std::vector<double> lastOutput;
    
    // Random number generator
    std::mt19937 rng;
    std::normal_distribution<double> weightDist;
    
    // Initialize weights and biases
    void initializeParameters();
    
    // Activation function and derivative
    double activate(double x) const;
    double activateDerivative(double x) const;
};

// Simple Neural Network
class NeuralNetwork {
public:
    NeuralNetwork();
    ~NeuralNetwork() = default;
    
    // Add layer to network
    void addLayer(size_t inputSize, size_t outputSize, const std::string& activation = "relu");
    
    // Forward propagation through all layers
    std::vector<double> forward(const std::vector<double>& input);
    
    // Train network using backpropagation
    void train(const std::vector<std::vector<double>>& inputs, 
               const std::vector<std::vector<double>>& targets,
               size_t epochs = 1000, 
               double learningRate = 0.01);
    
    // Predict single input
    std::vector<double> predict(const std::vector<double>& input);
    
    // Calculate loss (Mean Squared Error)
    double calculateLoss(const std::vector<double>& predicted, const std::vector<double>& actual);
    
    // Save/load model
    bool saveModel(const std::string& filepath);
    bool loadModel(const std::string& filepath);
    
    // Get network information
    size_t getNumLayers() const { return layers.size(); }
    size_t getInputSize() const { return inputDimension; }
    size_t getOutputSize() const { return outputDimension; }
    
private:
    std::vector<std::unique_ptr<NeuralLayer>> layers;
    size_t inputDimension;
    size_t outputDimension;
    
    // Helper functions
    std::vector<double> calculateGradient(const std::vector<double>& predicted, 
                                         const std::vector<double>& actual);
};

// LSTM Cell for sequence modeling
class LSTMCell {
public:
    LSTMCell(size_t inputSize, size_t hiddenSize);
    ~LSTMCell() = default;
    
    // Forward pass
    std::pair<std::vector<double>, std::vector<double>> forward(
        const std::vector<double>& input, 
        const std::vector<double>& prevState,
        const std::vector<double>& prevHidden);
    
    // Initialize cell state and hidden state
    std::vector<double> initializeState(size_t size);
    
private:
    size_t inputSize;
    size_t hiddenSize;
    
    // LSTM gates weights and biases
    std::vector<std::vector<double>> Wforget, Winput, Wcandidate, Woutput;
    std::vector<std::vector<double>> Uforget, Uinput, Ucandidate, Uoutput;
    std::vector<double> bforget, binput, bcandidate, boutput;
    
    // Random number generator
    std::mt19937 rng;
    std::normal_distribution<double> weightDist;
    
    // Initialize parameters
    void initializeParameters();
    
    // Sigmoid and Tanh functions
    std::vector<double> sigmoid(const std::vector<double>& x);
    std::vector<double> tanh(const std::vector<double>& x);
    
    // Matrix-vector multiplication
    std::vector<double> matVecMul(const std::vector<std::vector<double>>& matrix, 
                                 const std::vector<double>& vec);
    
    // Element-wise operations
    std::vector<double> elementWiseAdd(const std::vector<double>& a, const std::vector<double>& b);
    std::vector<double> elementWiseMultiply(const std::vector<double>& a, const std::vector<double>& b);
};

// Simple LSTM Network
class LstmNetwork {
public:
    LstmNetwork(size_t inputSize, size_t hiddenSize, size_t outputSize);
    ~LstmNetwork() = default;
    
    // Process sequence
    std::vector<std::vector<double>> forward(const std::vector<std::vector<double>>& sequence);
    
    // Predict next value in sequence
    std::vector<double> predictNext(const std::vector<std::vector<double>>& sequence);
    
private:
    size_t inputSize;
    size_t hiddenSize;
    size_t outputSize;
    
    std::unique_ptr<LSTMCell> lstmCell;
    std::unique_ptr<NeuralLayer> outputLayer;
    
    // Helper functions
    std::vector<double> sequenceToVector(const std::vector<std::vector<double>>& sequence);
};