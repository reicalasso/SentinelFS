# SentinelFS Ağ Sistemi Proje Teslimatları

**Ders:** Ağ Programlama  
**Proje:** SentinelFS P2P Dosya Senkronizasyon Sistemi  
**Tarih:** Aralık 2025

---

## 🎯 Proje Hedefleri

Bu projenin temel amaçları:
1. Merkezi sunucu olmadan çalışan P2P dosya senkronizasyon sistemi geliştirmek
2. AES-256 şifreleme ile uçtan uca güvenlik sağlamak
3. Delta sync algoritması ile %99+ bant genişliği tasarrufu elde etmek
4. Modern C++ ve TypeScript ile çapraz platform mimari oluşturmak

---

## � İlgili Çalışmalar

Mevcut P2P senkronizasyon sistemleri:

| Sistem | Avantajları | Dezavantajları | SentinelFS Farkı |
|:-------|:------------|:---------------|:-----------------|
| **Syncthing** | Açık kaynak, stabil | Karmaşık yapılandırma | Daha basit UI, ML tabanlı güvenlik |
| **Resilio Sync** | Hızlı, kullanıcı dostu | Kapalı kaynak, merkezi bağımlılık | Tamamen merkeziyetsiz, açık kaynak |
| **Dropbox (LAN Sync)** | Güvenilir | Bulut gerektirir | Sadece P2P, gizlilik odaklı |

---

## � Proje Özeti

SentinelFS, merkezi sunucu gerektirmeyen, cihazlar arasında doğrudan dosya senkronizasyonu sağlayan güvenli bir P2P sistemdir. Proje, modern ağ programlama tekniklerini kullanarak yüksek performanslı ve güvenli bir senkronizasyon altyapısı sunar.

---

## 🏗️ Sistem Mimarisi

### Ağ Topolojisi
```
┌─────────────────────────────────────────────────────────────┐
│                    P2P Mesh Ağı                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐                │
│   │ Node A  │────│ Node B  │────│ Node C  │                │
│   │ 8082    │    │ 8082    │    │ 8082    │                │
│   └────┬────┘    └────┬────┘    └────┬────┘                │
│        │              │              │                     │
│        └──────┬───────┘              │                     │
│               │ UDP Discovery        │                     │
│               ▼ 8083                 │                     │
│        ┌─────────────────┐          │                     │
│        │ Broadcast Domain │          │                     │
│        └─────────────────┘          │                     │
│                                       ▼                     │
│                               ┌─────────────┐              │
│                               │ Relay Server│ (NAT arkası) │
│                               │    8090     │              │
│                               └─────────────┘              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Katmanlı Mimari
1. **Uygulama Katmanı**: GUI ve CLI arayüzleri
2. **Senkronizasyon Katmanı**: Dosya değişikliği takibi ve delta sync
3. **Ağ Katmanı**: P2P iletişim ve keşif
4. **Güvenlik Katmanı**: Şifreleme ve kimlik doğrulama
5. **Depolama Katmanı**: Veritabanı ve dosya sistemi

---

## 🌐 Ağ Protokolleri

### 1. Peer Discovery (UDP)
- **Port**: 8083
- **Protokol**: UDP Broadcast
- **Format**: JSON
- **Sıklık**: 30 saniye

```json
{
  "type": "DISCOVERY",
  "peer_id": "PEER_82844",
  "version": "1.0.0",
  "listen_port": 8082,
  "session_code": "ABC123"
}
```

### 2. Peer Communication (TCP)
- **Port**: 8082
- **Protokol**: TCP
- **Format**: Length-prefixed JSON

#### Handshake
```json
{
  "type": "HANDSHAKE",
  "peer_id": "PEER_82844",
  "session_code": "ABC123",
  "auth_token": "sha256_hmac"
}
```

#### File Transfer
```json
{
  "type": "FILE_TRANSFER",
  "file_id": "FILE_001",
  "operation": "SYNC|DELETE|RENAME",
  "chunks": [
    {
      "index": 0,
      "offset": 0,
      "size": 4096,
      "hash": "sha256_hash"
    }
  ]
}
```

### 3. Relay Protocol (WebSocket)
- **Port**: 8090
- **Protokol**: WebSocket
- **Kullanım**: NAT arkasındaki peer'lar

---

## 🔒 Güvenlik Mimarisi

### Şifreleme
- **Algoritma**: AES-256-CBC
- **Anahtar Yönetimi**: Diffie-Hellman key exchange
- **Bütünlük**: HMAC-SHA256

### Kimlik Doğrulama
1. Session code ile ilk eşleşme
2. HMAC token ile sürekli doğrulama
3. Public key fingerprint ile güvenli bağlantı

---

## 📊 Performans Metrikleri

### Bandwidth Optimizasyonu
- **Delta Sync**: Sadece değişen bloklar transfer edilir
- **Compression**: LZ4 sıkıştırma
- **Deduplication**: Aynı hash'li bloklar tekrar transfer edilmez

### Gecikme Optimizasyonu
- **Connection Pooling**: Maksimum 10 eşzamanlı bağlantı
- **Pipeline**: Paralel chunk transferi
- **Smart Retry**: Adaptif timeout ile yeniden deneme

---

## 🛠️ Implementation Detayları

### C++ Backend (Boost.Asio)
```cpp
// Asenkron network manager
class NetworkManager {
    boost::asio::io_context io_context_;
    udp::socket discovery_socket_;
    tcp::acceptor acceptor_;
    
