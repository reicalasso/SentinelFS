# Threat Quarantine Backend Integration

## Overview
Bu dokümantasyon, Threat Quarantine özelliğinin backend tarafında nasıl entegre edileceğini açıklar.

## Gerekli Backend Komutları

### 1. DELETE_THREAT
Karantinaya alınmış bir tehditi kalıcı olarak siler.

**Format:** `DELETE_THREAT <threat_id>`

**Örnek:**
```
DELETE_THREAT 123
```

**Beklenen Davranış:**
- Tehdit dosyasını karantina dizininden siler
- Veritabanından tehdit kaydını kaldırır
- Başarı durumunda `{ success: true }` döner
- Hata durumunda `{ success: false, error: "error message" }` döner

---

### 2. MARK_THREAT_SAFE
Bir tehdidi güvenli olarak işaretler (ancak karantinadan çıkarmaz).

**Format:** `MARK_THREAT_SAFE <threat_id>`

**Örnek:**
```
MARK_THREAT_SAFE 123
```

**Beklenen Davranış:**
- Veritabanında tehdidin `markedSafe` alanını `true` yapar
- Tehdit hala karantinada kalır
- GUI'de tehdit "güvenli" olarak görünür
- Başarı durumunda `{ success: true }` döner

---

### 3. UNMARK_THREAT_SAFE
Bir tehdidin güvenli işaretini kaldırır.

**Format:** `UNMARK_THREAT_SAFE <threat_id>`

**Örnek:**
```
UNMARK_THREAT_SAFE 123
```

**Beklenen Davranış:**
- Veritabanında tehdidin `markedSafe` alanını `false` yapar
- Tehdit tekrar aktif tehdit olarak görünür
- Başarı durumunda `{ success: true }` döner

---

## Tehdit Verileri İletimi

### DETECTED_THREATS Event
Backend, tespit edilen tehditleri GUI'ye `DETECTED_THREATS` eventi ile göndermelidir.

**Event Format:**
```json
{
  "type": "DETECTED_THREATS",
  "payload": [
    {
      "id": 1,
      "filePath": "/path/to/suspicious/file.exe",
      "threatType": "RANSOMWARE",
      "threatLevel": "CRITICAL",
      "threatScore": 95.5,
      "detectedAt": 1701234567890,
      "entropy": 7.8,
      "fileSize": 1048576,
      "hash": "sha256hashhere...",
      "quarantinePath": "/path/to/quarantine/file.exe",
      "mlModelUsed": "random_forest_v2",
      "additionalInfo": "Detected rapid file encryption pattern",
      "markedSafe": false
    }
  ]
}
```

### Threat Type Options
- `RANSOMWARE`: Fidye yazılımı
- `HIGH_ENTROPY`: Yüksek entropi (şifreli dosya olabilir)
- `MASS_OPERATION`: Toplu dosya işlemi
- `SUSPICIOUS_PATTERN`: Şüpheli desen
- `UNKNOWN`: Bilinmeyen tehdit

### Threat Level Options
- `LOW`: Düşük tehdit
- `MEDIUM`: Orta tehdit
- `HIGH`: Yüksek tehdit
- `CRITICAL`: Kritik tehdit

---

## Database Schema Önerisi

```sql
CREATE TABLE detected_threats (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  file_path TEXT NOT NULL,
  threat_type TEXT NOT NULL,
  threat_level TEXT NOT NULL,
  threat_score REAL NOT NULL,
  detected_at INTEGER NOT NULL,
  entropy REAL,
  file_size INTEGER NOT NULL,
  hash TEXT,
  quarantine_path TEXT,
  ml_model_used TEXT,
  additional_info TEXT,
  marked_safe INTEGER DEFAULT 0
);
```

---

## Implementasyon Kontrol Listesi

### Backend (C++)
- [ ] `DELETE_THREAT` komutu implementasyonu
- [ ] `MARK_THREAT_SAFE` komutu implementasyonu
- [ ] `UNMARK_THREAT_SAFE` komutu implementasyonu
- [ ] Tehdit tespiti sırasında karantina dizini oluşturma
- [ ] Karantinaya alınan dosyaları kaydetme
- [ ] `DETECTED_THREATS` event'ini GUI'ye gönderme
- [ ] Database tablosu ve CRUD operasyonları

### ML Plugin
- [ ] Tehdit tespiti sonuçlarını veritabanına kaydetme
- [ ] Tespit edilen dosyaları karantina dizinine taşıma
- [ ] Tehdit bilgilerini daemon'a iletme

---

## Test Senaryoları

### 1. Tehdit Tespiti ve Listeleme
```bash
# ML plugin bir tehdit tespit eder
# GUI otomatik olarak Quarantine Center'a badge gösterir
# Kullanıcı "Threat Quarantine" sekmesine tıklar
# Tespit edilen tüm tehditler listelenir
```

### 2. Tehdit Silme
```bash
# Kullanıcı bir tehditi seçer
# "Sil" butonuna tıklar
# Onay diyalogu görünür
# Kullanıcı onaylar
# Backend DELETE_THREAT komutunu alır
# Dosya karantinadan silinir
# GUI listesinden tehdit kaldırılır
```

### 3. Güvenli İşaretleme
```bash
# Kullanıcı bir tehditi seçer
# "Güvenli İşaretle" butonuna tıklar
# Backend MARK_THREAT_SAFE komutunu alır
# Tehdit güvenli olarak işaretlenir
# GUI'de yeşil "Güvenli" badge'i görünür
```

---

## İletişim Akışı

```
ML Plugin → Daemon → GUI
    ↓         ↓        ↓
Tespit    Kaydet   Göster
          Event    Liste
         Gönder
```

**Örnek Akış:**
1. ML Plugin şüpheli dosya tespit eder
2. Dosyayı karantina dizinine taşır
3. Veritabanına kaydeder
4. Daemon'a bildirir
5. Daemon GUI'ye `DETECTED_THREATS` eventi gönderir
6. GUI karantina listesini günceller
7. Kullanıcıya bildirim gösterir

---

## Notlar

- Tüm timestamp değerleri Unix epoch (millisecond) formatında olmalıdır
- Hash değerleri SHA-256 formatında olmalıdır
- Karantina dizini güvenli bir konumda olmalıdır (örn: `~/.sentinelfs/quarantine/`)
- Karantinaya alınan dosyalar orijinal isimlerini korumalı ama benzersiz ID ile prefix'lenmelidir
- `markedSafe` olan dosyalar hala karantinada kalır, sadece kullanıcıya görsel feedback verir

---

## Örnek Karantina Dosya Yapısı

```
~/.sentinelfs/quarantine/
├── 1_suspicious_file.exe
├── 2_encrypted_document.docx
├── 3_high_entropy_data.bin
└── metadata.json
```

**metadata.json:**
```json
{
  "1": {
    "originalPath": "/home/user/Downloads/suspicious_file.exe",
    "quarantinedAt": 1701234567890,
    "threatId": 1
  }
}
```
