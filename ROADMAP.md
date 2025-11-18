
# 🗺️ **SentinelFS-Neo Roadmap (2024–2025)**

Framework-level P2P Synchronization Engine with Plugin Architecture

---

# 🎯 **A. Proje Aşamaları (High-Level Stages)**

```objectivec
Stage 1 — Core Foundations
Stage 2 — Filesystem & Delta Engine
Stage 3 — Network Layer & Auto-Remesh
Stage 4 — Storage Layer + Metadata
Stage 5 — ML Layer + Anomaly Detection
Stage 6 — UX, CLI, Daemonization
Stage 7 — Testing, Benchmarking, Hardening
Stage 8 — Extensions & Ecosystem
```

---

# 🧩 **B. Sprint Bazlı Yol Haritası (Detaylı)**

Aşağıdaki liste *doğrudan GitHub Issue Epikleri* şeklinde de kullanılabilir.

---

## 🟦 **Sprint 1 — Core Infrastructure (7–10 gün)**

**Hedef:** Plugin mimarisinin omurgasını kurmak

### ✅ Görevler

-    Plugin ABI (C API) tamamlanması
    
-    PluginLoader implementasyonu
    
-    EventBus + event dispatch sistemi
    
-    Config loader (JSON)
    
-    Logger sistemi
    
-    Core CMake yapılandırması
    
-    “Hello Plugin” örneği
    

### 🎯 Sonuç

Projede her şeyin üzerine inşa edildiği **Core** hazır.

---

## 🟩 **Sprint 2 — FileAPI + Snapshot Engine (7–9 gün)**

**Hedef:** Dosya sisteminin soyutlanması

### Görevler

-    IFileAPI tam implementasyon (std::filesystem + OpenSSL SHA-256)
    
-    Chunking sistemi (4 KB sliding window)
    
-    SnapshotEngine → recursive directory scanning
    
-    Snapshot compare (added/removed/modified)
    
-    File events → sync tasks mapping
    

### Sonuç

Sistem dosya değişikliklerini tespit edip işlem kuyruğuna atabiliyor.

---

## 🟨 **Sprint 3 — Filesystem Plugins (10–14 gün)**

**Hedef:** platform bazlı watcher eklentileri

### Görevler

-    watcher.linux (inotify)
    
-    watcher.macos (FSEvents)
    
-    watcher.windows (ReadDirectoryChangesW)
    
-    IWatcher interface testleri
    
-    FsEvent → EventBus entegrasyonu
    

### Sonuç

FS watchers → pipeline’ın başlangıcı çalışıyor.

---

## 🟧 **Sprint 4 — Delta Engine (Rsync-style) (14–20 gün)**

**Hedef:** Sadece değişen blokların senkronizasyonu

### Görevler

-    Weak checksum (rolling checksum)
    
-    Strong hash (SHA-256)
    
-    Block match algoritması
    
-    DeltaResult üretimi
    
-    Delta apply engine
    
-    Unit test + benchmark
    

### Sonuç

SentinelFS-Neo artık gerçek **delta tabanlı** senkron çalışıyor.

---

## 🟥 **Sprint 5 — Network Layer (15–20 gün)**

**Hedef:** Peer discovery + P2P temel altyapı

### Görevler

-    discovery.udp plugin’i
    
-    discovery.holepunch (WAN için)
    
-    transfer.tcp (temel transport)
    
-    Peer registry + alive-check
    
-    PeerInfo events
    
-    NAT traversal testleri
    

### Sonuç

Cihazlar birbirini görüyor ve connect oluyor.

---

## 🟪 **Sprint 6 — Auto-Remesh Engine (10–14 gün)**

**Hedef:** Adaptif P2P topolojisi

### Görevler

-    RTT ölçümü
    
-    Jitter & loss tracking
    
-    Peer scoring algoritması
    
-    Topoloji değişimi (adaptive mesh)
    
-    Dynamic route update
    
-    Stress test / failover test
    

### Sonuç

SentinelFS-Neo yavaş bağlantıları atıyor, hızlı olanları seçiyor.

---

