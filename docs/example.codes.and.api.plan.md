## 💻 **7\. Örnek Kod ve API Tasarımı**

### 7.1 Temel Modüller

-   **FileWatcher:** Dosya değişikliklerini izler ve delta hesaplar
    
-   **MetadataDB:** Dosya bilgilerini ve peer listesini saklar
    
-   **NetworkManager:** Peer discovery, remesh ve veri transferi
    
-   **DeltaEngine:** Dosya farklarını hesaplar ve uygular
    

---

### 7.2 Basit C++ API Örneği

```cpp
#include "sentinelfs_lite.h"

int main() {
    // 1. Metadata ve network setup
    MetadataDB db("metadata.db");
    NetworkManager net("SESSION_CODE_123");

    // 2. Peer discovery
    net.discoverPeers();
    auto peers = net.getPeers();
    std::cout << "Discovered peers: " << peers.size() << std::endl;

    // 3. File watcher başlat
    FileWatcher watcher("/sentinel/data", [&](const std::string &filePath){
        std::cout << "File changed: " << filePath << std::endl;

        // 4. Delta hesapla
        DeltaEngine delta(filePath);
        auto diff = delta.compute();

        // 5. Delta'yı tüm peer'lere gönder
        net.sendDelta(diff);
    });

    watcher.start();

    // 6. Peer listesi ve delta uygulama loop
    while(true) {
        net.updateRemesh();
        net.applyIncomingDeltas(db);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
```

---

### 7.3 Temel API Fonksiyonları

| Fonksiyon | Modül | Açıklama |
| --- | --- | --- |
| `MetadataDB::addFile(path, hash)` | MetadataDB | Yeni dosya ekler ve hash kaydeder |
| `MetadataDB::updateFile(path, hash)` | MetadataDB | Dosya değiştiğinde metadata günceller |
| `NetworkManager::discoverPeers()` | NetworkManager | Aynı session code’a sahip cihazları bulur |
| `NetworkManager::sendDelta(delta)` | NetworkManager | Hesaplanan delta’yı tüm peer’lere yollar |
| `NetworkManager::updateRemesh()` | NetworkManager | Ağ koşullarına göre peer bağlantılarını optimize eder |
| `DeltaEngine::compute()` | DeltaEngine | Dosya değişikliklerini hesaplar ve delta döner |
| `FileWatcher::start()` | FileWatcher | İzlemeyi başlatır ve callback ile delta hesaplatır |

---

### 7.4 Delta Hesaplama Örneği

```cpp
DeltaEngine delta("example.txt");

// Önceki ve yeni dosya hash'lerini karşılaştır
auto diff = delta.compute();

for (auto &chunk : diff) {
    std::cout << "Changed chunk offset: " << chunk.offset
              << ", length: " << chunk.length << std::endl;
}
```

---

### 7.5 Peer’e Delta Gönderme

```cpp
NetworkManager net("SESSION_CODE_123");

// Delta nesnesi
DeltaData deltaData = delta.compute();

// Tüm peer’lere gönder
net.sendDelta(deltaData);
```

---

### 7.6 Kullanım Notları

1.  **Session Code:** Peer discovery ve güvenlik için zorunlu
    
2.  **Delta Engine:** Büyük dosyalarda sadece değişen blokları göndererek ağ yükünü azaltır
    
3.  **Auto-Remesh:** `updateRemesh()` çağrısı, en iyi bağlantıları sürekli optimize eder
    
4.  **Threading:** FileWatcher ve NetworkManager paralel çalışacak şekilde tasarlanmıştır
    

---

### 7.7 ML Anomaly Detection Entegrasyonu

#### 7.7.1 Temel ML API

**MLAnalyzer Sınıfı (Python Subprocess):**

```cpp
#include <string>
#include <vector>

class MLAnalyzer {
public:
    // Python subprocess ile anomaly detection
    static bool checkAnomaly(const std::string& logFile) {
        std::string command = "python3 predict_anomaly.py " + logFile + " > /tmp/result.txt";
        int ret = system(command.c_str());
        
        if (ret != 0) {
            return false;  // Hata durumunda false
        }
        
        std::ifstream file("/tmp/result.txt");
        std::string line;
        std::getline(file, line);
        
        return line == "1";  // 1 = anomaly detected
    }
    
    // Feature extraction helper
    static std::vector<float> extractFeatures(const std::string& filePath) {
        return {
            static_cast<float>(getCurrentHour()),
            static_cast<float>(getFileSize(filePath)),
            static_cast<float>(getPeerID()),
            static_cast<float>(getAccessCount(filePath)),
            static_cast<float>(getCurrentDayOfWeek())
        };
    }
};
```

