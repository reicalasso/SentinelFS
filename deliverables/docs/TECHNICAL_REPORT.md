# SentinelFS Teknik Rapor

**Proje Adı:** SentinelFS - P2P Dosya Senkronizasyon Sistemi  
**Versiyon:** 1.0.0  
**Tarih:** Aralık 2025  
**Hazırlayan:** SentinelFS Development Team

---

## 1. Yönetici Özeti

SentinelFS, merkezi sunucu gerektirmeyen, peer-to-peer (P2P) mimarisine dayalı bir dosya senkronizasyon sistemidir. Sistem, yerel ağda bulunan cihazlar arasında otomatik dosya senkronizasyonu sağlar ve delta-sync algoritması ile bant genişliği kullanımını optimize eder.

### 1.1 Temel Özellikler

- **P2P Mimari:** Merkezi sunucu bağımlılığı olmadan çalışır
- **Delta Senkronizasyon:** Sadece değişen bloklar aktarılır
- **Otomatik Peer Keşfi:** UDP broadcast ile ağdaki peer'ları otomatik bulur
- **End-to-End Şifreleme:** AES-256-CBC + HMAC ile güvenli iletişim
- **Oturum Kodları:** Özel mesh ağları için erişim kontrolü
- **Gerçek Zamanlı İzleme:** inotify tabanlı dosya sistemi izleme
- **Modern GUI:** Electron + React tabanlı kontrol paneli

---

## 2. Sistem Mimarisi

### 2.1 Genel Bakış

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           SentinelFS Mimarisi                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────┐    IPC/JSON     ┌─────────────────────────────────┐    │
│  │  Electron   │◄───────────────►│         Sentinel Daemon         │    │
│  │    GUI      │                 │                                 │    │
│  └─────────────┘                 │  ┌─────────┐  ┌─────────────┐   │    │
│                                  │  │ Event   │  │   Plugin    │   │    │
│                                  │  │  Bus    │  │  Manager    │   │    │
│                                  │  └────┬────┘  └──────┬──────┘   │    │
│                                  │       │              │          │    │
│                                  │  ┌────┴──────────────┴────┐     │    │
│                                  │  │       Plugins          │     │    │
│                                  │  ├────────┬────────┬──────┤     │    │
│                                  │  │Network │Storage │FS    │     │    │
│                                  │  │Plugin  │Plugin  │Plugin│     │    │
│                                  │  └────────┴────────┴──────┘     │    │
│                                  └─────────────────────────────────┘    │
│                                           │                             │
│                                           ▼                             │
│                              ┌─────────────────────────┐                │
│                              │    Diğer Peer'lar       │                │
│                              │  (TCP/UDP üzerinden)    │                │
│                              └─────────────────────────┘                │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Bileşen Detayları

#### 2.2.1 Daemon (sentinel_daemon)

Ana işlem olarak çalışır ve tüm senkronizasyon mantığını yönetir.

| Bileşen | Sorumluluk |
|---------|------------|
| DaemonCore | Yaşam döngüsü yönetimi, sinyal işleme |
| IPCHandler | GUI ile JSON-RPC iletişimi |
| MetricsServer | Prometheus-uyumlu HTTP endpoint |
| PluginManager | Plugin yükleme ve yaşam döngüsü |
| EventBus | Pub/sub olay dağıtımı |

#### 2.2.2 Plugins

| Plugin | Dosya | Sorumluluk |
|--------|-------|------------|
| NetworkPlugin | `plugins/network/` | TCP bağlantıları, UDP discovery, handshake |
| StoragePlugin | `plugins/storage/` | SQLite veritabanı, peer/dosya kayıtları |
| FilesystemPlugin | `plugins/filesystem/` | inotify izleme, dosya hash hesaplama |
| MLPlugin | `plugins/ml/` | ONNX anomali tespiti (Future) |

#### 2.2.3 Core Modülleri

