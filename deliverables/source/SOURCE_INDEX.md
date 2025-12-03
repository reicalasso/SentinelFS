# SentinelFS Kaynak Kod İndeksi

Bu doküman projenin ana kaynak kod dosyalarını ve işlevlerini açıklar.

---

## 1. Daemon (app/daemon/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `sentinel_daemon.cpp` | ~150 | Ana giriş noktası, daemon başlatma |
| `DaemonCore.cpp/h` | ~300 | Daemon yaşam döngüsü, sinyal yönetimi |
| `IPCHandler.cpp/h` | ~400 | GUI ile IPC iletişimi (Unix socket) |
| `MetricsServer.cpp/h` | ~200 | Prometheus HTTP endpoint |

### Önemli Fonksiyonlar

```cpp
// sentinel_daemon.cpp
int main(int argc, char* argv[])  // Daemon başlatma

// DaemonCore.cpp
void DaemonCore::initialize()     // Plugin yükleme, EventBus kurulumu
void DaemonCore::run()            // Ana döngü
void DaemonCore::shutdown()       // Temiz kapanış

// IPCHandler.cpp
void IPCHandler::handleRequest(const string& json)  // JSON-RPC işleme
```

---

## 2. Core Modülleri (core/)

### 2.1 Senkronizasyon (core/sync/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `DeltaEngine.cpp/h` | ~600 | Rolling checksum, delta hesaplama |
| `DeltaSyncProtocolHandler.cpp/h` | ~800 | Sync protokolü işleme |
| `FileSyncHandler.cpp/h` | ~400 | Dosya değişiklik broadcast |
| `EventHandlers.cpp/h` | ~300 | EventBus abonelikleri |

### Önemli Fonksiyonlar

```cpp
// DeltaEngine.cpp
vector<BlockSignature> DeltaEngine::computeSignatures(const string& filepath)
vector<DeltaInstruction> DeltaEngine::computeDelta(const string& newFile, 
                                                    const vector<BlockSignature>& sigs)
void DeltaEngine::applyDelta(const string& targetFile, 
                             const vector<DeltaInstruction>& delta)

// DeltaSyncProtocolHandler.cpp
void handleUpdateAvailable(const Message& msg)   // Güncelleme bildirimi
void handleDeltaRequest(const Message& msg)      // Delta talep işleme
void handleDeltaData(const Message& msg)         // Delta veri alma
void handleFileData(const Message& msg)          // Tam dosya alma

// FileSyncHandler.cpp
void FileSyncHandler::broadcastFileUpdate(const string& path)
void FileSyncHandler::broadcastAllFilesToPeer(const string& peerId)
```

### 2.2 Güvenlik (core/security/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `Crypto.cpp/h` | ~500 | AES şifreleme, HMAC |
| `HandshakeProtocol.cpp/h` | ~400 | Peer kimlik doğrulama |
| `KeyManager.cpp/h` | ~200 | Key derivation, session code |

### Önemli Fonksiyonlar

```cpp
// Crypto.cpp
vector<uint8_t> Crypto::encrypt(const vector<uint8_t>& plaintext, 
                                 const vector<uint8_t>& key)
vector<uint8_t> Crypto::decrypt(const vector<uint8_t>& ciphertext, 
                                 const vector<uint8_t>& key)
string Crypto::hmac(const string& data, const vector<uint8_t>& key)

// HandshakeProtocol.cpp
bool HandshakeProtocol::initiateHandshake(TCPConnection& conn)
bool HandshakeProtocol::respondToHandshake(TCPConnection& conn)
```

### 2.3 Network (core/network/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `TCPHandler.cpp/h` | ~600 | TCP bağlantı yönetimi |
| `UDPDiscovery.cpp/h` | ~300 | Peer keşfi (broadcast) |
| `ConnectionPool.cpp/h` | ~250 | Bağlantı havuzu |
| `BandwidthLimiter.cpp/h` | ~150 | Rate limiting |

### Önemli Fonksiyonlar

```cpp
// TCPHandler.cpp
void TCPHandler::connect(const string& ip, int port)
void TCPHandler::send(const string& peerId, const Message& msg)
void TCPHandler::onMessage(const string& peerId, const Message& msg)

// UDPDiscovery.cpp
void UDPDiscovery::startBroadcasting()
void UDPDiscovery::onPeerDiscovered(const string& peerId, const string& ip, int port)
```

### 2.4 Storage (core/storage/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `DatabaseManager.cpp/h` | ~500 | SQLite işlemleri |
| `FileMetadata.cpp/h` | ~200 | Dosya hash/metadata |
| `PeerStore.cpp/h` | ~250 | Peer kayıtları |

### Önemli Fonksiyonlar

```cpp
// DatabaseManager.cpp
void DatabaseManager::initialize()
void DatabaseManager::addFile(const FileInfo& file)
optional<FileInfo> DatabaseManager::getFile(const string& path)
void DatabaseManager::addPeer(const PeerInfo& peer)
vector<PeerInfo> DatabaseManager::getAllPeers()
```

