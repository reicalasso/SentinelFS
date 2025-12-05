# SentinelFS Kaynak Kod İndeksi

Bu doküman projenin ana kaynak kod dosyalarını ve işlevlerini açıklar.

---

## 1. Daemon (app/daemon/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `sentinel_daemon.cpp` | 419 | Ana giriş noktası, daemon başlatma |
| `DaemonCore.cpp/h` | 620 | Daemon yaşam döngüsü, sinyal yönetimi |
| `IPCHandler.cpp/h` | 1956 | GUI ile IPC iletişimi (Unix socket/Named Pipe) |
| `MetricsServer.cpp/h` | 171 | Prometheus HTTP endpoint |

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
| `DeltaSyncProtocolHandler.cpp/h` | 801 | Sync protokolü işleme |
| `FileSyncHandler.cpp/h` | 614 | Dosya değişiklik broadcast |
| `EventHandlers.cpp/h` | 615 | EventBus abonelikleri |
| `ConflictResolver.cpp/h` | 399 | Çakışma çözme mantığı |
| `OfflineQueue.cpp/h` | 308 | Çevrimdışı olay kuyruğu |

### Önemli Fonksiyonlar

```cpp
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
| `Crypto.cpp/h` | 413 | AES şifreleme, HMAC |
| `SessionCode.h` | 54 | Oturum kodu yönetimi |

### Önemli Fonksiyonlar

```cpp
// Crypto.cpp
vector<uint8_t> Crypto::encrypt(const vector<uint8_t>& plaintext, 
                                 const vector<uint8_t>& key)
vector<uint8_t> Crypto::decrypt(const vector<uint8_t>& ciphertext, 
                                 const vector<uint8_t>& key)
string Crypto::hmac(const string& data, const vector<uint8_t>& key)
```

### 2.3 Network (core/network/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `DeltaEngine.cpp/h` | 344 | Rolling checksum, delta hesaplama |
| `BandwidthLimiter.cpp/h` | 390 | Rate limiting |
| `AutoRemeshManager.cpp/h` | 214 | Otomatik ağ yeniden yapılandırma |
| `NetworkManager.cpp/h` | 180 | Ağ yönetimi arayüzü |

### Önemli Fonksiyonlar

```cpp
// DeltaEngine.cpp
vector<BlockSignature> DeltaEngine::computeSignatures(const string& filepath)
vector<DeltaInstruction> DeltaEngine::computeDelta(const string& newFile, 
                                                    const vector<BlockSignature>& sigs)
void DeltaEngine::applyDelta(const string& targetFile, 
                             const vector<DeltaInstruction>& delta)
```

### 2.4 Utils (core/utils/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `EventBus.cpp/h` | 154 | Pub/sub event sistemi |
| `Logger.cpp/h` | 187 | Logging altyapısı |
| `Config.cpp/h` | 193 | INI dosya okuma |
| `ThreadPool.cpp/h` | 112 | Worker thread havuzu |
| `MetricsCollector.cpp/h` | 494 | Metrik toplama |
| `HealthEndpoint.cpp/h` | 354 | Sağlık kontrolü endpoint'i |

---

## 3. Plugins (plugins/)

### 3.1 Network Plugin (plugins/network/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `network_plugin.cpp` | 318 | Plugin ana sınıfı |
| `TCPHandler.cpp/h` | 566 | TCP bağlantı yönetimi |
| `UDPDiscovery.cpp/h` | 337 | Peer keşfi (broadcast) |
| `TCPRelay.cpp/h` | 561 | TCP relay sunucusu |
| `HandshakeProtocol.cpp/h` | 570 | Peer kimlik doğrulama |

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

### 3.2 Storage Plugin (plugins/storage/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `storage_plugin.cpp` | 148 | Plugin ana sınıfı |
| `SQLiteHandler.cpp/h` | 256 | SQLite veritabanı işlemleri |
| `FileMetadataManager.cpp/h` | 186 | Dosya metadata yönetimi |
| `PeerManager.cpp/h` | 254 | Peer kayıtları yönetimi |
| `ConflictManager.cpp/h` | 254 | Çakışma kayıtları yönetimi |

### Önemli Fonksiyonlar

```cpp
// SQLiteHandler.cpp
void SQLiteHandler::initialize()
void SQLiteHandler::executeQuery(const string& query)

// FileMetadataManager.cpp
void FileMetadataManager::addFile(const FileInfo& file)
optional<FileInfo> FileMetadataManager::getFile(const string& path)
```

### 3.3 Filesystem Plugin (plugins/filesystem/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `filesystem_plugin.cpp` | 179 | Plugin ana sınıfı |
| `InotifyWatcher.cpp/h` | 304 | Linux dosya izleme (inotify) |
| `Win32Watcher.cpp/h` | 81 | Windows dosya izleme |
| `FSEventsWatcher.cpp/h` | 81 | macOS dosya izleme |
| `FileHasher.cpp/h` | 57 | Dosya hash hesaplama |

### 3.4 ML Plugin (plugins/ml/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `ml_plugin.cpp` | 83 | Plugin ana sınıfı |
| `AnomalyDetector.cpp/h` | 161 | Anomali tespiti |

---

## 4. CLI (app/cli/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `sentinel_cli.cpp` | 22 | Komut satırı aracı (Placeholder) |

---

## 5. GUI (gui/)

### 5.1 Electron (gui/electron/)

| Dosya | Açıklama |
|-------|----------|
| `main.ts` | Electron ana process |
| `preload.ts` | IPC bridge |

### 5.2 React (gui/src/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `App.tsx` | 340 | Ana uygulama bileşeni |
| `components/Dashboard.tsx` | 517 | Ana panel |
| `components/Files.tsx` | 400 | Dosya tarayıcı |
| `components/Peers.tsx` | 359 | Peer listesi |
| `components/Settings.tsx` | 661 | Ayarlar |
| `components/Transfers.tsx` | 158 | Transfer listesi |
| `hooks/useAppState.ts` | 243 | Uygulama durumu hook'u |

---

## 6. Tests (tests/)

| Dosya | Satır | Açıklama |
|-------|-------|----------|
| `delta_test.cpp` | 100 | Delta algoritması testleri |
| `discovery_test.cpp` | 87 | Peer discovery testleri |
| `filesystem_test.cpp` | 88 | FileWatcher testleri |
| `storage_test.cpp` | 115 | SQLite testleri |
| `transfer_test.cpp` | 96 | Dosya transfer testleri |
| `watcher_test.cpp` | 75 | inotify testleri |
| `bandwidth_limiter_test.cpp` | 21 | Rate limiting testleri |
| `integration/network_integration_test.cpp` | 199 | Ağ entegrasyon testleri |
| `integration/sync_integration_test.cpp` | 188 | Senkronizasyon entegrasyon testleri |
| `autoremsh_integ_test.cpp` | 149 | Auto-remesh testleri |

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
| `docs/PRODUCTION.md` | Production deployment |
| `docs/security/` | Güvenlik dokümanları |

---

**Toplam Kaynak Kod:** ~16,600+ satır C++ / ~3,000 satır TypeScript