| Modül | Konum | Sorumluluk |
|-------|-------|------------|
| DeltaEngine | `core/sync/` | Rolling checksum, delta hesaplama |
| DeltaSyncProtocolHandler | `core/sync/` | Protokol mesajları işleme |
| FileSyncHandler | `core/sync/` | Dosya değişikliklerini broadcast |
| EventHandlers | `core/sync/` | EventBus abonelikleri |
| Crypto | `core/security/` | AES şifreleme, HMAC, key derivation |
| HandshakeProtocol | `core/security/` | Peer kimlik doğrulama |

---

## 3. Protokol Tasarımı

### 3.1 Peer Keşfi (UDP Discovery)

```
Mesaj Formatı: SENTINEL_DISCOVERY|PEER_ID|TCP_PORT

Örnek: SENTINEL_DISCOVERY|PEER_82844|8082

Akış:
1. Her peer periyodik olarak (10-60 saniye) UDP broadcast gönderir
2. Diğer peer'lar mesajı alır ve PEER_DISCOVERED olayı yayınlar
3. EventHandlers peer'ı veritabanına ekler ve TCP bağlantısı başlatır
```

### 3.2 Handshake Protokolü

```
┌──────────┐                              ┌──────────┐
│  Client  │                              │  Server  │
└────┬─────┘                              └────┬─────┘
     │                                         │
     │  SENTINEL_HELLO|client_id|version       │
     │────────────────────────────────────────►│
     │                                         │
     │  SENTINEL_CHALLENGE|server_id|nonce     │
     │◄────────────────────────────────────────│
     │                                         │
     │  SENTINEL_AUTH|digest                   │
     │────────────────────────────────────────►│
     │                                         │
     │  SENTINEL_WELCOME|encryption_key        │
     │◄────────────────────────────────────────│
     │                                         │

Digest = HMAC-SHA256(
    "CLIENT_AUTH|client_id|server_id|session_code",
    DeriveKey(session_code)
)
```

### 3.3 Dosya Senkronizasyon Protokolü

```
┌──────────┐                              ┌──────────┐
│  Sender  │                              │ Receiver │
└────┬─────┘                              └────┬─────┘
     │                                         │
     │  UPDATE_AVAILABLE|path|hash|size        │
     │────────────────────────────────────────►│
     │                                         │
     │  REQUEST_DELTA|path|signatures          │
     │◄────────────────────────────────────────│
     │                                         │
     │  DELTA_DATA|path|chunk_id/total|data    │
     │────────────────────────────────────────►│
     │  ...                                    │
     │                                         │

Veya yeni dosya için:
     │  FILE_DATA|path|chunk_id/total|data     │
     │────────────────────────────────────────►│
```

### 3.4 Mesaj Türleri