### 2.5 Utils (core/utils/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `EventBus.cpp/h` | ~300 | Pub/sub event sistemi |
| `Logger.cpp/h` | ~200 | Logging altyapısı |
| `Config.cpp/h` | ~250 | INI dosya okuma |
| `ThreadPool.cpp/h` | ~150 | Worker thread havuzu |

---

## 3. Plugins (plugins/)

### 3.1 Network Plugin (plugins/network/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `NetworkPlugin.cpp/h` | ~500 | Plugin ana sınıfı |
| `TCPServer.cpp/h` | ~300 | TCP sunucu |
| `MessageRouter.cpp/h` | ~250 | Mesaj yönlendirme |

### 3.2 Storage Plugin (plugins/storage/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `StoragePlugin.cpp/h` | ~400 | SQLite plugin |
| `SyncQueue.cpp/h` | ~200 | Senkronizasyon kuyruğu |

### 3.3 Filesystem Plugin (plugins/filesystem/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `FilesystemPlugin.cpp/h` | ~500 | inotify izleme |
| `FileWatcher.cpp/h` | ~400 | Dosya değişiklik tespiti |
| `HashCalculator.cpp/h` | ~150 | SHA-256 hash |

### 3.4 ML Plugin (plugins/ml/) [Opsiyonel]

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `MLPlugin.cpp/h` | ~300 | ONNX runtime entegrasyonu |
| `AnomalyDetector.cpp/h` | ~250 | Anomali tespiti |

---

## 4. CLI (app/cli/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `sentinel_cli.cpp` | ~300 | Komut satırı aracı |

### Komutlar

```bash
sentinel_cli status          # Daemon durumu
sentinel_cli peers           # Bağlı peer'lar
sentinel_cli files           # İzlenen dosyalar
sentinel_cli add-watch PATH  # Klasör izlemeye ekleme
sentinel_cli sync PATH       # Manuel senkronizasyon
```

---

## 5. GUI (gui/)

### 5.1 Electron (gui/electron/)

| Dosya | Açıklama |
|-------|----------|
| `main.ts` | Electron ana process |
| `preload.ts` | IPC bridge |

### 5.2 React (gui/src/)

| Dosya | Açıklama |
|-------|----------|
| `App.tsx` | Ana uygulama bileşeni |
| `main.tsx` | React giriş noktası |
| `components/Sidebar.tsx` | Sol navigasyon |
| `components/StatusBar.tsx` | Alt durum çubuğu |
| `components/TransferItem.tsx` | Transfer satırı |
| `pages/Dashboard.tsx` | Ana panel |
| `pages/Files.tsx` | Dosya tarayıcı |
| `pages/Transfers.tsx` | Transfer listesi |
| `pages/Settings.tsx` | Ayarlar |
| `hooks/useDaemon.ts` | IPC hook |
| `contexts/DaemonContext.tsx` | Global state |

---

## 6. Tests (tests/)

| Dosya | Açıklama |
|-------|----------|
| `delta_test.cpp` | Delta algoritması testleri |
| `discovery_test.cpp` | Peer discovery testleri |
| `filesystem_test.cpp` | FileWatcher testleri |
| `storage_test.cpp` | SQLite testleri |
| `transfer_test.cpp` | Dosya transfer testleri |
| `watcher_test.cpp` | inotify testleri |
| `bandwidth_limiter_test.cpp` | Rate limiting testleri |
| `integration/` | End-to-end testler |
| `fuzz/` | Fuzzing testleri |

---

## 7. Build Sistemi

| Dosya | Açıklama |
|-------|----------|
| `CMakeLists.txt` | Ana CMake dosyası |
| `core/CMakeLists.txt` | Core modül build |
| `plugins/CMakeLists.txt` | Plugin build |
| `app/daemon/CMakeLists.txt` | Daemon build |
| `app/cli/CMakeLists.txt` | CLI build |
| `tests/CMakeLists.txt` | Test build |

### Build Komutları

```bash
# Release build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Testleri çalıştır
ctest --output-on-failure
```

---

## 8. Konfigürasyon Dosyaları

| Dosya | Açıklama |
|-------|----------|
| `config/sentinel.conf` | Daemon yapılandırması |
| `peer1.conf` | Peer 1 test config |
| `peer2.conf` | Peer 2 test config |
| `plugins/plugins.conf` | Plugin enable/disable |

---

## 9. Dokümantasyon

| Dosya | Açıklama |
|-------|----------|
| `README.md` | Proje tanıtımı |
| `ARCHITECTURE.md` | Mimari detayları |
| `PLUGIN_GUIDE.md` | Plugin geliştirme |
| `docs/PRODUCTION.md` | Production deployment |
| `docs/security/` | Güvenlik dokümanları |

---

**Toplam Kaynak Kod:** ~15,000+ satır C++ / ~3,000 satır TypeScript
