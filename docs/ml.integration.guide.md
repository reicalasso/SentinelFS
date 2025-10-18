# 🤖 Machine Learning Integration Guide

## İçindekiler

1. [Genel Bakış](#1-genel-bakış)
2. [Veri Toplama ve Preprocessing](#2-veri-toplama-ve-preprocessing)
3. [Model Eğitimi](#3-model-eğitimi)
4. [C++ Entegrasyonu](#4-c-entegrasyonu)
5. [Deployment](#5-deployment)
6. [Performans Optimizasyonu](#6-performans-optimizasyonu)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. Genel Bakış

### 1.1 Amaç

SentinelFS-Neo'ya entegre edilen ML modülü, dosya erişim davranışlarını analiz ederek **anormal aktiviteleri gerçek zamanlı tespit eder**. Bu özellik:

- **Güvenlik**: Yetkisiz erişim veya şüpheli aktiviteleri tespit
- **Performans**: Olağandışı dosya erişim patternlerini belirleme
- **Monitoring**: Sistem davranışının sürekli izlenmesi

### 1.2 Mimari Akış

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ FileWatcher  │───▶│   Feature    │───▶│  ML Model    │───▶│   Alert      │
│   Events     │    │  Extraction  │    │  (ONNX/Py)   │    │   System     │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
       │                    │                     │                  │
       ▼                    ▼                     ▼                  ▼
  File Access          Features              Prediction         Log/Action
  Monitoring         (timestamp,             (Normal/Anomaly)
                     file_size, etc.)
```

### 1.3 Desteklenen Modeller

- **Isolation Forest**: Unsupervised anomaly detection (önerilen)
- **One-Class SVM**: Alternatif unsupervised model
- **LSTM**: Zaman serisi tabanlı pattern recognition
- **Autoencoder**: Deep learning tabanlı anomaly detection

---

## 2. Veri Toplama ve Preprocessing

### 2.1 Veri Kaynakları

**FileWatcher Modülünden:**
```cpp
struct FileEvent {
    std::string path;           // Dosya yolu
    std::string operation;      // READ, WRITE, DELETE
    size_t file_size;          // Dosya boyutu (bytes)
    time_t timestamp;          // Erişim zamanı
    uint32_t peer_id;          // Peer kimliği
    uint32_t access_count;     // Son 1 saatteki erişim sayısı
};
```

**MetadataDB'den:**
- Dosya hash history
- Peer access patterns
- Historical statistics

### 2.2 Log Formatı

**CSV Format (Eğitim için):**
```csv
timestamp,file_path,file_size,peer_id,operation,access_count,hour,day_of_week
2024-10-18T10:30:15,/data/file1.txt,2048,1,WRITE,1,10,4
2024-10-18T10:30:20,/data/file2.dat,524288,2,DELETE,5,10,4
2024-10-18T03:15:42,/data/sensitive.doc,1048576,99,READ,50,3,0
```

**Feature Columns:**
- `timestamp`: ISO 8601 format
- `file_path`: Absolute path
- `file_size`: Bytes
- `peer_id`: Integer ID
- `operation`: ENUM (READ, WRITE, DELETE, RENAME)
- `access_count`: Integer
- `hour`: 0-23
- `day_of_week`: 0-6 (Monday=0)

### 2.3 Data Collection Script

**log_collector.cpp:**
```cpp
#include <fstream>
#include <chrono>
#include <iomanip>

class DataCollector {
private:
    std::ofstream log_file;
    
public:
    DataCollector(const std::string& path) {
        log_file.open(path, std::ios::app);
        // Write header if new file
        if (log_file.tellp() == 0) {
            log_file << "timestamp,file_path,file_size,peer_id,operation,access_count,hour,day_of_week\n";
        }
    }
    
    void log(const FileEvent& event) {
        auto time = std::chrono::system_clock::from_time_t(event.timestamp);
        auto time_t = std::chrono::system_clock::to_time_t(time);
        std::tm tm = *std::localtime(&time_t);
        
        log_file << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << ","
                 << event.path << ","
                 << event.file_size << ","
                 << event.peer_id << ","
                 << event.operation << ","
                 << event.access_count << ","
                 << tm.tm_hour << ","
                 << tm.tm_wday << "\n";
        log_file.flush();
    }
};
```

### 2.4 Data Preprocessing

**preprocess.py:**
```python
import pandas as pd
import numpy as np
from sklearn.preprocessing import StandardScaler

def preprocess_data(csv_path):
    # Load data
    df = pd.read_csv(csv_path)
    
    # Parse timestamp
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    df['hour'] = df['timestamp'].dt.hour
    df['day_of_week'] = df['timestamp'].dt.dayofweek
    df['is_weekend'] = df['day_of_week'].isin([5, 6]).astype(int)
    
    # Feature engineering
    df['file_size_mb'] = df['file_size'] / (1024 * 1024)
    df['log_file_size'] = np.log1p(df['file_size'])
    df['is_night'] = df['hour'].isin(range(0, 6)).astype(int)
    
    # Encode categorical
    df['op_code'] = df['operation'].map({'READ': 0, 'WRITE': 1, 'DELETE': 2, 'RENAME': 3})
    
    # Remove outliers (optional)
    df = df[df['file_size'] < df['file_size'].quantile(0.99)]
    
    # Select features for training
    features = ['hour', 'day_of_week', 'file_size_mb', 'peer_id', 'access_count', 
                'is_weekend', 'is_night', 'op_code']
    
    X = df[features]
    
    # Normalize
    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X)
    
    return X_scaled, scaler, features

# Usage
X, scaler, features = preprocess_data('logs/file_access.csv')
print(f"Preprocessed {len(X)} samples with {len(features)} features")
```

---

## 3. Model Eğitimi

### 3.1 Isolation Forest (Önerilen)

**train_isolation_forest.py:**
```python
import pandas as pd
import numpy as np
from sklearn.ensemble import IsolationForest
from sklearn.model_selection import train_test_split, GridSearchCV
from sklearn.metrics import classification_report, confusion_matrix
import joblib
import matplotlib.pyplot as plt

def train_model(csv_path, output_model='models/anomaly_detector.pkl'):
    # Load and preprocess
    X, scaler, features = preprocess_data(csv_path)
    
    # Hyperparameter tuning
    param_grid = {
        'n_estimators': [100, 200, 300],
        'max_samples': ['auto', 256, 512],
        'contamination': [0.001, 0.01, 0.05],
        'max_features': [1.0, 0.8]
    }
    
    iso_forest = IsolationForest(random_state=42, n_jobs=-1)
    
    # Grid search (using custom scorer)
    # Note: IsolationForest is unsupervised, so we use all data
    best_model = IsolationForest(
        n_estimators=200,
        max_samples='auto',
        contamination=0.01,
        max_features=1.0,
        random_state=42,
        n_jobs=-1
    )
    
    # Fit model
    best_model.fit(X)
    
    # Predictions (-1 = anomaly, 1 = normal)
    predictions = best_model.predict(X)
    anomaly_scores = best_model.score_samples(X)
    
    # Statistics
    n_anomalies = (predictions == -1).sum()
    anomaly_rate = n_anomalies / len(predictions)
    
    print(f"Training completed!")
    print(f"Total samples: {len(X)}")
    print(f"Detected anomalies: {n_anomalies} ({anomaly_rate*100:.2f}%)")
    print(f"Anomaly score range: [{anomaly_scores.min():.4f}, {anomaly_scores.max():.4f}]")
    
    # Save model and scaler
    joblib.dump(best_model, output_model)
    joblib.dump(scaler, output_model.replace('.pkl', '_scaler.pkl'))
    joblib.dump(features, output_model.replace('.pkl', '_features.pkl'))
    
    print(f"Model saved to {output_model}")
    
    # Visualization
    plt.figure(figsize=(12, 4))
    
    plt.subplot(131)
    plt.hist(anomaly_scores, bins=50)
    plt.xlabel('Anomaly Score')
    plt.ylabel('Frequency')
    plt.title('Anomaly Score Distribution')
    
    plt.subplot(132)
    plt.scatter(range(len(anomaly_scores)), anomaly_scores, 
                c=predictions, cmap='RdYlGn', alpha=0.6, s=1)
    plt.xlabel('Sample Index')
    plt.ylabel('Anomaly Score')
    plt.title('Anomaly Detection Results')
    plt.colorbar(label='Prediction')
    
    plt.subplot(133)
    hourly_anomalies = pd.DataFrame({
        'hour': X[:, features.index('hour')],
        'is_anomaly': predictions == -1
    }).groupby('hour')['is_anomaly'].sum()
    hourly_anomalies.plot(kind='bar')
    plt.xlabel('Hour of Day')
    plt.ylabel('Anomaly Count')
    plt.title('Anomalies by Hour')
    
    plt.tight_layout()
    plt.savefig('models/training_results.png')
    print("Visualization saved to models/training_results.png")
    
    return best_model, scaler, features

# Train
if __name__ == '__main__':
    model, scaler, features = train_model('logs/file_access.csv')
```

### 3.2 LSTM Model (Zaman Serisi)

**train_lstm.py:**
```python
import torch
import torch.nn as nn
import numpy as np
from torch.utils.data import Dataset, DataLoader

class FileAccessLSTM(nn.Module):
    def __init__(self, input_size, hidden_size=64, num_layers=2):
        super().__init__()
        self.lstm = nn.LSTM(input_size, hidden_size, num_layers, 
                           batch_first=True, dropout=0.2)
        self.fc1 = nn.Linear(hidden_size, 32)
        self.fc2 = nn.Linear(32, 1)
        self.relu = nn.ReLU()
        self.sigmoid = nn.Sigmoid()
        
    def forward(self, x):
        lstm_out, _ = self.lstm(x)
        out = self.relu(self.fc1(lstm_out[:, -1, :]))
        out = self.sigmoid(self.fc2(out))
        return out

class TimeSeriesDataset(Dataset):
    def __init__(self, sequences, labels):
        self.sequences = sequences
        self.labels = labels
        
    def __len__(self):
        return len(self.sequences)
    
    def __getitem__(self, idx):
        return torch.FloatTensor(self.sequences[idx]), torch.FloatTensor([self.labels[idx]])

def create_sequences(data, seq_length=10):
    sequences = []
    labels = []
    
    for i in range(len(data) - seq_length):
        seq = data[i:i+seq_length]
        # Label: 1 if next access is anomalous (based on some threshold)
        label = 1 if data[i+seq_length, -1] > threshold else 0
        sequences.append(seq)
        labels.append(label)
    
    return np.array(sequences), np.array(labels)

# Training loop
def train_lstm(model, train_loader, epochs=50):
    criterion = nn.BCELoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=0.001)
    
    for epoch in range(epochs):
        model.train()
        total_loss = 0
        
        for sequences, labels in train_loader:
            optimizer.zero_grad()
            outputs = model(sequences)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        
        if (epoch + 1) % 10 == 0:
            print(f'Epoch [{epoch+1}/{epochs}], Loss: {total_loss/len(train_loader):.4f}')
    
    return model

# Usage example
# sequences, labels = create_sequences(X_scaled, seq_length=10)
# dataset = TimeSeriesDataset(sequences, labels)
# train_loader = DataLoader(dataset, batch_size=32, shuffle=True)
# model = FileAccessLSTM(input_size=len(features))
# trained_model = train_lstm(model, train_loader)
```

---

## 4. C++ Entegrasyonu

### 4.1 Python Subprocess (Basit)

**ml_analyzer.hpp:**
```cpp
#ifndef ML_ANALYZER_HPP
#define ML_ANALYZER_HPP

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstdlib>

class MLAnalyzer {
public:
    static bool predict(const std::vector<float>& features) {
        // Write features to temp file
        std::ofstream temp_file("/tmp/features.txt");
        for (const auto& f : features) {
            temp_file << f << " ";
        }
        temp_file.close();
        
        // Call Python script
        std::string cmd = "python3 scripts/predict.py /tmp/features.txt > /tmp/prediction.txt 2>&1";
        int ret = system(cmd.c_str());
        
        if (ret != 0) {
            std::cerr << "[ML] Prediction failed!" << std::endl;
            return false;
        }
        
        // Read result
        std::ifstream result_file("/tmp/prediction.txt");
        int prediction;
        result_file >> prediction;
        
        return prediction == 1;  // 1 = anomaly
    }
};

#endif // ML_ANALYZER_HPP
```

**scripts/predict.py:**
```python
#!/usr/bin/env python3
import sys
import joblib
import numpy as np

# Load model
model = joblib.load('models/anomaly_detector.pkl')
scaler = joblib.load('models/anomaly_detector_scaler.pkl')

# Read features
with open(sys.argv[1], 'r') as f:
    features = np.array([float(x) for x in f.read().split()])

# Normalize
features_scaled = scaler.transform(features.reshape(1, -1))

# Predict
prediction = model.predict(features_scaled)

# Output: 1 = anomaly, 0 = normal
print(1 if prediction[0] == -1 else 0)
```

### 4.2 ONNX Runtime (Yüksek Performans)

**CMakeLists.txt (ONNX support):**
```cmake
find_package(onnxruntime REQUIRED)

add_executable(sentinelfs-neo 
    src/main.cpp
    src/ml_analyzer_onnx.cpp
    # ... other sources
)

target_link_libraries(sentinelfs-neo 
    onnxruntime
    # ... other libraries
)
```

**ml_analyzer_onnx.hpp:**
```cpp
#ifndef ML_ANALYZER_ONNX_HPP
#define ML_ANALYZER_ONNX_HPP

#include <onnxruntime_cxx_api.h>
#include <vector>
#include <memory>
#include <iostream>

class ONNXAnalyzer {
private:
    Ort::Env env;
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    
    std::vector<const char*> input_names{"float_input"};
    std::vector<const char*> output_names{"output"};
    
public:
    ONNXAnalyzer(const std::string& model_path) 
        : env(ORT_LOGGING_LEVEL_WARNING, "SentinelFS") {
        
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_EXTENDED
        );
        
        try {
            session = std::make_unique<Ort::Session>(
                env, model_path.c_str(), session_options
            );
            std::cout << "[ML] ONNX model loaded: " << model_path << std::endl;
        } catch (const Ort::Exception& e) {
            std::cerr << "[ML] Failed to load ONNX model: " << e.what() << std::endl;
            throw;
        }
    }
    
    bool predict(const std::vector<float>& features) {
        try {
            // Create input tensor
            auto memory_info = Ort::MemoryInfo::CreateCpu(
                OrtArenaAllocator, OrtMemTypeDefault
            );
            
            std::vector<int64_t> input_shape = {1, static_cast<int64_t>(features.size())};
            
            auto input_tensor = Ort::Value::CreateTensor<float>(
                memory_info,
                const_cast<float*>(features.data()),
                features.size(),
                input_shape.data(),
                input_shape.size()
            );
            
            // Run inference
            auto output_tensors = session->Run(
                Ort::RunOptions{nullptr},
                input_names.data(),
                &input_tensor,
                1,
                output_names.data(),
                1
            );
            
            // Get result
            float* output_data = output_tensors[0].GetTensorMutableData<float>();
            
            // Isolation Forest: -1 = anomaly, 1 = normal
            return output_data[0] < 0;
            
        } catch (const Ort::Exception& e) {
            std::cerr << "[ML] Inference error: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Batch prediction
    std::vector<bool> predictBatch(const std::vector<std::vector<float>>& features_batch) {
        std::vector<bool> results;
        results.reserve(features_batch.size());
        
        for (const auto& features : features_batch) {
            results.push_back(predict(features));
        }
        
        return results;
    }
    
    float getAnomalyScore(const std::vector<float>& features) {
        // Similar to predict, but returns raw score
        // Implementation depends on model output
        return 0.0f;  // Placeholder
    }
};

#endif // ML_ANALYZER_ONNX_HPP
```

### 4.3 Feature Extraction Helper

**feature_extractor.hpp:**
```cpp
#ifndef FEATURE_EXTRACTOR_HPP
#define FEATURE_EXTRACTOR_HPP

#include <vector>
#include <ctime>
#include <cmath>

struct FileEvent {
    std::string path;
    std::string operation;
    size_t file_size;
    time_t timestamp;
    uint32_t peer_id;
    uint32_t access_count;
};

class FeatureExtractor {
public:
    static std::vector<float> extract(const FileEvent& event) {
        std::tm* tm = std::localtime(&event.timestamp);
        
        int hour = tm->tm_hour;
        int day_of_week = tm->tm_wday;
        float file_size_mb = static_cast<float>(event.file_size) / (1024.0f * 1024.0f);
        int is_weekend = (day_of_week == 0 || day_of_week == 6) ? 1 : 0;
        int is_night = (hour >= 0 && hour < 6) ? 1 : 0;
        
        int op_code = 0;
        if (event.operation == "READ") op_code = 0;
        else if (event.operation == "WRITE") op_code = 1;
        else if (event.operation == "DELETE") op_code = 2;
        else if (event.operation == "RENAME") op_code = 3;
        
        // Match training features order
        return {
            static_cast<float>(hour),
            static_cast<float>(day_of_week),
            file_size_mb,
            static_cast<float>(event.peer_id),
            static_cast<float>(event.access_count),
            static_cast<float>(is_weekend),
            static_cast<float>(is_night),
            static_cast<float>(op_code)
        };
    }
    
    // Normalize features (must match training scaler)
    static std::vector<float> normalize(const std::vector<float>& features, 
                                       const std::vector<float>& mean,
                                       const std::vector<float>& std_dev) {
        std::vector<float> normalized(features.size());
        
        for (size_t i = 0; i < features.size(); ++i) {
            normalized[i] = (features[i] - mean[i]) / std_dev[i];
        }
        
        return normalized;
    }
};

#endif // FEATURE_EXTRACTOR_HPP
```

---

## 5. Deployment

### 5.1 Model Deployment Checklist

- [ ] Model trained and validated
- [ ] ONNX conversion completed (if using ONNX)
- [ ] Scaler parameters exported
- [ ] Feature list documented
- [ ] C++ integration tested
- [ ] Performance benchmarks completed
- [ ] Fallback mechanism implemented
- [ ] Logging configured

### 5.2 Full Integration Example

**main.cpp:**
```cpp
#include "sentinelfs_lite.h"
#include "ml_analyzer_onnx.hpp"
#include "feature_extractor.hpp"
#include "data_collector.hpp"

int main(int argc, char** argv) {
    // Configuration
    std::string session_code = "DEMO-2024";
    std::string sync_path = "/sentinel/data";
    bool enable_ml = true;
    
    // Initialize components
    MetadataDB db("metadata.db");
    NetworkManager net(session_code);
    DataCollector collector("logs/file_access.csv");
    
    // ML Analyzer (with error handling)
    std::unique_ptr<ONNXAnalyzer> ml_analyzer;
    if (enable_ml) {
        try {
            ml_analyzer = std::make_unique<ONNXAnalyzer>("models/file_access_model.onnx");
            std::cout << "[INFO] ML anomaly detection enabled" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[WARN] ML initialization failed: " << e.what() << std::endl;
            std::cerr << "[WARN] Continuing without ML features" << std::endl;
            enable_ml = false;
        }
    }
    
    // File watcher with ML integration
    FileWatcher watcher(sync_path, [&](const FileEvent& event) {
        std::cout << "[FS] " << event.operation << ": " << event.path << std::endl;
        
        // Log for future training
        collector.log(event);
        
        // Delta sync
        DeltaEngine delta(event.path);
        auto diff = delta.compute();
        net.sendDelta(diff);
        
        // ML Anomaly Detection
        if (enable_ml && ml_analyzer) {
            auto features = FeatureExtractor::extract(event);
            bool is_anomaly = ml_analyzer->predict(features);
            
            if (is_anomaly) {
                std::cout << "[ALERT] 🚨 ANOMALY DETECTED!" << std::endl;
                std::cout << "  File: " << event.path << std::endl;
                std::cout << "  Operation: " << event.operation << std::endl;
                std::cout << "  Size: " << event.file_size << " bytes" << std::endl;
                std::cout << "  Peer: " << event.peer_id << std::endl;
                std::cout << "  Time: " << std::ctime(&event.timestamp);
                
                // Log to database
                db.logAnomaly(event.path, "ML_DETECTED", features);
                
                // - Take action
                // - Send alert to admin
                // - Quarantine file
                // - Block peer
                // - Request manual review
            }
        }
    });
    
    // Start services
    net.discoverPeers();
    watcher.start();
    
    std::cout << "[INFO] SentinelFS-Neo started with session: " << session_code << std::endl;
    std::cout << "[INFO] Watching: " << sync_path << std::endl;
    
    // Main loop
    while (true) {
        net.updateRemesh();
        net.applyIncomingDeltas(db);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
```

---

## 6. Performans Optimizasyonu

### 6.1 Asenkron ML Processing

```cpp
#include <future>
#include <queue>
#include <mutex>

class AsyncMLAnalyzer {
private:
    ONNXAnalyzer analyzer;
    std::queue<std::future<bool>> pending_predictions;
    std::mutex queue_mutex;
    
public:
    AsyncMLAnalyzer(const std::string& model_path) : analyzer(model_path) {}
    
    void predictAsync(const std::vector<float>& features, 
                     std::function<void(bool)> callback) {
        auto future = std::async(std::launch::async, [this, features, callback]() {
            bool result = analyzer.predict(features);
            callback(result);
            return result;
        });
        
        std::lock_guard<std::mutex> lock(queue_mutex);
        pending_predictions.push(std::move(future));
    }
    
    void cleanup() {
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!pending_predictions.empty()) {
            if (pending_predictions.front().wait_for(std::chrono::seconds(0)) == 
                std::future_status::ready) {
                pending_predictions.pop();
            } else {
                break;
            }
        }
    }
};
```

### 6.2 Feature Caching

```cpp
class FeatureCache {
private:
    std::unordered_map<std::string, std::vector<float>> cache;
    std::mutex cache_mutex;
    size_t max_size = 1000;
    
public:
    bool get(const std::string& key, std::vector<float>& features) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache.find(key);
        if (it != cache.end()) {
            features = it->second;
            return true;
        }
        return false;
    }
    
    void put(const std::string& key, const std::vector<float>& features) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (cache.size() >= max_size) {
            cache.erase(cache.begin());  // Simple eviction
        }
        cache[key] = features;
    }
};
```

### 6.3 Batch Processing

```cpp
class BatchMLProcessor {
private:
    ONNXAnalyzer analyzer;
    std::vector<std::vector<float>> batch;
    std::vector<std::function<void(bool)>> callbacks;
    size_t batch_size = 32;
    std::mutex batch_mutex;
    
public:
    BatchMLProcessor(const std::string& model_path) : analyzer(model_path) {}
    
    void add(const std::vector<float>& features, std::function<void(bool)> callback) {
        std::lock_guard<std::mutex> lock(batch_mutex);
        batch.push_back(features);
        callbacks.push_back(callback);
        
        if (batch.size() >= batch_size) {
            process();
        }
    }
    
private:
    void process() {
        auto results = analyzer.predictBatch(batch);
        
        for (size_t i = 0; i < results.size(); ++i) {
            callbacks[i](results[i]);
        }
        
        batch.clear();
        callbacks.clear();
    }
};
```

---

## 7. Troubleshooting

### 7.1 Common Issues

**Problem**: Model loading fails
```
Solution:
- Check model file path
- Verify ONNX version compatibility
- Ensure model was exported correctly
- Check file permissions
```

**Problem**: High false positive rate
```
Solution:
- Retrain with more data
- Adjust contamination parameter
- Fine-tune feature engineering
- Add more contextual features
```

**Problem**: Inference too slow
```
Solution:
- Use ONNX instead of Python subprocess
- Enable async processing
- Reduce model complexity
- Use batch processing
- Cache features
```

**Problem**: Feature mismatch errors
```
Solution:
- Ensure feature order matches training
- Verify normalization parameters
- Check data types (float vs int)
- Validate feature count
```

### 7.2 Debugging Tips

**Enable verbose logging:**
```cpp
#define ML_DEBUG 1

#ifdef ML_DEBUG
    #define ML_LOG(x) std::cout << "[ML DEBUG] " << x << std::endl
#else
    #define ML_LOG(x)
#endif
```

**Test with known data:**
```cpp
// Test with normal file access
std::vector<float> normal_features = {14.0, 3.0, 1.5, 1.0, 5.0, 0.0, 0.0, 1.0};
assert(!analyzer.predict(normal_features));

// Test with anomalous pattern
std::vector<float> anomaly_features = {3.0, 0.0, 500.0, 999.0, 100.0, 1.0, 1.0, 2.0};
assert(analyzer.predict(anomaly_features));
```

**Benchmark performance:**
```cpp
auto start = std::chrono::high_resolution_clock::now();
bool result = analyzer.predict(features);
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "Inference time: " << duration.count() << " μs" << std::endl;
```

### 7.3 Performance Metrics

**Target Metrics:**
- **Inference latency**: < 10ms (ONNX), < 100ms (Python)
- **False positive rate**: < 5%
- **Detection accuracy**: > 95%
- **Memory usage**: < 100MB
- **CPU usage**: < 10% average

---

## Sonuç

Bu rehber, SentinelFS-Neo'ya ML entegrasyonunu adım adım açıklamaktadır. Daha fazla bilgi için:

- [Architecture Documentation](architecture.md)
- [API Examples](example.codes.and.api.plan.md)
- [Future Plans](future.plans.and.summary.md)

---

**Son Güncelleme**: 2024-10-18
**Versiyon**: 1.0
