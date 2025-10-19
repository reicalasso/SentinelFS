#include "neural_network.hpp"
#include <algorithm>
#include <numeric>
#include <random>
#include <iostream>
#include <sstream>

// NeuralLayer Implementation
NeuralLayer::NeuralLayer(size_t inputSize, size_t outputSize, const std::string& activation)
    : inputSize(inputSize), outputSize(outputSize), activationFunction(activation),
      rng(std::random_device{}()), weightDist(0.0, 1.0) {
    initializeParameters();
}

void NeuralLayer::initializeParameters() {
    // Xavier initialization for weights
    double weightScale = std::sqrt(2.0 / static_cast<double>(inputSize));
    std::normal_distribution<double> initDist(0.0, weightScale);
    
    // Initialize weights matrix [outputSize][inputSize]
    weights.resize(outputSize, std::vector<double>(inputSize));
    for (size_t i = 0; i < outputSize; ++i) {
        for (size_t j = 0; j < inputSize; ++j) {
            weights[i][j] = initDist(rng);
        }
    }
    
    // Initialize biases [outputSize]
    biases.resize(outputSize, 0.0);
}

std::vector<double> NeuralLayer::forward(const std::vector<double>& input) {
    if (input.size() != inputSize) {
        throw std::invalid_argument("Input size mismatch");
    }
    
    lastInput = input;
    lastOutput.resize(outputSize, 0.0);
    
    // Compute weighted sum + bias for each neuron
    for (size_t i = 0; i < outputSize; ++i) {
        double sum = biases[i];
        for (size_t j = 0; j < inputSize; ++j) {
            sum += weights[i][j] * input[j];
        }
        lastOutput[i] = activate(sum);
    }
    
    return lastOutput;
}

std::vector<double> NeuralLayer::backward(const std::vector<double>& gradOutput, double learningRate) {
    if (gradOutput.size() != outputSize) {
        throw std::invalid_argument("Gradient output size mismatch");
    }
    
    std::vector<double> gradInput(inputSize, 0.0);
    
    // Compute gradients for weights, biases, and input
    for (size_t i = 0; i < outputSize; ++i) {
        double delta = gradOutput[i] * activateDerivative(lastOutput[i]);
        
        // Update biases
        biases[i] -= learningRate * delta;
        
        // Update weights and compute gradient for input
        for (size_t j = 0; j < inputSize; ++j) {
            weights[i][j] -= learningRate * delta * lastInput[j];
            gradInput[j] += delta * weights[i][j];
        }
    }
    
    return gradInput;
}

double NeuralLayer::activate(double x) const {
    if (activationFunction == "sigmoid") {
        return ActivationFunctions::sigmoid(x);
    } else if (activationFunction == "tanh") {
        return std::tanh(x);
    } else {  // Default to ReLU
        return ActivationFunctions::relu(x);
    }
}

double NeuralLayer::activateDerivative(double x) const {
    if (activationFunction == "sigmoid") {
        return ActivationFunctions::sigmoidDerivative(x);
    } else if (activationFunction == "tanh") {
        return ActivationFunctions::tanhDerivative(x);
    } else {  // Default to ReLU
        return ActivationFunctions::reluDerivative(x);
    }
}

// NeuralNetwork Implementation
NeuralNetwork::NeuralNetwork() : inputDimension(0), outputDimension(0) {}

void NeuralNetwork::addLayer(size_t inputSize, size_t outputSize, const std::string& activation) {
    layers.push_back(std::make_unique<NeuralLayer>(inputSize, outputSize, activation));
    
    if (layers.size() == 1) {
        this->inputDimension = inputSize;
    }
    this->outputDimension = outputSize;
}

std::vector<double> NeuralNetwork::forward(const std::vector<double>& input) {
    if (layers.empty()) {
        return input;  // Return input if no layers
    }
    
    std::vector<double> current = input;
    for (const auto& layer : layers) {
        current = layer->forward(current);
    }
    
    return current;
}

std::vector<double> NeuralNetwork::predict(const std::vector<double>& input) {
    return forward(input);
}

void NeuralNetwork::train(const std::vector<std::vector<double>>& inputs, 
                          const std::vector<std::vector<double>>& targets,
                          size_t epochs, 
                          double learningRate) {
    if (inputs.empty() || targets.empty() || inputs.size() != targets.size()) {
        throw std::invalid_argument("Invalid training data");
    }
    
    for (size_t epoch = 0; epoch < epochs; ++epoch) {
        double totalLoss = 0.0;
        
        for (size_t i = 0; i < inputs.size(); ++i) {
            // Forward pass
            auto predicted = forward(inputs[i]);
            
            // Calculate loss
            double loss = calculateLoss(predicted, targets[i]);
            totalLoss += loss;
            
            // Backward pass
            auto grad = calculateGradient(predicted, targets[i]);
            
            // Backpropagate through layers (in reverse order)
            for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
                grad = (*it)->backward(grad, learningRate);
            }
        }
        
        // Print progress every 100 epochs
        if (epoch % 100 == 0) {
            std::cout << "Epoch " << epoch << ", Average Loss: " 
                      << (totalLoss / inputs.size()) << std::endl;
        }
    }
}