    void start_discovery();
    void handle_peer_connect();
    void process_message();
};
```

### TypeScript Frontend (Electron)
```typescript
// IPC Communication
interface IPCMessage {
  type: string;
  payload: any;
}

// Network Status
interface NetworkStatus {
  connectedPeers: number;
  uploadSpeed: number;
  downloadSpeed: number;
  pendingTransfers: number;
}
```

---

## 📈 Test Sonuçları

### Performans Testleri
| Metrik | Değer |
|--------|-------|
| Dosya Transfer Hızı | 85 MB/s (LAN) |
| Delta Sync Tasarrufu | %99.2 |
| Peer Keşif Süresi | < 3 saniye |
| Bağlantı Kurma | 150 ms |
| Concurrent Connections | 50+ |

### Güvenlik Testleri
- ✅ Man-in-the-middle saldırıları engellendi
- ✅ Session code olmadan bağlantı kurulamadı
- ✅ Dosya bütünlüğü korundu
- ✅ Replay attack'lar önlendi

---

## � Metodoloji

Proje geliştirme süreci:

1. **Analiz Fazı (2 Hafta)**
   - Mevcut P2P sistemlerinin incelenmesi
   - Protokol tasarımı ve güvenlik gereksinimleri
   - Teknoloji seçimi (C++17, Boost.Asio, Electron)

2. **Tasarım Fazı (3 Hafta)**
   - Modüler mimarinin oluşturulması
   - Plugin sistemi tasarımı
   - Ağ protokollerinin spesifikasyonu

3. **Implementasyon Fazı (6 Hafta)**
   - Backend daemon geliştirilmesi
   - GUI arayüzünün oluşturulması
   - Güvenlik katmanının entegrasyonu

4. **Test ve Optimizasyon (3 Hafta)**
   - Performans testleri
   - Güvenlik denetimleri
   - Optimizasyon ve hata düzeltmeleri

---

## �🔧 Dağıtım

### Sistem Gereksinimleri
- **OS**: Linux (Ubuntu 20.04+)
- **CPU**: 2 core
- **RAM**: 2 GB
- **Network**: 100 Mbps

### Kurulum Yöntemleri
1. **AppImage**: Taşınabilir paket
2. **DEB/RPM**: Paket yöneticisi
3. **Docker**: Konteyner dağıtım

---

## 📝 Kod Analizi

### Backend İstatistikleri
- **Toplam Satır**: ~16,600
- **Header Dosyaları**: 45
- **Source Dosyaları**: 67
- **Test Dosyaları**: 23

### Frontend İstatistikleri
- **Toplam Satır**: ~3,000
- **Components**: 28
- **TypeScript Files**: 35
- **Test Suites**: 12

### Ağ Kodu Dağılımı
| Modül | Satır Sayısı | Görev |
|-------|--------------|-------|
| NetworkPlugin | 2,100 | P2P iletişim |
| DeltaSync | 1,800 | Dosya senkronizasyonu |
| SecurityPlugin | 1,200 | Şifreleme |
| RelayServer | 800 | Relay hizmeti |

---

## 🎯 Öğrenim Çıktıları

Bu proje ile kazanılan beceriler:

1. **P2P Ağ Programlama**
   - UDP broadcast keşif
   - TCP peer-to-peer iletişim
   - NAT traversal teknikleri

2. **Modern C++**
   - Boost.Asio asenkron I/O
   - Thread pool yönetimi
   - RAII ve smart pointers

3. **Güvenli Programlama**
   - Kriptografi entegrasyonu
   - Güvenli kodlama pratikleri
   - Security by design

4. **Sistem Tasarımı**
   - Modüler mimari
   - Plugin sistemi
   - Cross-platform geliştirme

---

## 🔮 Gelecek Geliştirmeler

1. **IPv6 Desteği**
2. **QUIC Protokolü**
3. **Mesh Network Optimizasyonu**
4. **Machine Learning Tabanlı Yönlendirme**

---

## Ekler

### A. Network Packet Capture
[Wireshark capture dosyası eklendi]

### B. Performance Benchmark Script
[benchmark.sh dosyası eklendi]

### C. Security Audit Report
[security_audit.pdf dosyası eklendi]

---

**Proje Ekibi:** SentinelFS Development Team  
**İletişim:** github.com/reicalasso/SentinelFS
