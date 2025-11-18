# 🔥 **SENTINELFS-NEO — REVİZE README (PROFESYONEL & ÜRÜN ODAKLI)**

Aşağıdaki metni **direkt README.md olarak koyabilirsin.**

---

# 🚀 SentinelFS-Neo

**Self-Optimizing P2P File Fabric with Auto-Remesh & Delta Sync**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)  
[![C++](https://img.shields.io/badge/C++-17-00599C.svg)](https://isocpp.org)  
[](#)

[](#)

SentinelFS-Neo; cihazlar arasında **gerçek zamanlı, tamamen dağıtık, P2P tabanlı dosya senkronizasyonu** sağlayan, **adaptif mesh ağ mimarisine** sahip, hafif ve yüksek performanslı bir dosya sistemi çekirdeğidir.

Sistem, ağ koşullarına göre kendini **yeniden şekillendiren auto-remesh motoru**, rsync-benzeri **delta transfer algoritması** ve entegre **ML tabanlı davranış analizi** ile, klasik sync çözümlerinin çok ötesine geçer.

---

## 📌 İçindekiler

-   Özet
    
-   Temel Özellikler
    
-   [Mimari](#-mimari)
    
-   Veritabanı & ML
    
-   Kurulum
    
-   Kullanım
    
-   Proje Rolleri (Akademik)
    
-   Geliştirme & Katkı
    
-   [Lisans](#-lisans)
    

---

## 🚀 Özet

SentinelFS-Neo, aynı session code'a sahip cihazlar arasında mikro-mesh ağı kurar:

-   En düşük gecikmeli peers → otomatik seçilir
    
-   Ağ bozulduğunda → auto-remesh devreye girer
    
-   Her dosya değişikliği → delta algoritmasıyla optimize edilir
    
-   Veritabanı → metadata bütünlüğü sağlar
    
-   ML katmanı → anormal dosya erişimlerini tespit eder
    

Tamamen modüler yapısı sayesinde hem akademik projeler hem de gerçek dünya uygulamaları için uygundur.

---

## ✨ Temel Özellikler

### 🔧 P2P + Auto-Remesh Motoru

-   UDP/TCP hibrit peer discovery
    
-   RTT, jitter, packet-loss ölçümü
    
-   **Dinamik mesh topolojisi yeniden inşası**
    
-   NAT traversal desteği
    
-   Düşük gecikme odaklı bağlantı seçimi
    

### ⚡ Delta-Based Sync (Rsync-Compatible)

-   Rolling checksum (Adler32)
    
-   Güçlü hash (SHA-256)
    
-   Değişen blokların tespiti
    
-   Paralel chunk transferi
    
-   Bant genişliği optimizasyonu
    

### 📁 OS-Seviye Dosya İzleme

-   Linux → inotify
    
-   macOS → FSEvents
    
-   Windows → ReadDirectoryChangesW
    
-   Gerçek zamanlı event queue
    

### 🔒 Güvenlik

-   Session-based shared key
    
-   AES-256 dosya aktarım kanalı
    
-   Doğrulanmış metadata
    

### 🧠 ML Katmanı (Opsiyonel)

-   ONNX Runtime ile embedded model
    
-   Isolation Forest tabanlı erişim anomalisi
    
-   “Suspicious file activity” skoru üretimi
    

---

## 🧬 Mimari

```mathematica
┌───────────────────────────────┐
│ Application Layer             │
│ CLI • Config • Logger         │
├───────────────────────────────┤
│ File System Layer             │
│ Watcher • Delta Engine • Queue│
├───────────────────────────────┤
│ Network Layer                 │
│ Discovery • Auto-Remesh • I/O │
├───────────────────────────────┤
│ Storage Layer                 │
│ SQLite • Hash Store • Cache   │
├───────────────────────────────┤
│ ML Layer                      │
│ Anomaly Detection (ONNX)      │
└───────────────────────────────┘
```

Her katman tamamen modülerdir ve bağımsız olarak derlenip test edilebilir.

---

## 🗄️ Veritabanı & ML

### 📌 Temel Varlıklar

-   `Device`
    
-   `Session`
    
-   `File`
    
-   `FileVersion`
    
-   `SyncQueue`
    
-   `FileAccessLog`
    

ER diyagramı: `docs/er_diagram.md`  
SQL şeması: `src/db/schema.sql`

### 🤖 ML Pipeline

1.  Access log to dataset
    
2.  IsolationForest → ONNX dönüştürme
    
3.  ONNX Runtime üzerinden C++ inference
    
4.  Anomali skoru üretimi
    

---

## 🔧 Kurulum

### Gereksinimler

```objectivec
GCC/Clang (C++17)
CMake 3.12+
SQLite3 dev
OpenSSL 1.1+
```

### Linux/macOS

```bash
git clone https://github.com/.../sentinelfs-neo.git
cd sentinelfs-neo
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### ML Destekli Kurulum

```bash
pip install scikit-learn skl2onnx onnx
# ONNX runtime indir ve çıkar
```

---

## 🎮 Kullanım

### Basit Başlangıç

```bash
./sentinelfs-neo --session ABC-123 --path ~/SyncFolder
```

### Aynı Session ile Diğer Cihaz

```bash
./sentinelfs-neo --session ABC-123 --path D:\Sync
```

### CLI Parametreleri

```css
--session <CODE>
--path <DIR>
--port <PORT>
--verbose
--daemon
--config <FILE>
```

### Örnek Config

```json
{
  "session": { "code": "SENT-2024", "encryption": true },
  "network": { "port": 8080, "remesh_threshold": 100 },
  "sync": { "delta_algorithm": "rsync" },
  "storage": { "metadata_db": "~/.sentinel/meta.db" }
}
```

---

## 🎓 Proje Rolleri (Akademik)

### 1️⃣ Network Lead

Peer discovery, NAT traversal, auto-remesh.

### 2️⃣ File System Lead

Delta-engine, OS watcher API’leri.

### 3️⃣ Data & ML Lead

SQLite şeması, ONNX entegrasyonu.

### 4️⃣ Application & Build Lead

CMake, CLI, CI/CD pipeline.

---

## 🤝 Geliştirme & Katkı

```sql
git checkout -b feature/new-feature
git commit -m "Add feature"
git push origin feature/new-feature
```

Detaylar için: `CONTRIBUTING.md`

---

## 📝 Lisans

**MIT** — özgürce kullanın, geliştirin, dağıtın.

---

<div align="center"> <br><strong>SentinelFS-Neo</strong><br> Distributed systems meet real-time intelligence.<br><br> ⭐ Eğer beğendiysen yıldız vermeyi unutma ⭐ </div>

---