# NetFalcon Network Plugin

NetFalcon, SentinelFS için modüler, çok taşıyıcılı ve yüksek performanslı bir ağ eklentisidir.

## Özellikler

### Çok Taşıyıcı Desteği
- **TCP**: Varsayılan, güvenilir bağlantılar
- **QUIC**: (Gelecek) Düşük gecikmeli UDP tabanlı
- **WebRTC**: (Gelecek) NAT traversal için
- **Relay**: NAT arkasındaki cihazlar için

### Akıllı Taşıyıcı Seçimi
- `PREFER_DIRECT`: Doğrudan bağlantıları tercih et
- `PREFER_RELIABLE`: En düşük paket kaybı
- `PREFER_FAST`: En düşük RTT
- `FALLBACK_CHAIN`: Sırayla dene
- `ADAPTIVE`: Dinamik seçim

### Güvenlik
- Session code doğrulama
- AES-256 şifreleme
- HMAC-SHA256 bütünlük kontrolü
- Replay attack koruması
- Anahtar döndürme desteği

### Keşif Servisi
- UDP broadcast (LAN)
- mDNS/Bonjour (gelecek)
- Relay server discovery

### Bant Genişliği Yönetimi
- Global upload/download limitleri
- Per-peer limitler
- Token bucket algoritması

## Mimari

```
┌─────────────────────────────────────────────────────────────┐
│                    NetFalconPlugin                          │
│                   (INetworkAPI impl)                        │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Transport   │  │  Session    │  │    Discovery        │  │
│  │ Registry    │  │  Manager    │  │    Service          │  │
│  └──────┬──────┘  └─────────────┘  └─────────────────────┘  │
│         │                                                   │
│  ┌──────┴──────────────────────────────────────────────┐    │
│  │              Transport Layer                         │    │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐ │    │
│  │  │   TCP   │  │  QUIC   │  │ WebRTC  │  │  Relay  │ │    │
│  │  └─────────┘  └─────────┘  └─────────┘  └─────────┘ │    │
│  └──────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

## Kullanım

### Daemon ile Kullanım

`plugins.conf` dosyasında:
```
network=./build/plugins/netfalcon/libnetfalcon.so
```

### Programatik Kullanım

```cpp
#include "NetFalconPlugin.h"

using namespace SentinelFS::NetFalcon;

// Plugin oluştur
auto plugin = std::make_unique<NetFalconPlugin>();
plugin->initialize(&eventBus);

// Yapılandır
plugin->setSessionCode("ABC123");
plugin->setEncryptionEnabled(true);

// Dinlemeye başla
plugin->startListening(8000);
plugin->startDiscovery(9999);

// Peer'a bağlan
plugin->connectToPeer("192.168.1.100", 8000);

// Veri gönder
std::vector<uint8_t> data = {...};
plugin->sendData("FALCON_12345", data);
```

## Yapılandırma

```cpp
NetFalconConfig config;
config.transportStrategy = TransportStrategy::ADAPTIVE;
config.enableTcp = true;
config.enableRelay = true;
config.encryptionEnabled = true;
config.maxConnections = 64;
config.autoReconnect = true;
config.globalUploadLimit = 1024 * 1024;  // 1 MB/s
config.globalDownloadLimit = 0;          // Unlimited

plugin->setConfig(config);
```

## Derleme

```bash
cd SentinelFS
mkdir -p build && cd build
cmake .. -DSKIP_SYSTEM_INSTALL=ON
make netfalcon -j4
```

## Dosya Yapısı

```
plugins/netfalcon/
├── include/
│   ├── ITransport.h          # Transport soyutlaması
│   ├── TransportRegistry.h   # Transport yönetimi
│   ├── SessionManager.h      # Oturum ve güvenlik
│   ├── DiscoveryService.h    # Peer keşfi
│   ├── TCPTransport.h        # TCP implementasyonu
│   └── NetFalconPlugin.h     # Ana plugin
├── src/
│   ├── TransportRegistry.cpp
│   ├── SessionManager.cpp
│   ├── DiscoveryService.cpp
│   ├── TCPTransport.cpp
│   └── NetFalconPlugin.cpp
├── CMakeLists.txt
└── README.md
```

## Eski NetworkPlugin ile Farklar

| Özellik | NetworkPlugin | NetFalcon |
|---------|---------------|-----------|
| Taşıyıcı | Sadece TCP | TCP + QUIC + WebRTC + Relay |
| Taşıyıcı Seçimi | Sabit | Akıllı/Adaptif |
| Modülerlik | Monolitik | Katmanlı |
| Multi-session | Hayır | Evet |
| Anahtar Döndürme | Hayır | Evet |
| Replay Koruması | Hayır | Evet |
| mDNS Keşfi | Hayır | Planlandı |

## Lisans

Apache-2.0