double NeuralNetwork::calculateLoss(const std::vector<double>& predicted, const std::vector<double>& actual) {
    if (predicted.size() != actual.size()) {
        throw std::invalid_argument("Size mismatch in loss calculation");
    }
    
    double mse = 0.0;
    for (size_t i = 0; i < predicted.size(); ++i) {
        double diff = predicted[i] - actual[i];
        mse += diff * diff;
    }
    
    return mse / predicted.size();
}

std::vector<double> NeuralNetwork::calculateGradient(const std::vector<double>& predicted, const std::vector<double>& actual) {
    std::vector<double> gradient(predicted.size());
    for (size_t i = 0; i < predicted.size(); ++i) {
        gradient[i] = 2.0 * (predicted[i] - actual[i]) / predicted.size();
    }
    return gradient;
}

bool NeuralNetwork::saveModel(const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file) {
        return false;
    }
    
    // Save network structure
    file << layers.size() << " " << inputDimension << " " << outputDimension << "\n";
    
    // Save each layer
    for (const auto& layer : layers) {
        const auto& weights = layer->getWeights();
        const auto& biases = layer->getBiases();
        
        file << weights.size() << " " << (weights.empty() ? 0 : weights[0].size()) << " " 
             << biases.size() << "\n";
        
        // Save weights
        for (const auto& row : weights) {
            for (double w : row) {
                file << w << " ";
            }
            file << "\n";
        }
        
        // Save biases
        for (double b : biases) {
            file << b << " ";
        }
        file << "\n";
    }
    
    file.close();
    return true;
}

bool NeuralNetwork::loadModel(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file) {
        return false;
    }
    
    // Clear existing layers
    layers.clear();
    
    // Load network structure
    size_t numLayers, inputDim, outputDim;
    file >> numLayers >> inputDim >> outputDim;
    
    inputDimension = inputDim;
    outputDimension = outputDim;
    
    // Load each layer
    for (size_t i = 0; i < numLayers; ++i) {
        size_t rows, cols, biasSize;
        file >> rows >> cols >> biasSize;
        
        // Create layer
        auto layer = std::make_unique<NeuralLayer>(cols, rows);
        
        // Load weights
        std::vector<std::vector<double>> weights(rows, std::vector<double>(cols));
        for (size_t r = 0; r < rows; ++r) {
            for (size_t c = 0; c < cols; ++c) {
                file >> weights[r][c];
            }
        }
        layer->setWeights(weights);
        
        // Load biases
        std::vector<double> biases(biasSize);
        for (size_t b = 0; b < biasSize; ++b) {
            file >> biases[b];
        }
        layer->setBiases(biases);
        
        layers.push_back(std::move(layer));
    }
    
    file.close();
    return true;
}

// LSTMCell Implementation
LSTMCell::LSTMCell(size_t inputSize, size_t hiddenSize)
    : inputSize(inputSize), hiddenSize(hiddenSize),
      rng(std::random_device{}()), weightDist(0.0, 0.1) {
    initializeParameters();
}

void LSTMCell::initializeParameters() {
    // Initialize weight matrices for all gates
    // W matrices: [hiddenSize][inputSize]
    // U matrices: [hiddenSize][hiddenSize]
    // b vectors: [hiddenSize]
    
    // Forget gate
    Wforget.resize(hiddenSize, std::vector<double>(inputSize));
    Uforget.resize(hiddenSize, std::vector<double>(hiddenSize));
    bforget.resize(hiddenSize, 0.0);
    
    // Input gate
    Winput.resize(hiddenSize, std::vector<double>(inputSize));
    Uinput.resize(hiddenSize, std::vector<double>(hiddenSize));
    binput.resize(hiddenSize, 0.0);
    
    // Candidate gate
    Wcandidate.resize(hiddenSize, std::vector<double>(inputSize));
    Ucandidate.resize(hiddenSize, std::vector<double>(hiddenSize));
    bcandidate.resize(hiddenSize, 0.0);
    
    // Output gate
    Woutput.resize(hiddenSize, std::vector<double>(inputSize));
    Uoutput.resize(hiddenSize, std::vector<double>(hiddenSize));
    boutput.resize(hiddenSize, 0.0);
    
    // Initialize all weights
    auto initMatrix = [&](std::vector<std::vector<double>>& matrix) {
        for (auto& row : matrix) {
            for (double& val : row) {
                val = weightDist(rng);
            }
        }
    };
    
    initMatrix(Wforget);
    initMatrix(Uforget);
    initMatrix(Winput);
    initMatrix(Uinput);
    initMatrix(Wcandidate);
    initMatrix(Ucandidate);
    initMatrix(Woutput);
    initMatrix(Uoutput);
}