---

#### 7.7.2 ONNX Runtime ile Yüksek Performanslı Analiz

```cpp
#include <onnxruntime_cxx_api.h>
#include <vector>

class ONNXAnalyzer {
private:
    Ort::Env env;
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;

public:
    ONNXAnalyzer() : env(ORT_LOGGING_LEVEL_WARNING, "SentinelFS") {
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        
        session = std::make_unique<Ort::Session>(
            env, 
            "models/file_access_model.onnx", 
            session_options
        );
    }
    
    bool predict(const std::vector<float>& features) {
        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<int64_t> input_shape = {1, static_cast<int64_t>(features.size())};
        
        auto input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, 
            const_cast<float*>(features.data()), 
            features.size(),
            input_shape.data(), 
            input_shape.size()
        );
        
        const char* input_names[] = {"float_input"};
        const char* output_names[] = {"output"};
        
        auto output_tensors = session->Run(
            Ort::RunOptions{nullptr}, 
            input_names, 
            &input_tensor, 
            1,
            output_names, 
            1
        );
        
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        return output_data[0] < 0;  // -1 = anomaly, 1 = normal
    }
    
    // Batch prediction desteği
    std::vector<bool> predictBatch(const std::vector<std::vector<float>>& features_batch) {
        std::vector<bool> results;
        for (const auto& features : features_batch) {
            results.push_back(predict(features));
        }
        return results;
    }
};
```

---

#### 7.7.3 FileWatcher ile ML Entegrasyonu

**Gerçek Zamanlı Anomaly Detection:**

```cpp
#include "sentinelfs_lite.h"
#include "ml_analyzer.h"

int main() {
    MetadataDB db("metadata.db");
    NetworkManager net("SESSION_CODE_123");
    
    // ML Analyzer seçimi (ONNX veya Python)
    #ifdef USE_ONNX
        ONNXAnalyzer ml_analyzer;
    #else
        // Python subprocess kullanımı (daha basit)
    #endif
    
    // File watcher + ML callback
    FileWatcher watcher("/sentinel/data", [&](const FileEvent& event){
        std::cout << "[INFO] File " << event.type << ": " << event.path << std::endl;
        
        // 1. Delta hesapla ve gönder
        DeltaEngine delta(event.path);
        auto diff = delta.compute();
        net.sendDelta(diff);
        
        // 2. ML Feature extraction
        std::vector<float> features = {
            static_cast<float>(std::time(nullptr) % 86400),  // hour of day
            static_cast<float>(event.file_size),
            static_cast<float>(event.peer_id),
            static_cast<float>(db.getAccessCount(event.path)),
            static_cast<float>(getCurrentDayOfWeek())
        };
        
        // 3. Anomaly detection
        #ifdef USE_ONNX
            bool is_anomaly = ml_analyzer.predict(features);
        #else
            bool is_anomaly = MLAnalyzer::checkAnomaly("logs/access.log");
        #endif
        
        // 4. Alert handling
        if(is_anomaly) {
            std::cout << "[ALERT] 🚨 Anomalous file access detected!" << std::endl;
            std::cout << "  File: " << event.path << std::endl;
            std::cout << "  Size: " << event.file_size << " bytes" << std::endl;
            std::cout << "  Peer: " << event.peer_id << std::endl;
            
            // Log to database
            db.logAnomaly(event.path, features);
            
            // Quarantine or block
            // security.quarantineFile(event.path);
        }
    });
    
    watcher.start();
    
    // Main loop
    while(true) {
        net.updateRemesh();
        net.applyIncomingDeltas(db);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
```

---

#### 7.7.4 Python Model Eğitim Scripti

**train_model.py:**

```python
import pandas as pd
from sklearn.ensemble import IsolationForest
from sklearn.model_selection import train_test_split
import joblib

# Veri yükleme
data = pd.read_csv("logs/file_access_log.csv")

# Feature engineering
data['hour'] = pd.to_datetime(data['timestamp']).dt.hour
data['day_of_week'] = pd.to_datetime(data['timestamp']).dt.dayofweek
data['file_size_mb'] = data['file_size'] / (1024 * 1024)

# Features
features = ['hour', 'file_size_mb', 'peer_id', 'access_count', 'day_of_week']
X = data[features]

# Model eğitimi
model = IsolationForest(
    contamination=0.01,  # %1 anomaly bekleniyor
    n_estimators=100,
    max_samples='auto',
    random_state=42
)

model.fit(X)

# Model kaydetme
joblib.dump(model, "models/file_access_model.pkl")
print("✅ Model trained and saved successfully!")

# Accuracy test
predictions = model.predict(X)
anomaly_count = (predictions == -1).sum()
print(f"Detected {anomaly_count} anomalies in training data")
```

