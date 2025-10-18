# SentinelFS-Neo

> **Distributed P2P File Synchronization System with Auto-Remesh Technology**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C++-17-00599C.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)](https://github.com)

---

## 📖 İçindekiler

- [Proje Hakkında](#-proje-hakkında)
- [Özellikler](#-özellikler)
- [Mimari](#-mimari)
- [Kurulum](#-kurulum)
- [Kullanım](#-kullanım)
- [Akademik Bağlantılar](#-akademik-bağlantılar)
- [Geliştirme](#-geliştirme)
- [Katkıda Bulunma](#-katkıda-bulunma)
- [Lisans](#-lisans)

---

## 🚀 Proje Hakkında

**SentinelFS-Neo**, birden fazla cihaz arasında **gerçek zamanlı P2P dosya senkronizasyonu** sağlayan, **otomatik ağ optimizasyonu** (auto-remesh) özellikli, hafif ve modüler bir dağıtık dosya sistemi çekirdeğidir.

### 🎯 Temel Konsept

Her cihaz aynı **session code** ile ağa katıldığında:
- Sistem otomatik olarak **en düşük gecikmeli** peer'larla mesh ağı kurar
- Ağ koşullarına göre **dinamik olarak yeniden yapılanır** (auto-remesh)
- Dosya değişikliklerini **delta tabanlı algoritma** ile verimli şekilde senkronize eder
- Sadece **değişen kısımları** transfer ederek bant genişliği tasarrufu sağlar

### 🎓 Akademik Proje Kapsamı

Bu proje, **üç temel bilgisayar bilimleri dersi**nin entegrasyonunu gösterir:

| Ders | Kapsam | Kullanılan Teknolojiler |
|------|--------|------------------------|
| **💾 Veritabanı** | Metadata yönetimi, veri bütünlüğü, transaction | SQLite, KV Store, Hash Tables |
| **🌐 Ağ (Networking)** | Peer discovery, mesh topoloji, latency optimizasyonu | UDP/TCP, NAT Traversal, Auto-Remesh |
| **⚙️ İşletim Sistemleri** | File watching, concurrency, thread management | inotify, Threads, Mutex/Lock |

---

## ✨ Özellikler

### 🔧 Temel Özellikler

- **🌐 P2P Peer Discovery**: Aynı session code'a sahip cihazları otomatik keşfeder
- **⚡ Auto-Remesh**: Ping sürelerine göre ağ topolojisini optimize eder
- **📁 Delta-Based Sync**: Sadece değişen dosya parçalarını transfer eder
- **💾 Metadata Management**: Hash, timestamp ve cihaz bilgilerini yönetir
- **🔒 Secure Session**: Shared-key tabanlı oturum güvenliği
- **🔄 Real-Time Monitoring**: Dosya değişikliklerini gerçek zamanlı izler
- **📊 Conflict Resolution**: Çakışma çözme mekanizması
- **🎯 Multi-threaded**: Paralel işlem desteği

### 🚀 Gelişmiş Özellikler

- **Dynamic Network Adaptation**: Ağ koşullarına göre adaptif davranış
- **Bandwidth Optimization**: Akıllı veri transfer algoritmaları
- **Fault Tolerance**: Peer kaybında otomatik kurtarma
- **Modular Architecture**: Kolay genişletilebilir yapı

---

## 🧠 Mimari

### Katmanlı Mimari

```
┌─────────────────────────────────────────────────────┐
│           Application Layer                          │
│  CLI / Config / Session Management / Logger          │
├─────────────────────────────────────────────────────┤
│           File System Layer                          │
│  File Watcher / Delta Engine / Sync Queue            │
├─────────────────────────────────────────────────────┤
│           Network Layer                              │
│  Peer Discovery / Auto-Remesh / Transfer Engine      │
├─────────────────────────────────────────────────────┤
│           Storage Layer                              │
│  Metadata DB / Hash Store / Device Cache             │
└─────────────────────────────────────────────────────┘
```

### Modüller

#### **Application Layer**
- `main.cpp` - Uygulama giriş noktası
- `cli.cpp/hpp` - Komut satırı arayüzü
- `logger.cpp/hpp` - Loglama sistemi
- `config.json` - Konfigürasyon dosyası

#### **File System Layer**
- `watcher.cpp/hpp` - Dosya değişiklik izleme
- `delta_engine.cpp/hpp` - Delta hesaplama motoru
- `file_queue.cpp/hpp` - Senkronizasyon kuyruğu

#### **Network Layer**
- `peer_discovery.cpp/hpp` - Peer keşif sistemi
- `mesh_manager.cpp/hpp` - Auto-remesh yönetimi
- `transfer_engine.cpp/hpp` - Veri transfer motoru

#### **Storage Layer**
- `metadata_db.cpp/hpp` - Metadata veritabanı
- `hash_store.cpp/hpp` - Hash yönetimi
- `device_cache.cpp/hpp` - Cihaz bilgi önbelleği

Detaylı mimari bilgiler için: [📄 Architecture Documentation](docs/architecture.md)

---

## 🔧 Kurulum

### Gereksinimler

- **C++ Compiler**: GCC 7+ veya Clang 6+ (C++17 desteği)
- **CMake**: 3.12 veya üzeri
- **SQLite3**: 3.x
- **OpenSSL**: 1.1+ (opsiyonel, gelişmiş güvenlik için)

### Linux/macOS

```bash
# Repository'yi klonlayın
git clone https://github.com/kullaniciadi/sentinelfs-neo.git
cd sentinelfs-neo

# Build dizini oluşturun
mkdir build && cd build

# CMake ile yapılandırın
cmake ..

# Derleyin
make -j$(nproc)

# Kurulum (opsiyonel)
sudo make install
```

### Windows

```powershell
# Visual Studio 2019+ kullanarak
git clone https://github.com/kullaniciadi/sentinelfs-neo.git
cd sentinelfs-neo
mkdir build && cd build
cmake -G "Visual Studio 16 2019" ..
cmake --build . --config Release
```

---

## 🎮 Kullanım

### Hızlı Başlangıç

```bash
# İlk cihazı başlatın
./sentinelfs-neo --session DEMO-2024-XYZ --path /path/to/sync

# İkinci cihazda aynı session code ile bağlanın
./sentinelfs-neo --session DEMO-2024-XYZ --path /another/path
```

### Komut Satırı Seçenekleri

```
Usage: sentinelfs-neo [OPTIONS]

Options:
  --session <CODE>      Session code (zorunlu)
  --path <PATH>         Senkronize edilecek dizin
  --port <PORT>         Network portu (varsayılan: 8080)
  --verbose             Detaylı loglama
  --config <FILE>       Konfigürasyon dosyası
  --daemon              Arka plan servisi olarak çalış
  --help                Yardım mesajı
```

### Konfigürasyon Örneği

```json
{
  "session": {
    "code": "SENT-2024-XYZ",
    "encryption": true
  },
  "network": {
    "port": 8080,
    "discovery_interval": 5000,
    "remesh_threshold": 100
  },
  "sync": {
    "watch_interval": 1000,
    "delta_algorithm": "rsync",
    "conflict_resolution": "timestamp"
  },
  "storage": {
    "metadata_db": "~/.sentinelfs/metadata.db",
    "cache_size": 1024
  }
}
```

### Kullanım Senaryoları

Detaylı kullanım örnekleri için: [📄 Use Cases](docs/usecases.md)

---

## 🎓 Akademik Bağlantılar

### Veritabanı Dersi İçin

- **Metadata Yönetimi**: Dosya hash'leri ve metadata'nın SQLite ile yönetimi
- **Transaction Management**: ACID özellikleri ve conflict resolution
- **Indexing**: Hash tabanlı hızlı arama mekanizmaları
- **Cache Stratejileri**: Cihaz bilgilerinin önbelleklenmesi

### Ağ Dersi İçin

- **Peer Discovery**: UDP broadcast ile cihaz keşfi
- **Mesh Topology**: Dinamik ağ topolojisi oluşturma
- **Auto-Remesh Algorithm**: Latency tabanlı ağ optimizasyonu
- **NAT Traversal**: Güvenlik duvarı arkası bağlantı
- **Protocol Design**: Custom P2P protokol tasarımı

### İşletim Sistemleri Dersi İçin

- **File System Monitoring**: inotify/FSEvents kullanımı
- **Multi-threading**: Producer-consumer pattern uygulaması
- **Synchronization**: Mutex, locks ve thread-safe yapılar
- **I/O Management**: Asenkron dosya işlemleri
- **Process Communication**: IPC mekanizmaları

Detaylı bilgi için: [📄 Development Plan](docs/development.plan.md)

---

## 🛠️ Geliştirme

### Test Ortamı Kurulumu

```bash
# Test bağımlılıklarını yükleyin
cd tests
./setup_test_env.sh

# Testleri çalıştırın
./run_tests.sh
```

### Debug Modu

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./sentinelfs-neo --verbose --debug
```

### Dokümantasyon

- [📘 Architecture](docs/architecture.md) - Mimari detayları
- [📗 Development Plan](docs/development.plan.md) - Geliştirme planı
- [📙 Test Plan](docs/test.plan.md) - Test senaryoları
- [📕 API Reference](docs/example.codes.and.api.plan.md) - API dokümantasyonu
- [📔 Future Plans](docs/future.plans.and.summary.md) - Gelecek planları

---

## 🤝 Katkıda Bulunma

Katkılarınızı bekliyoruz! Lütfen şu adımları takip edin:

1. Fork edin
2. Feature branch oluşturun (`git checkout -b feature/amazing-feature`)
3. Değişikliklerinizi commit edin (`git commit -m 'Add amazing feature'`)
4. Branch'inizi push edin (`git push origin feature/amazing-feature`)
5. Pull Request açın

Detaylar için: [CONTRIBUTING.md](CONTRIBUTING.md)

---

## 📝 Lisans

Bu proje MIT lisansı altında lisanslanmıştır. Detaylar için [LICENSE](LICENSE) dosyasına bakın.

---

## � Yazarlar

- **Proje Sahibi** - [GitHub Profili](https://github.com/kullaniciadi)

---

## 🙏 Teşekkürler

- Tüm katkıda bulunanlara
- Akademik danışmanlara
- Açık kaynak topluluğuna

---

## 📞 İletişim

- **Issues**: [GitHub Issues](https://github.com/kullaniciadi/sentinelfs-neo/issues)
- **Discussions**: [GitHub Discussions](https://github.com/kullaniciadi/sentinelfs-neo/discussions)

---

<div align="center">

**⭐ Beğendiyseniz yıldız vermeyi unutmayın! ⭐**

Made with ❤️ for academic and practical learning

</div>