| Mesaj | Yön | Açıklama |
|-------|-----|----------|
| UPDATE_AVAILABLE | Sender → Receiver | Dosya güncellemesi bildirimi |
| REQUEST_DELTA | Receiver → Sender | Delta/tam dosya talebi |
| DELTA_DATA | Sender → Receiver | Delta verisi (chunk'lar halinde) |
| FILE_DATA | Sender → Receiver | Tam dosya verisi (yeni dosyalar) |
| DELETE_FILE | Sender → Receiver | Dosya silme bildirimi |

---

## 4. Delta Senkronizasyon Algoritması

### 4.1 Rolling Checksum (Adler-32)

```cpp
// Block signature hesaplama
struct BlockSignature {
    uint32_t weak_checksum;   // Adler-32
    uint64_t strong_checksum; // SHA-256 (truncated)
    size_t offset;
    size_t length;
};

// Adler-32 rolling checksum
uint32_t adler32_rolling(uint32_t prev, uint8_t old_byte, uint8_t new_byte, size_t block_size) {
    uint32_t a = prev & 0xFFFF;
    uint32_t b = (prev >> 16) & 0xFFFF;
    
    a = (a - old_byte + new_byte) % MOD_ADLER;
    b = (b - block_size * old_byte + a - 1) % MOD_ADLER;
    
    return (b << 16) | a;
}
```

### 4.2 Delta Hesaplama Akışı

```
1. Alıcı: Mevcut dosya için block signature'ları hesapla
2. Alıcı: Signature'ları REQUEST_DELTA ile gönder
3. Gönderici: Yeni dosyayı tara
   - Her pozisyonda rolling checksum hesapla
   - Eşleşme bulunursa: COPY instruction oluştur
   - Eşleşme yoksa: DATA instruction oluştur
4. Gönderici: Delta'yı DELTA_DATA olarak gönder
5. Alıcı: Delta'yı uygulayarak dosyayı güncelle
```

### 4.3 Delta Instruction Türleri

```cpp
enum class DeltaType : uint8_t {
    COPY = 0,  // Mevcut dosyadan kopyala
    DATA = 1   // Yeni veri ekle
};

struct DeltaInstruction {
    DeltaType type;
    size_t offset;    // COPY için: kaynak offset
    size_t length;    // Byte sayısı
    vector<uint8_t> data;  // DATA için: yeni veriler
};
```

---

## 5. Güvenlik

### 5.1 Şifreleme

| Katman | Algoritma | Amaç |
|--------|-----------|------|
| Transport | AES-256-CBC | Veri gizliliği |
| Integrity | HMAC-SHA256 | Veri bütünlüğü |
| Key Derivation | PBKDF2 | Session code'dan key türetme |

### 5.2 Oturum Kodu Akışı

```
Session Code: 6 karakter (A-Z, 0-9)
Entropy: 36^6 = ~2.2 milyar kombinasyon

Key Derivation:
  encryption_key = PBKDF2(session_code, "SentinelFS_Salt", 100000, SHA256, 32)
```

### 5.3 Mesaj Şifreleme

```cpp
// Şifreleme
encrypted = AES_256_CBC_Encrypt(plaintext, key, iv)
hmac = HMAC_SHA256(encrypted, key)
message = iv || encrypted || hmac

// Çözme
1. HMAC doğrula
2. IV'yi ayır
3. AES ile çöz
```

---

## 6. Veritabanı Şeması

### 6.1 Tablolar

```sql
-- Peer bilgileri
CREATE TABLE peers (
    id TEXT PRIMARY KEY,
    ip TEXT NOT NULL,
    port INTEGER NOT NULL,
    last_seen INTEGER,
    status TEXT DEFAULT 'unknown',
    latency INTEGER DEFAULT -1
);

-- Dosya kayıtları
CREATE TABLE files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT UNIQUE NOT NULL,
    hash TEXT NOT NULL,
    size INTEGER,
    modified_time INTEGER,
    synced INTEGER DEFAULT 0
);

-- Dosya versiyonları (conflict resolution için)
CREATE TABLE file_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER REFERENCES files(id),
    version INTEGER,
    hash TEXT,
    timestamp INTEGER
);

-- İzlenen klasörler
CREATE TABLE watched_folders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT UNIQUE NOT NULL,
    status TEXT DEFAULT 'active',
    file_count INTEGER DEFAULT 0
);

-- Ignore pattern'ları
CREATE TABLE ignore_patterns (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    pattern TEXT NOT NULL,
    active INTEGER DEFAULT 1
);

-- Senkronizasyon kuyruğu
CREATE TABLE sync_queue (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_path TEXT NOT NULL,
    operation TEXT NOT NULL,
    priority INTEGER DEFAULT 0,
    created_at INTEGER
);
```

---

## 7. GUI Mimarisi

### 7.1 Teknoloji Stack

| Katman | Teknoloji |
|--------|-----------|
| Framework | Electron 28+ |
| UI Library | React 18 |
| State Management | Context API |
| Styling | Tailwind CSS |
| Build Tool | Vite |
| IPC | Electron IPC + JSON-RPC |

### 7.2 Sayfa Yapısı

| Sayfa | Dosya | Fonksiyon |
|-------|-------|-----------|
| Dashboard | `Dashboard.tsx` | Genel durum, metrikler |
| Files | `Files.tsx` | Dosya tarayıcı, izlenen klasörler |
| Transfers | `Transfers.tsx` | Upload/download kuyruğu |
| Settings | `Settings.tsx` | Yapılandırma paneli |

### 7.3 IPC Komutları

```typescript
// GUI → Daemon
interface IPCCommands {
  'get-status': () => DaemonStatus;
  'get-peers': () => PeerInfo[];
  'get-files': () => FileInfo[];
  'add-watch': (path: string) => boolean;
  'remove-watch': (path: string) => boolean;
  'pause-sync': () => void;
  'resume-sync': () => void;
  'set-session-code': (code: string) => boolean;
  'set-bandwidth': (up: number, down: number) => void;
}
```

---

## 8. Performans Optimizasyonları

### 8.1 Network

- **Chunk-based transfer:** 64KB chunk'lar ile büyük dosya transferi
- **Connection pooling:** Peer başına tek TCP bağlantısı
- **Rate limiting:** Yapılandırılabilir bant genişliği limitleri

### 8.2 Storage

- **SQLite WAL mode:** Concurrent read/write desteği
- **Indexed queries:** hash, path, status üzerinde indeksler
- **Batch operations:** Toplu insert/update işlemleri

### 8.3 Memory

- **Streaming delta:** Büyük dosyalar memory-mapped
- **Chunk assembly:** Sadece tamamlanan chunk'lar RAM'de

---

## 9. Hata Yönetimi

### 9.1 Bağlantı Hataları

| Hata | Davranış |
|------|----------|
| TCP bağlantı kopması | Otomatik yeniden bağlanma (5s backoff) |
| Handshake başarısız | Peer "inactive" olarak işaretlenir |
| Timeout | İşlem iptal, retry kuyruğa eklenir |

### 9.2 Sync Loop Önleme

```cpp
// Son patch'lenen dosyaları 2 saniye ignore et
unordered_map<string, time_point> ignoreList_;

void markAsPatched(const string& filename) {
    ignoreList_[filename] = steady_clock::now();
}

bool shouldIgnore(const string& filename) {
    auto it = ignoreList_.find(filename);
    if (it != ignoreList_.end()) {
        auto elapsed = steady_clock::now() - it->second;
        if (elapsed < 2s) return true;
        ignoreList_.erase(it);
    }
    return false;
}
```

---

## 10. Bilinen Limitasyonlar

1. **Tek ağ segmenti:** UDP broadcast LAN içinde çalışır, WAN için relay gerekir
2. **Dosya boyutu:** 2GB üzeri dosyalar için memory pressure oluşabilir
3. **Symbolic link:** Symlink'ler takip edilmez, kopyalanmaz
4. **Conflict resolution:** Aynı anda düzenleme durumunda "last write wins"
5. **Platform:** Şu an sadece Linux (inotify). macOS/Windows portu planlanıyor

---

## 11. Gelecek Geliştirmeler

- [ ] WAN relay sunucusu (NAT traversal)
- [ ] macOS FSEvents desteği
- [ ] Windows ReadDirectoryChangesW desteği
- [ ] Conflict resolution UI
- [ ] Selective sync (klasör/dosya bazlı)
- [ ] Dosya versiyonlama ve rollback
- [ ] Mobile companion app

---

## 12. Sonuç

SentinelFS, modern P2P teknolojileri kullanarak güvenli ve verimli dosya senkronizasyonu sağlar. Delta-sync algoritması bant genişliği kullanımını minimize ederken, end-to-end şifreleme veri güvenliğini garanti eder. Modüler plugin mimarisi gelecekteki genişlemelere olanak tanır.

---