**predict_anomaly.py:**

```python
import sys
import joblib
import pandas as pd

# Model yükleme
model = joblib.load("models/file_access_model.pkl")

# Log dosyasından son satırı oku
log_file = sys.argv[1]
data = pd.read_csv(log_file).tail(1)

# Feature extraction
features = data[['hour', 'file_size_mb', 'peer_id', 'access_count', 'day_of_week']]

# Prediction
prediction = model.predict(features)

# Output: 1 = anomaly, 0 = normal
print(1 if prediction[0] == -1 else 0)
```

---

#### 7.7.5 ONNX Model Export (Python → C++)

```python
import skl2onnx
from skl2onnx import convert_sklearn
from skl2onnx.common.data_types import FloatTensorType
import joblib

# Model yükleme
model = joblib.load("models/file_access_model.pkl")

# ONNX'e çevirme
initial_type = [('float_input', FloatTensorType([None, 5]))]
onnx_model = convert_sklearn(model, initial_types=initial_type)

# Kaydetme
with open("models/file_access_model.onnx", "wb") as f:
    f.write(onnx_model.SerializeToString())

print("✅ ONNX model exported successfully!")
```

---

#### 7.7.6 ML API Fonksiyonları

| Fonksiyon | Modül | Açıklama |
|-----------|-------|----------|
| `MLAnalyzer::checkAnomaly(logFile)` | MLAnalyzer | Python subprocess ile anomaly tespiti |
| `ONNXAnalyzer::predict(features)` | ONNXAnalyzer | ONNX ile hızlı anomaly tespiti |
| `ONNXAnalyzer::predictBatch(batch)` | ONNXAnalyzer | Toplu prediction desteği |
| `MLAnalyzer::extractFeatures(path)` | MLAnalyzer | Dosyadan feature çıkarma |
| `MetadataDB::logAnomaly(path, features)` | MetadataDB | Anomaly bilgisini kaydetme |
| `MetadataDB::getAccessCount(path)` | MetadataDB | Dosya erişim sayısını getirme |

---

#### 7.7.7 Performans Optimizasyonu

**Asenkron ML Processing:**

```cpp
#include <future>
#include <queue>

class AsyncMLAnalyzer {
private:
    ONNXAnalyzer ml_analyzer;
    std::queue<std::future<bool>> predictions;
    
public:
    void predictAsync(const std::vector<float>& features) {
        predictions.push(std::async(std::launch::async, [this, features](){
            return ml_analyzer.predict(features);
        }));
    }
    
    bool getResult() {
        if (predictions.empty()) return false;
        
        auto& future = predictions.front();
        bool result = future.get();
        predictions.pop();
        
        return result;
    }
};
```

**Kullanım:**
```cpp
AsyncMLAnalyzer async_ml;

// Non-blocking prediction
async_ml.predictAsync(features);

// Daha sonra result al
if (async_ml.getResult()) {
    std::cout << "Anomaly detected!" << std::endl;
}
```

---

#### 7.7.8 Test ve Validation

**Unit Test Örneği:**

```cpp
#include <gtest/gtest.h>
#include "ml_analyzer.h"

TEST(MLAnalyzer, NormalFileAccess) {
    std::vector<float> normal_features = {14.0, 1024.0, 1.0, 5.0, 3.0};  // Öğleden sonra, küçük dosya
    ONNXAnalyzer analyzer;
    EXPECT_FALSE(analyzer.predict(normal_features));
}

TEST(MLAnalyzer, AnomalousFileAccess) {
    std::vector<float> anomaly_features = {3.0, 1048576.0, 999.0, 100.0, 0.0};  // Gece 3, büyük dosya, yüksek frekans
    ONNXAnalyzer analyzer;
    EXPECT_TRUE(analyzer.predict(anomaly_features));
}
```

**Detaylı bilgi için:** [Bölüm 9.6 - AI Destekli Anomali Tespiti](future.plans.and.summary.md#96-ai-destekli-anomali-tespiti-detayları)

---