std::pair<std::vector<double>, std::vector<double>> LSTMCell::forward(
    const std::vector<double>& input, 
    const std::vector<double>& prevState,
    const std::vector<double>& prevHidden) {
    
    if (input.size() != inputSize || prevState.size() != hiddenSize || prevHidden.size() != hiddenSize) {
        throw std::invalid_argument("Input size mismatch in LSTM cell");
    }
    
    // Forget gate: f = sigmoid(Wf * input + Uf * prevHidden + bf)
    auto forgetGate = sigmoid(elementWiseAdd(
        elementWiseAdd(matVecMul(Wforget, input), matVecMul(Uforget, prevHidden)), 
        bforget));
    
    // Input gate: i = sigmoid(Wi * input + Ui * prevHidden + bi)
    auto inputGate = sigmoid(elementWiseAdd(
        elementWiseAdd(matVecMul(Winput, input), matVecMul(Uinput, prevHidden)), 
        binput));
    
    // Candidate gate: c~ = tanh(Wc * input + Uc * prevHidden + bc)
    auto candidate = tanh(elementWiseAdd(
        elementWiseAdd(matVecMul(Wcandidate, input), matVecMul(Ucandidate, prevHidden)), 
        bcandidate));
    
    // Cell state: c = f * prevState + i * c~
    auto cellState = elementWiseAdd(
        elementWiseMultiply(forgetGate, prevState),
        elementWiseMultiply(inputGate, candidate));
    
    // Output gate: o = sigmoid(Wo * input + Uo * prevHidden + bo)
    auto outputGate = sigmoid(elementWiseAdd(
        elementWiseAdd(matVecMul(Woutput, input), matVecMul(Uoutput, prevHidden)), 
        boutput));
    
    // Hidden state: h = o * tanh(c)
    auto hiddenState = elementWiseMultiply(outputGate, tanh(cellState));
    
    return std::make_pair(cellState, hiddenState);
}

std::vector<double> LSTMCell::initializeState(size_t size) {
    return std::vector<double>(size, 0.0);
}

std::vector<double> LSTMCell::sigmoid(const std::vector<double>& x) {
    std::vector<double> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        result[i] = 1.0 / (1.0 + std::exp(-x[i]));
    }
    return result;
}

std::vector<double> LSTMCell::tanh(const std::vector<double>& x) {
    std::vector<double> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        result[i] = std::tanh(x[i]);
    }
    return result;
}

std::vector<double> LSTMCell::matVecMul(const std::vector<std::vector<double>>& matrix, 
                                       const std::vector<double>& vec) {
    std::vector<double> result(matrix.size(), 0.0);
    for (size_t i = 0; i < matrix.size(); ++i) {
        for (size_t j = 0; j < vec.size(); ++j) {
            result[i] += matrix[i][j] * vec[j];
        }
    }
    return result;
}

std::vector<double> LSTMCell::elementWiseAdd(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("Vector size mismatch in element-wise addition");
    }
    
    std::vector<double> result(a.size());
    for (size_t i = 0; i < a.size(); ++i) {
        result[i] = a[i] + b[i];
    }
    return result;
}

std::vector<double> LSTMCell::elementWiseMultiply(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("Vector size mismatch in element-wise multiplication");
    }
    
    std::vector<double> result(a.size());
    for (size_t i = 0; i < a.size(); ++i) {
        result[i] = a[i] * b[i];
    }
    return result;
}

// LstmNetwork Implementation
LstmNetwork::LstmNetwork(size_t inputSize, size_t hiddenSize, size_t outputSize)
    : inputSize(inputSize), hiddenSize(hiddenSize), outputSize(outputSize) {
    
    lstmCell = std::make_unique<LSTMCell>(inputSize, hiddenSize);
    outputLayer = std::make_unique<NeuralLayer>(hiddenSize, outputSize, "sigmoid");
}

std::vector<std::vector<double>> LstmNetwork::forward(const std::vector<std::vector<double>>& sequence) {
    if (sequence.empty()) {
        return {};
    }
    
    std::vector<double> cellState = lstmCell->initializeState(hiddenSize);
    std::vector<double> hiddenState = lstmCell->initializeState(hiddenSize);
    
    std::vector<std::vector<double>> outputs;
    
    for (const auto& input : sequence) {
        auto result = lstmCell->forward(input, cellState, hiddenState);
        cellState = result.first;
        hiddenState = result.second;
        
        // Pass hidden state through output layer
        auto output = outputLayer->forward(hiddenState);
        outputs.push_back(output);
    }
    
    return outputs;
}

std::vector<double> LstmNetwork::predictNext(const std::vector<std::vector<double>>& sequence) {
    if (sequence.empty()) {
        return std::vector<double>(outputSize, 0.0);
    }
    
    // Get the last output from the sequence
    auto outputs = forward(sequence);
    if (outputs.empty()) {
        return std::vector<double>(outputSize, 0.0);
    }
    
    return outputs.back();
}

std::vector<double> LstmNetwork::sequenceToVector(const std::vector<std::vector<double>>& sequence) {
    std::vector<double> result;
    for (const auto& vec : sequence) {
        result.insert(result.end(), vec.begin(), vec.end());
    }
    return result;
}