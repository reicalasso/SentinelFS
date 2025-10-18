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