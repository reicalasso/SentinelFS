# SentinelFS Kaynak Kod İndeksi

**Versiyon:** 1.0.0  
**Tarih:** Aralık 2025

---

## İçindekiler

1. [Genel Bakış](#1-genel-bakış)
2. [Daemon Modülü](#2-daemon-modülü)
3. [Core Kütüphaneler](#3-core-kütüphaneler)
4. [Plugin Sistemi](#4-plugin-sistemi)
5. [GUI Uygulaması](#5-gui-uygulaması)
6. [Test Suite](#6-test-suite)
7. [Build Sistemi](#7-build-sistemi)
8. [Yapılandırma Dosyaları](#8-yapılandırma-dosyaları)

---

## 1. Genel Bakış

### 1.1 Kod İstatistikleri

| Kategori | Satır Sayısı | Dosya Sayısı |
|:---------|:-------------|:-------------|
| **C++ Core/Daemon** | ~16,600 | ~80 |
| **TypeScript/React GUI** | ~3,000 | ~35 |
| **Test Kodu** | ~1,500 | ~20 |
| **Build/Config** | ~500 | ~15 |
| **Toplam** | **~21,600** | **~150** |

### 1.2 Dizin Yapısı Özeti

```
SentinelFS/
├── app/                    # Uygulama giriş noktaları
│   ├── daemon/             # Ana daemon (C++)
│   └── cli/                # Komut satırı aracı
├── core/                   # Core kütüphaneler
│   ├── include/            # Header dosyaları
│   ├── network/            # Ağ modülleri
│   ├── security/           # Güvenlik modülleri
│   ├── sync/               # Senkronizasyon mantığı
│   ├── storage/            # Depolama arayüzleri
│   └── utils/              # Yardımcı fonksiyonlar
├── plugins/                # Plugin implementasyonları
│   ├── filesystem/         # Dosya sistemi izleme
│   ├── network/            # Ağ bağlantıları
│   ├── storage/            # SQLite veritabanı
│   └── ml/                 # Anomali tespiti
├── gui/                    # Electron + React GUI
│   ├── electron/           # Main process
│   └── src/                # React komponetleri
└── tests/                  # Test suite
    ├── unit/               # Birim testleri
    └── integration/        # Entegrasyon testleri
```

---

## 2. Daemon Modülü

### 2.1 Ana Giriş Noktası (`app/daemon/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `sentinel_daemon.cpp` | 419 | Main entry point, CLI argument parsing, daemon başlatma |
| `DaemonCore.h` | 85 | Daemon core header |
| `DaemonCore.cpp` | 535 | Plugin yükleme, EventBus kurulumu, yaşam döngüsü |
| `MetricsServer.h` | 45 | Metrics server header |
| `MetricsServer.cpp` | 126 | Prometheus-uyumlu HTTP endpoint |

### 2.2 IPC Handler (`app/daemon/ipc/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `IPCHandler.h` | 156 | IPC handler header |
| `IPCHandler.cpp` | 1800 | Unix socket server, JSON-RPC command routing |
| `IPCCommands.h` | 95 | Komut tanımları |
| `IPCCommands.cpp` | 450 | Komut implementasyonları |

### 2.3 Event Handlers (`app/daemon/core/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `EventHandlers.h` | 78 | Event handler header |
| `EventHandlers.cpp` | 537 | EventBus subscription'ları, olay işleme |

### 2.4 Önemli Fonksiyonlar

```cpp
// sentinel_daemon.cpp
int main(int argc, char* argv[])
  → Daemon başlatma, argüman parsing, sinyal kurulumu

// DaemonCore.cpp
void DaemonCore::initialize()
  → Plugin loading, EventBus setup, network services başlatma

void DaemonCore::run()
  → Ana event loop, blocking call

void DaemonCore::shutdown()
  → Graceful shutdown, cleanup

// IPCHandler.cpp
void IPCHandler::handleRequest(const std::string& json)
  → JSON-RPC request parsing ve routing

std::string IPCHandler::processCommand(const IPCCommand& cmd)
  → Komut execution ve response oluşturma
```

---

## 3. Core Kütüphaneler

### 3.1 Senkronizasyon (`core/sync/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `DeltaSyncProtocolHandler.h` | 125 | Delta sync protocol header |
| `DeltaSyncProtocolHandler.cpp` | 676 | UPDATE_AVAILABLE, REQUEST_DELTA, DELTA_DATA işleme |
| `FileSyncHandler.h` | 68 | File sync handler header |
| `FileSyncHandler.cpp` | 546 | Dosya değişiklik broadcast, peer sync |
| `ConflictResolver.h` | 54 | Conflict resolver header |
| `ConflictResolver.cpp` | 345 | Last-write-wins, versioning stratejileri |
| `OfflineQueue.h` | 42 | Offline queue header |
| `OfflineQueue.cpp` | 266 | Çevrimdışı olay kuyruğu |

```cpp
// Önemli fonksiyonlar
void DeltaSyncProtocolHandler::handleUpdateAvailable(const Message& msg)
  → Peer'dan gelen güncelleme bildirimi işleme

void DeltaSyncProtocolHandler::handleDeltaRequest(const Message& msg)
  → Delta signature request işleme

void DeltaSyncProtocolHandler::handleDeltaData(const Message& msg)
  → Gelen delta verisi işleme ve dosya güncelleme

void FileSyncHandler::broadcastFileUpdate(const std::string& path)
  → Dosya değişikliğini tüm peer'lara broadcast

void FileSyncHandler::broadcastAllFilesToPeer(const std::string& peerId)
  → Yeni peer'a tüm dosyaları gönder
```

### 3.2 Güvenlik (`core/security/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `Crypto.h` | 78 | Crypto interface header |
| `Crypto.cpp` | 335 | AES-256-CBC, HMAC-SHA256, PBKDF2 |
| `SessionCode.h` | 54 | Session code management |
| `KeyManager.h` | 92 | Key manager header |
| `KeyManager.cpp` | 285 | Key derivation, storage |

```cpp
// Önemli fonksiyonlar
std::vector<uint8_t> Crypto::encrypt(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key)
  → AES-256-CBC şifreleme

std::vector<uint8_t> Crypto::decrypt(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key)
  → AES-256-CBC çözme

std::string Crypto::hmac(
    const std::string& data,
    const std::vector<uint8_t>& key)
  → HMAC-SHA256 hesaplama

std::vector<uint8_t> Crypto::deriveKey(
    const std::string& password,
    const std::string& salt,
    int iterations)
  → PBKDF2 key derivation
```

### 3.3 Network (`core/network/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `DeltaEngine.h` | 86 | Delta engine header |
| `DeltaEngine.cpp` | 258 | Rolling checksum (Adler-32), delta hesaplama |
| `BandwidthLimiter.h` | 52 | Rate limiter header |
| `BandwidthLimiter.cpp` | 338 | Token bucket rate limiting |
| `AutoRemeshManager.h` | 45 | Auto-remesh header |
| `AutoRemeshManager.cpp` | 169 | RTT-based topology optimization |
| `NetworkManager.h` | 38 | Network manager interface |
| `NetworkManager.cpp` | 142 | High-level network operations |

```cpp
// DeltaEngine önemli fonksiyonlar
std::vector<BlockSignature> DeltaEngine::computeSignatures(
    const std::string& filepath,
    size_t blockSize = 4096)
  → Dosya için block signature'ları hesapla

std::vector<DeltaInstruction> DeltaEngine::computeDelta(
    const std::string& newFile,
    const std::vector<BlockSignature>& signatures)
  → Yeni dosya ve signature'lar arasında delta hesapla

void DeltaEngine::applyDelta(
    const std::string& targetFile,
    const std::vector<DeltaInstruction>& delta)
  → Delta instructions uygula
```

### 3.4 Utils (`core/utils/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `EventBus.h` | 65 | Event bus header |
| `EventBus.cpp` | 89 | Pub/sub event distribution |
| `Logger.h` | 52 | Logger header |
| `Logger.cpp` | 135 | Logging altyapısı (file + console) |
| `Config.h` | 48 | Config reader header |
| `Config.cpp` | 145 | INI file parsing |
| `ThreadPool.h` | 45 | Thread pool header |
| `ThreadPool.cpp` | 67 | Worker thread management |
| `MetricsCollector.h` | 85 | Metrics collector header |
| `MetricsCollector.cpp` | 409 | Prometheus metric collection |
| `HealthEndpoint.h` | 42 | Health check header |
| `HealthEndpoint.cpp` | 312 | /healthz, /readyz endpoints |
| `PathUtils.h` | 35 | Path utilities header |
| `PathUtils.cpp` | 95 | XDG paths, path manipulation |

---

## 4. Plugin Sistemi

### 4.1 Network Plugin (`plugins/network/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `network_plugin.cpp` | 318 | Plugin entry point, INetworkAPI implementation |
| `TCPHandler.h` | 95 | TCP handler header |
| `TCPHandler.cpp` | 471 | TCP connection management |
| `UDPDiscovery.h` | 58 | UDP discovery header |
| `UDPDiscovery.cpp` | 279 | Peer broadcast ve discovery |
| `HandshakeProtocol.h` | 72 | Handshake header |
| `HandshakeProtocol.cpp` | 498 | Challenge-response authentication |
| `TCPRelay.h` | 65 | TCP relay header |
| `TCPRelay.cpp` | 496 | NAT traversal relay |

```cpp
// Önemli fonksiyonlar
void TCPHandler::connect(const std::string& ip, int port)
  → TCP bağlantısı kur

void TCPHandler::send(const std::string& peerId, const Message& msg)
  → Peer'a mesaj gönder

void TCPHandler::onMessage(const std::string& peerId, const Message& msg)
  → Gelen mesaj callback

void UDPDiscovery::startBroadcasting()
  → Periyodik broadcast başlat

void UDPDiscovery::onPeerDiscovered(
    const std::string& peerId,
    const std::string& ip,
    int port)
  → Yeni peer keşfedildi callback
```

### 4.2 Storage Plugin (`plugins/storage/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `storage_plugin.cpp` | 148 | Plugin entry point, IStorageAPI implementation |
| `SQLiteHandler.h` | 68 | SQLite handler header |
| `SQLiteHandler.cpp` | 188 | Low-level SQLite operations |
| `FileMetadataManager.h` | 52 | File metadata header |
| `FileMetadataManager.cpp` | 134 | files tablosu CRUD |
| `PeerManager.h` | 48 | Peer manager header |
| `PeerManager.cpp` | 206 | peers tablosu CRUD |
| `ConflictManager.h` | 45 | Conflict manager header |
| `ConflictManager.cpp` | 209 | Conflict records management |

```cpp
// Önemli fonksiyonlar
void SQLiteHandler::initialize()
  → Veritabanı ve tabloları oluştur

void SQLiteHandler::executeQuery(const std::string& query)
  → Raw SQL execution

void FileMetadataManager::addFile(const FileInfo& file)
  → Dosya kaydı ekle

std::optional<FileInfo> FileMetadataManager::getFile(const std::string& path)
  → Dosya kaydı getir

void PeerManager::addPeer(const PeerInfo& peer)
  → Peer kaydı ekle

std::vector<PeerInfo> PeerManager::getAllPeers()
  → Tüm peer'ları getir
```

### 4.3 Filesystem Plugin (`plugins/filesystem/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `filesystem_plugin.cpp` | 179 | Plugin entry point, IFilesystemAPI implementation |
| `InotifyWatcher.h` | 62 | Linux watcher header |
| `InotifyWatcher.cpp` | 242 | inotify-based file watching |
| `Win32Watcher.h` | 45 | Windows watcher header |
| `Win32Watcher.cpp` | 36 | ReadDirectoryChangesW wrapper (stub) |
| `FSEventsWatcher.h` | 45 | macOS watcher header |
| `FSEventsWatcher.cpp` | 36 | FSEvents wrapper (stub) |
| `FileHasher.h` | 32 | File hasher header |
| `FileHasher.cpp` | 25 | SHA-256 file hashing |

```cpp
// Önemli fonksiyonlar
void InotifyWatcher::addWatch(const std::string& path)
  → Klasör izlemeye al

void InotifyWatcher::removeWatch(const std::string& path)
  → İzlemeyi kaldır

void InotifyWatcher::onFileEvent(const FileEvent& event)
  → Dosya olayı callback (create, modify, delete)

std::string FileHasher::hashFile(const std::string& path)
  → Dosya SHA-256 hash hesapla
```

### 4.4 ML Plugin (`plugins/ml/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `ml_plugin.cpp` | 83 | Plugin entry point |
| `AnomalyDetector.h` | 52 | Anomaly detector header |
| `AnomalyDetector.cpp` | 109 | ONNX Isolation Forest inference |

```cpp
// Önemli fonksiyonlar
float AnomalyDetector::predict(const std::vector<float>& features)
  → Anomali skoru hesapla (0-1)

bool AnomalyDetector::isAnomaly(const FileEvent& event)
  → Olay anomali mi kontrol et

void AnomalyDetector::onAnomalyDetected(const AnomalyInfo& info)
  → Anomali tespit callback
```

---

## 5. GUI Uygulaması

### 5.1 Electron Main Process (`gui/electron/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `main.ts` | 185 | Electron ana process, window management |
| `preload.ts` | 65 | Context bridge, IPC exposure |

### 5.2 React Components (`gui/src/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `App.tsx` | 340 | Ana uygulama, routing, context provider |
| `components/Dashboard.tsx` | 517 | Ana panel, metrik kartları, activity feed |
| `components/Files.tsx` | 400 | Dosya tarayıcı, watch folder yönetimi |
| `components/Peers.tsx` | 359 | Peer listesi, bağlantı durumu |
| `components/Transfers.tsx` | 158 | Aktif transfer listesi, progress |
| `components/Settings.tsx` | 661 | Yapılandırma paneli |
| `components/Sidebar.tsx` | 125 | Navigation sidebar |
| `components/StatusBar.tsx` | 85 | Alt durum çubuğu |

### 5.3 Hooks ve Context (`gui/src/hooks/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `useAppState.ts` | 243 | Global state management hook |
| `useDaemon.ts` | 156 | Daemon IPC communication hook |
| `useTheme.ts` | 45 | Theme switching hook |

### 5.4 Type Definitions (`gui/src/types/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `daemon.ts` | 125 | Daemon API type definitions |
| `ipc.ts` | 85 | IPC message types |

---

## 6. Test Suite

### 6.1 Unit Tests (`tests/unit/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `delta_test.cpp` | 100 | Delta algoritması testleri |
| `discovery_test.cpp` | 87 | UDP peer discovery testleri |
| `filesystem_test.cpp` | 88 | FileWatcher testleri |
| `storage_test.cpp` | 115 | SQLite storage testleri |
| `transfer_test.cpp` | 96 | File transfer testleri |
| `watcher_test.cpp` | 75 | inotify watcher testleri |
| `bandwidth_limiter_test.cpp` | 21 | Rate limiting testleri |
| `crypto_test.cpp` | 85 | Encryption testleri |
| `test_file_sync_handler.cpp` | 37 | FileSyncHandler testleri |

### 6.2 Integration Tests (`tests/integration/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `network_integration_test.cpp` | 199 | Ağ entegrasyon testleri |
| `sync_integration_test.cpp` | 188 | Senkronizasyon entegrasyon testleri |
| `autoremsh_integ_test.cpp` | 149 | Auto-remesh entegrasyon testleri |

### 6.3 Fuzz Tests (`tests/fuzz/`)

| Dosya | Satır | Açıklama |
|:------|:------|:---------|
| `protocol_fuzz.cpp` | 95 | Protocol message fuzzing |
| `delta_fuzz.cpp` | 78 | Delta algorithm fuzzing |

---

## 7. Build Sistemi

### 7.1 CMake Dosyaları

| Dosya | Açıklama |
|:------|:---------|
| `CMakeLists.txt` | Ana CMake dosyası, proje tanımı |
| `core/CMakeLists.txt` | Core kütüphane build |
| `plugins/*/CMakeLists.txt` | Plugin build dosyaları |
| `app/daemon/CMakeLists.txt` | Daemon executable build |
| `app/cli/CMakeLists.txt` | CLI executable build |
| `tests/CMakeLists.txt` | Test suite build |

### 7.2 Build Komutları

```bash
# Debug build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --parallel

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel $(nproc)

# Testleri çalıştır
ctest --output-on-failure

# AppImage oluştur
cmake --build . --target appimage
```

---

## 8. Yapılandırma Dosyaları

### 8.1 Config Dosyaları

| Dosya | Konum | Açıklama |
|:------|:------|:---------|
| `sentinel.conf` | `config/` | Ana daemon yapılandırması |
| `peer1.conf` | `config/` | Test peer 1 yapılandırması |
| `peer2.conf` | `config/` | Test peer 2 yapılandırması |
| `plugins.conf` | `config/` | Plugin enable/disable |

### 8.2 Örnek Yapılandırma

```ini
[daemon]
peer_id = PEER_AUTO
tcp_port = 8081
udp_port = 8888
metrics_port = 9100
log_level = INFO

[security]
session_code = GZWTFP
encryption = true

[sync]
watch_directory = /home/user/sync
ignore_patterns = .git,*.tmp,*.swp
delta_enabled = true
block_size = 4096

[network]
max_connections = 10
broadcast_interval = 30
reconnect_interval = 15

[bandwidth]
upload_limit = 0
download_limit = 0

[ml]
anomaly_detection = true
threshold = 0.8
```

---

## Özet İstatistikler

| Modül | C++ Satır | TS/JS Satır | Toplam |
|:------|:----------|:------------|:-------|
| Daemon | 4,200 | - | 4,200 |
| Core | 5,800 | - | 5,800 |
| Plugins | 4,100 | - | 4,100 |
| GUI | - | 3,000 | 3,000 |
| Tests | 1,500 | 200 | 1,700 |
| **Toplam** | **15,600** | **3,200** | **18,800** |

---

**Kaynak Kod İndeksi Sonu**

*SentinelFS Development Team - Aralık 2025*
