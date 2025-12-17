# SentinelFS Tehdit Karantina Sistemi

**Versiyon:** 1.0.0

---

## 🎯 Genel Bakış

Tehdit Karantina Merkezi, ML tabanlı tehdit tespiti sonucunda şüpheli dosyaları yönetir.

### Özellikler
- ✅ Tehdit listesi ve detayları
- ✅ Dosya silme ve güvenli işaretleme
- ✅ Filtreleme ve arama
- ✅ Dashboard entegrasyonu
- ✅ Bildirim sistemi

---

## 🏗️ Mimari

```
ML Plugin → Karantina → GUI
Tespit    → Kaydet   → Göster
```

### Dosya Yapısı
```
~/.sentinelfs/quarantine/
├── 1_suspicious_file.exe
├── 2_encrypted_doc.docx
└── metadata.json
```

---

## 🔌 Backend Entegrasyonu

### Gerekli Komutlar
| Komut | Açıklama |
|:------|:---------|
| `DELETE_THREAT <id>` | Tehdidi sil |
| `MARK_THREAT_SAFE <id>` | Güvenli işaretle |
| `UNMARK_THREAT_SAFE <id>` | İşareti kaldır |

### Event Formatı
```json
{
  "type": "DETECTED_THREATS",
  "payload": [
    {
      "id": 1,
      "filePath": "/path/to/file",
      "threatType": "RANSOMWARE",
      "threatLevel": "CRITICAL",
      "threatScore": 95.5,
      "detectedAt": 1701234567890,
      "markedSafe": false
    }
  ]
}
```

---

## 📊 Database Schema

```sql
CREATE TABLE detected_threats (
  id INTEGER PRIMARY KEY,
  file_path TEXT NOT NULL,
  threat_type TEXT NOT NULL,
  threat_level TEXT NOT NULL,
  threat_score REAL NOT NULL,
  detected_at INTEGER NOT NULL,
  marked_safe INTEGER DEFAULT 0
);
```

---

## 🎨 UI Bileşenleri

### Modal Yapısı
```
┌─────────────────────────────────────┐
│  🛡️ Tehdit Karantina Merkezi        │
├─────────────────────────────────────┤
│  🔍 [Arama] [Filtre] [Sırala]      │
├──────────────┬────────────────────┤
│   Tehdit     │     Detaylar       │
│   Listesi    │                    │
│              │ ⚠️ KRİTİK TEHDİT │
│ 📄 file.exe  │ 📋 Bilgiler       │n│              │ 🧠 Analiz         │
├──────────────┴────────────────────┤
│     [✅ Güvenli] [🗑️ Sil]        │
└─────────────────────────────────────┘
```

---

## 🧪 Test

### Mock Data
```typescript
window.api.handleData({
  type: "DETECTED_THREATS",
  payload: [{
    id: 1,
    filePath: "/home/user/test.exe",
    threatType: "RANSOMWARE",
    threatLevel: "CRITICAL",
    threatScore: 95.5,
    detectedAt: Date.now(),
    markedSafe: false
  }]
});
```

---

## 📝 Implementasyon Checklist

### Backend (C++)
- [ ] DELETE_THREAT komutu
- [ ] MARK_THREAT_SAFE komutu
- [ ] Karantina dizini yönetimi
- [ ] DETECTED_THREATS eventi

### Frontend (TypeScript)
- [ ] QuarantineCenter modal
- [ ] Threat listesi
- [ ] Detay paneli
- [ ] Aksiyon butonları

---

*SentinelFS Security Team - Aralık 2025*