## 🟫 **Sprint 7 — Storage Layer (7–10 gün)**

**Hedef:** Metadata veritabanı

### Görevler

-    SQLite tabanlı metadata store (files/devices/versions)
    
-    Hash store (blake3 opsiyon)
    
-    Write-ahead queue
    
-    Metadata caching
    
-    DB transaction yönetimi
    

### Sonuç

Tüm dosya metadata’sı persist ediliyor.

---

## 🟩 **Sprint 8 — ML Layer (14–21 gün)**

**Hedef:** Davranış analizi + anomali tespiti

### Görevler

-    ONNX Runtime entegre et
    
-    AccessLog → dataset
    
-    Isolation Forest modeli
    
-    model.onnx oluştur
    
-    IAnomalyDetector plugin
    
-    AnomalyScoreEvent → UI/CLI hook
    

### Sonuç

Sistem anormal dosya erişimlerini tespit ediyor.

---

## 🟦 **Sprint 9 — CLI + Service + UX Layer (7–12 gün)**

**Hedef:** Kullanıcı arayüzü

### Görevler

-    CLI komutları (start/status/config/plugins)
    
-    Daemon service (Linux/macOS/Win)
    
-    Verbose logging + sync progress output
    
-    Plugin yönetim arayüzü
    

### Sonuç

Kullanıcı yapılandırabilir, durum alabilir, plugin ekleyebilir.

---

## 🟩 **Sprint 10 — Test, Benchmark & Hardening (20+ gün)**

**Hedef:** Özgüven seviyesi üretime yakın sistem

### Görevler

-    Unit test (Catch2)
    
-    Integration test
    
-    Plugin isolation test
    
-    Network stress test
    
-    File race condition test
    
-    Memory usage profiling
    
-    CPU load benchmarking
    

### Sonuç

Stabil, kararlı ve ölçümlenmiş platform.

---

## 🟨 **Sprint 11 — Extensibility & Plugins (Devam eden)**

Gelecekte eklenebilecek özellikler:

### Network

-   QUIC transfer plugin
    
-   Bluetooth/WiFi-Direct plugin
    
-   WebRTC data channel plugin
    

### Filesystem

-   bsdiff delta plugin
    
-   “Real-time compression delta” plugin
    

### Storage

-   LMDB backend
    
-   RocksDB backend
    

### ML

-   Transformer-based filesystem anomaly model
    
-   Sequence prediction for pre-sync optimization
    

---

# 🚀 **C. Release Plan**

| Sürüm | İçerik | Zaman |
| --- | --- | --- |
| **v0.1.0 (Core MVP)** | PluginLoader + EventBus | 2–3 hafta |
| **v0.2.0 (FS MVP)** | Watcher + Snapshot | 1–2 hafta |
| **v0.3.0 (Delta MVP)** | Delta motoru | 2–3 hafta |
| **v0.4.0 (Network MVP)** | Discovery + TCP | 3–4 hafta |
| **v0.5.0 (Mesh Beta)** | Auto-Remesh | 2–3 hafta |
| **v0.6.0 (Metadata)** | SQLite store | 1–2 hafta |
| **v0.7.0 (ML Beta)** | ISOForest + ONNX | 2–3 hafta |
| **v1.0 (Stable)** | Full test + daemon | 4–6 hafta |

---

# 🎖️ **D. Long-Term Vision**

SentinelFS-Neo, klasik bir "sync tool" değil — bir **distributed file fabric framework**.

Uzun vadeli hedefler:

-   🌐 Multi-cloud peer mesh
    
-   🔒 Zero-trust signature model
    
-   ✨ Predictive sync
    
-   🧱 Changelog çıkarabilen VFS katmanı
    
-   📦 Plugin marketplace
    
-   🛰️ IoT cluster’larına özel lightweight mod
    

---

# 🧿 **E. Roadmap Özet**

```objectivec
Core → FS → Delta → Network → Remesh → Storage → ML → CLI → Hardening
```

Her aşama bağımsız modül üzerine kurulu olduğu için geliştirilebilirlik çok yüksek.

---