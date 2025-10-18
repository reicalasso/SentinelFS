## ⚙️ **3\. Uygulama Detayları**

### 3.1 Peer Discovery (Cihaz Keşfi)

Her cihaz SentinelFS-Lite’ı başlattığında ağda diğer cihazları bulmak için basit bir **UDP broadcast** sinyali gönderir.  
Bu sinyal, cihazın kimliğini (`device_id`) ve oturum kimliğini (`session_code`) içerir.  
Aynı session code’a sahip cihazlar, birbirlerini “meşru peer” olarak tanır.

#### **Peer Discovery İş Akışı**

1.  Cihaz başlatılır, `session_code` girilir.
    
2.  UDP broadcast ile ağda bir “hello” paketi gönderilir:
    
    ```pgsql
    SENTHELLO|session=SENT-1234-XYZ|ip=192.168.1.10|port=4000
    ```
    
3.  Bu paketi alan diğer cihaz, eğer kendi `session_code` değeriyle eşleşiyorsa, TCP bağlantısı kurmak için yanıt verir.
    
4.  Cihazlar birbirini **peer listesine** ekler.
    

#### **Peer Discovery Pseudocode**

```cpp
// discovery.cpp
void PeerDiscovery::broadcastHello() {
    std::string packet = "SENTHELLO|" + sessionCode + "|" + localIP;
    sendUDP(packet, broadcastAddress, 4000);
}

void PeerDiscovery::listenForPeers() {
    while (true) {
        std::string msg = recvUDP(4000);
        if (msg.contains(sessionCode)) {
            std::string peerIP = parseIP(msg);
            peers.insert(peerIP);
            establishTCP(peerIP);
        }
    }
}
```

Bu yöntem, **NAT arkasındaki cihazlar için basit yerel keşif** sağlar.  
Gelecekte, STUN/ICE destekli geniş ağ keşfi eklenebilir.

---

### 3.2 Auto-Remesh (Ağ Yeniden Yapılandırması)

Mesh ağı statik değildir; cihazlar taşınabilir, ağ kalitesi değişebilir.  
Bu yüzden sistem her 30 saniyede bir **ping testi** yapar, en düşük gecikmeli bağlantıları “aktif”, yüksek gecikmeli olanları “pasif” hale getirir.

#### **Auto-Remesh İş Akışı**

1.  Peer listesi alınır.
    
2.  Her peer’e küçük bir “ping” paketi gönderilir.
    
3.  Gecikme süreleri (ms) ölçülür.
    
4.  En düşük `N` gecikmeli peer aktif bağlantı olarak belirlenir.
    
5.  Gerekirse bazı bağlantılar kapatılır veya yeniden açılır.
    

#### **Auto-Remesh Pseudocode**

```cpp
// remesh.cpp
void AutoRemesh::evaluateConnections() {
    std::vector<PeerLatency> latencies;
    for (auto &peer : peers) {
        auto ms = ping(peer);
        latencies.push_back({peer, ms});
    }
    std::sort(latencies.begin(), latencies.end(),
              [](auto &a, auto &b){ return a.latency < b.latency; });
    activePeers = selectTop(latencies, MAX_ACTIVE_CONNECTIONS);
}
```

Bu mekanizma sayesinde sistem, **ağın fiziksel durumuna göre kendini optimize eder.**  
Bu da projeye işletim sistemi seviyesinde **adaptif kaynak yönetimi** örneği kazandırır.

---

### 3.3 Delta Sync (Dosya Farkı Hesaplama ve Aktarım)

Tam dosyayı göndermek yerine, sadece değişen bölümler (blok farkları) hesaplanır.  
Bu hem ağ yükünü azaltır hem de disk I/O verimliliğini artırır.

#### **Delta Sync Süreci**

1.  File Watcher bir değişiklik tespit eder (`onChange(path)`).
    
2.  `delta_engine` eski ve yeni hash değerlerini karşılaştırır.
    
3.  Sadece değişen bloklar (`delta_chunks`) oluşturulur.
    
4.  Bu delta’lar Network Layer’a gönderilir.
    
5.  Karşı taraf bu delta’yı kendi dosyasına uygular.
    

#### **Delta Pseudocode**

```cpp
// delta_engine.cpp
Delta DeltaEngine::createDelta(const File &oldFile, const File &newFile) {
    Delta delta;
    for (size_t i = 0; i < newFile.blocks(); ++i) {
        if (hash(newFile.block(i)) != hash(oldFile.block(i))) {
            delta.add(i, newFile.block(i));
        }
    }
    return delta;
}

void DeltaEngine::applyDelta(File &target, const Delta &delta) {
    for (auto &chunk : delta.chunks) {
        target.writeBlock(chunk.index, chunk.data);
    }
}
```

Bu yapı, hem “işletim sistemi” hem “veritabanı” dersinde önemli bir konu olan **veri bütünlüğü** kavramını destekler.

---

### 3.4 Metadata Database (SQLite veya KV Store)

Her cihaz, dosya sisteminin “durumunu” kendi metadata veritabanında tutar.  
Bu veritabanı, sistemin “neyin güncel, neyin eski” olduğunu anlamasını sağlar.

#### **Metadata Şeması (SQLite)**

| Tablo | Alan | Açıklama |
| --- | --- | --- |
| `files` | `path`, `hash`, `modified_at` | Dosya kimliği ve hash bilgisi |
| `peers` | `ip`, `latency`, `last_seen` | Peer bağlantı durumu |
| `settings` | `key`, `value` | Sistem ayarları |

#### **Örnek Kod**

```cpp
// db.cpp
void Database::updateFileHash(std::string path, std::string hash) {
    exec("REPLACE INTO files (path, hash, modified_at) VALUES (?, ?, NOW())",
         path, hash);
}
```

Bu veritabanı, File System Layer ve Network Layer arasında **durum tutarlılığı** sağlar.

---

### 3.5 Güvenlik Katmanı (Lite)

SentinelFS-Lite basit ama etkili bir güvenlik modeli kullanır:

-   Session code tabanlı kimlik doğrulama
    
-   CRC32 bütünlük kontrolü
    
-   Her bağlantıda nonce tabanlı “handshake” (replay saldırılarını önlemek için)
    

Bu kısım, ileride TLS veya ECDH anahtar değişimiyle genişletilebilir.

---

### 3.6 Özet Akış Diyagramı

```scss
┌────────────────────────────┐
│  Start SentinelFS-Lite     │
└────────────┬───────────────┘
             │
             ▼
┌────────────────────────────┐
│  Peer Discovery (UDP)      │
│  → Peer list oluşturulur   │
└────────────┬───────────────┘
             │
             ▼
┌────────────────────────────┐
│  Auto-Remesh               │
│  → En iyi bağlantılar seç  │
└────────────┬───────────────┘
             │
             ▼
┌────────────────────────────┐
│  File Watcher              │
│  → Değişiklik algılandı    │
└────────────┬───────────────┘
             │
             ▼
┌────────────────────────────┐
│  Delta Engine              │
│  → Fark hesapla, gönder    │
└────────────┬───────────────┘
             │
             ▼
┌────────────────────────────┐
│  Apply Delta on Receiver   │
│  → Metadata güncelle       │
└────────────────────────────┘
```

---
