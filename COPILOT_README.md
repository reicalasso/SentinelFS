## 🧠 SentinelFS-Neo — Copilot İçin Mimari Manifesto

Bu proje **plugin tabanlı, modüler bir P2P dosya senkronizasyon sistemi**dir.  
Copilot, bu proje için kod üretirken aşağıdaki kurallara uymalıdır.

---

### 📌 1. Proje Mimarisi

SentinelFS-Neo şu katmanlardan oluşur:

```pgsql
core/
plugins/
  filesystem/
  network/
  storage/
  ml/
app/
tests/
runtime/
```

Copilot aşağıdaki yapıya uymalıdır:

-   **Core**, uygulamanın beyni ve tek sabit katmandır
    
-   **Plugin'ler bağımsızdır** ve `plugin_info`, `plugin_create`, `plugin_destroy` fonksiyonlarını export eder
    
-   **Her plugin kendi klasöründe kendi CMake dosyasıyla ayrı inşa edilir**
    
-   **Core, plugin’lere asla doğrudan bağlı değildir** — sadece PluginLoader ile dinamik yükler
    

---

### 📌 2. Plugin Mantığı

Her plugin şu üç fonksiyonu sağlar:

```cpp
extern "C" SFS_PluginInfo plugin_info();
extern "C" void* plugin_create();
extern "C" void plugin_destroy(void*);
```

Copilot yalnızca bu formatta plugin önerileri oluşturmalıdır.

---

### 📌 3. Core Katmanı Sorumlulukları

Copilot’un core içine ekleyeceği kodlar *yalnızca*:

-   EventBus
    
-   PluginLoader
    
-   FileAPI
    
-   SnapshotEngine
    
-   Config
    
-   Logger
    

olmalıdır.

Başka iş mantığı **core’a yazılmamalıdır**.

---

### 📌 4. File System Katmanı Kuralları

Copilot şu API’yi referans almalıdır:

```cpp
class IFileAPI {
public:
    virtual bool exists(const std::string&) const = 0;
    virtual bool remove(const std::string&) = 0;
    virtual std::vector<uint8_t> read_all(const std::string&) const = 0;
    virtual bool write_all(const std::string&, const std::vector<uint8_t>&) = 0;
    virtual uint64_t file_size(const std::string&) const = 0;
    virtual std::string hash(const std::string&) const = 0;
    virtual std::vector<FileChunk> split_into_chunks(const std::string&, size_t) const = 0;
};
```

Copilot başka API tasarlamamalıdır.

---

### 📌 5. Delta Engine Kuralları

Copilot, delta motoru için şu interface’i kullanmalıdır:

```cpp
class IDeltaEngine {
public:
    virtual DeltaResult generate(...) = 0;
    virtual bool apply(...) = 0;
};
```

Rsync tarzı rolling checksum → **uygulanabilir**.

---

### 📌 6. Network Plugin Kuralları

Discovery ve transfer motorları plugin’dır.

Copilot şu interface’i referans almalıdır:

```cpp
class IDiscovery {
public:
    std::function<void(const PeerInfo&)> on_peer_found;
    virtual void start(const std::string&, uint16_t) = 0;
    virtual void stop() = 0;
};
```

---

### 📌 7. Kod Üretim Stili

Copilot şunlara dikkat etmelidir:

-   Kod daima **modüler** olmalıdır
    
-   Kod **plugin sınırlarını ihlal etmemelidir**
    
-   Statik global değişkenler kullanmamalıdır
    
-   Kod **event bus üzerinden iletişim** kurmalıdır
    
-   CMake yapılandırması her modülde yer almalıdır
    

---

### 📌 8. Copilot’un Üretmemesi Gereken Şeyler

Copilot aşağıdakileri üretmemelidir:

❌ Monolit kod  
❌ Plugin yükleme olmadan doğrudan sınıf örnekleme  
❌ Core içine business logic gömme  
❌ Rastgele klasör yapısı  
❌ Rastgele interface ismi  
❌ Rust/Go/Python kodu

Bu proje **C++17 + CMake + plugin architecture** üzerine kuruludur.

---