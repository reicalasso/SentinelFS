# 🛡️ SentinelFS

**Güvenli P2P Dosya Senkronizasyon Sistemi**

SentinelFS, merkezi sunucu gerektirmeyen, cihazlar arasında doğrudan senkronizasyon sağlayan güvenli bir dosya senkronizasyon sistemidir. Askeri sınıfı şifreleme ile verilerinizi özel ve güvende tutar.

---

## ✨ Özellikler

### 🔒 Güvenlik
- **Uçtan Uca Şifreleme:** AES-256-CBC ile tüm transferler şifrelenir
- **Sıfır Bilgi:** Merkezi sunucu veri veya meta veri depolamaz
- **Oturum Kodları:** Sadece yetkili cihazların ağa katılmasını sağlar

### ⚡ Performans
- **Delta Sync Motoru:** Sadece değişen blokları transfer eder, %99 bant genişliği tasarrufu
- **Otomatik Ağ Yönetimi:** Bağlantıları otomatik olarak iyileştirir ve optimize eder
- **Düşük Kaynak Kullanımı:** Arka planda verimli çalışır

### 🧩 Modüler Mimari
- **Eklenti Sistemi:** Temel işlevler bağımsız eklentiler olarak ayrıştırılmıştır
- **Makine Öğrenmesi:** Anomali tespiti ve fidye yazılımı koruması

---

## 🏗️ Teknoloji

### Backend (Daemon)
- **C++17/20** - Modern C++
- **Boost.Asio** - Asenkron I/O
- **SQLite3** - Veri depolama
- **OpenSSL** - Şifreleme
- **ONNX Runtime** - ML modelleri

### Frontend (GUI)
- **Electron** - Çapraz platform arayüz
- **React 18** - Modern UI
- **TypeScript** - Tip güvenliği
- **TailwindCSS** - Stiller

---

## � Proje Yapısı

```
SentinelFS/
├── app/                  # Uygulama Giriş Noktaları
│   ├── daemon/           # Ana C++ Servisi
│   └── cli/              # Komut Satırı Arayüzü
├── core/                 # Çekirdek Kütüphaneler
│   ├── network/          # Delta Sync, Bant Genişliği
│   ├── security/         # Şifreleme, Oturum Yönetimi
│   ├── sync/             # Dosya İzleyici, Çakışma Çözümü
│   └── utils/            # ThreadPool, Logger, Config
├── plugins/              # Modüler Eklentiler
│   ├── filesystem/       # Dosya İzleyiciler
│   ├── network/          - TCP/UDP Yönetimi
│   ├── storage/          # Veritabanı İşlemleri
│   └── ml/               # Anomali Tespiti
├── gui/                  # Grafiksel Arayüz
│   ├── electron/         # Ana Süreç
│   └── src/              # Renderer Süreç (React)
└── tests/                # Test Suite
```

---

## � Kurulum ve Çalıştırma

### Gereksinimler
- CMake 3.15+
- C++ Derleyici (GCC 9+, Clang 10+)
- Node.js 16+ & npm
- OpenSSL, SQLite3, Boost (Asio)

### Hızlı Başlangıç
```bash
# Tek komutla derleme ve çalıştırma:
./scripts/start_safe.sh

# Seçeneklerle:
./scripts/start_safe.sh --daemon-only   # Sadece daemon
./scripts/start_safe.sh --rebuild       # Temiz derleme
```

### Manuel Derleme

**Daemon Derleme:**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

**Daemon Çalıştırma:**
```bash
./scripts/run_daemon.sh
# Veya manuel:
SENTINELFS_PLUGIN_DIR=./build/plugins \
LD_LIBRARY_PATH=./build/core:$LD_LIBRARY_PATH \
./build/app/daemon/sentinel_daemon
```

**GUI Derleme ve Çalıştırma:**
```bash
cd gui
npm install
npm run dev    # Geliştirme modu
npm run build  # Prodüksiyon
```

### Yapılandırma
- Konfigürasyon: `~/.config/sentinelfs/sentinel.conf`
- Senkronizasyon klasörü: `~/SentinelFS` (varsayılan)

---

## � İstatistikler

| Metrik | Değer |
|:-------|:------|
| **C++ Kod Tabanı** | ~16,600 Satır |
| **TypeScript Kod Tabanı** | ~3,000 Satır |
| **Toplam Kaynak Dosya** | ~150 Dosya |
| **Mimari** | Eklenti Tabanlı P2P Mesh |
| **Şifreleme** | AES-256-CBC + HMAC |

---

## 📄 Lisans

Bu proje **SPL-1.0** lisansı altındadır. Detaylar için [LICENSE](LICENSE) dosyasına bakın.

---

*SentinelFS Team - Aralık 2025*