# SentinelFS - YMH339 Proje Sunumu Senaryosu
**Süre:** 10 Dakika  
**Tarih:** 25 Aralık 2025, 09:30  
**Veritabanı:** `/home/rei/.local/share/sentinelfs/sentinel.db`

---

## 📊 Sunum Akışı

### **1. Giriş ve Proje Tanıtımı (2 dakika)**

**Söylenecekler:**
> "Merhaba, ben [İsim]. SentinelFS projemizi sunacağım. SentinelFS, P2P dosya senkronizasyonu ve güvenlik tehditi tespiti yapan bir sistemdir. Projemizde SQLite veritabanı kullanarak dosya yönetimi, versiyon kontrolü, peer network yönetimi ve ML tabanlı tehdit tespiti gerçekleştiriyoruz."

**Gösterilecekler:**
- Proje mimarisi (ARCHITECTURE.md)
- Veritabanı ER diyagramı (DATABASE_ER_DIAGRAM.md)

**Önemli Noktalar:**
- 14 tablo (files, peers, activity_log, detected_threats, file_versions, vb.)
- 19 dosya, 3 peer, 5 tehdit tespiti
- Gerçek zamanlı çalışan sistem

---

### **2. Veritabanı Fiziksel Tasarımı (3 dakika)**

**Söylenecekler:**
> "Veritabanımız 14 tablodan oluşuyor. Ana tablolarımız:"
> - **files**: Dosya metadata (path, size, hash, version)
> - **peers**: Ağ eşleri (peer_id, address, latency, status)
> - **activity_log**: Tüm dosya operasyonları (op_type, timestamp, peer_id)
> - **detected_threats**: ML ile tespit edilen tehditler (threat_score, entropy)
> - **file_versions**: Dosya versiyon geçmişi

**Gösterilecekler:**
```sql
-- Tablo yapısını göster
.schema files
.schema detected_threats
```

**Önemli Noktalar:**
- Foreign key ilişkileri (CASCADE)
- İndeksler (performans optimizasyonu)
- Triggerlar (otomatik timestamp güncelleme)
- Lookup tablolar (op_types, status_types, threat_types, threat_levels)

---

### **3. Canlı Sorgu Gösterimi (4 dakika)**

#### **SORGU 1: Nested Query - Ortalama Üstü Dosyalar**
```sql
SELECT 
    path,
    size,
    ROUND(size / 1024.0, 2) as size_kb,
    datetime(modified, 'unixepoch') as modified_date
FROM files
WHERE size > (
    SELECT AVG(size) 
    FROM files
)
ORDER BY size DESC
LIMIT 5;
```

**Açıklama:**
> "Bu sorgu nested subquery kullanarak ortalama dosya boyutunun üzerindeki dosyaları buluyor. İç sorgu ortalamayı hesaplıyor, dış sorgu bunu filtre olarak kullanıyor."

**Beklenen Çıktı:**
- powershell_dropper.ps1 (4.91 KB)
- rootkit_module.c (3.75 KB)
- README.md (3.3 KB)

---

#### **SORGU 2: Multi-JOIN - Tehdit Analizi**
```sql
SELECT 
    f.path,
    f.version,
    dt.threat_score,
    tt.name as threat_type,
    tl.name as threat_level,
    datetime(dt.detected_at) as detection_date
FROM detected_threats dt
JOIN files f ON dt.file_id = f.id
JOIN threat_types tt ON dt.threat_type_id = tt.id
JOIN threat_levels tl ON dt.threat_level_id = tl.id
WHERE dt.marked_safe = 0
ORDER BY dt.threat_score DESC
LIMIT 5;
```

**Açıklama:**
> "4 tablo birleştiriyoruz: detected_threats, files, threat_types ve threat_levels. INNER JOIN ile sadece tehdit tespit edilen dosyaları gösteriyoruz."

**Beklenen Çıktı:**
- NEW_RANSOM_NOTE.txt - ANOMALOUS_BEHAVIOR - MEDIUM (0.012)
- xmrig_config.json - ANOMALOUS_BEHAVIOR - MEDIUM (0.01)

---

#### **SORGU 3: GROUP BY + HAVING - Tehdit Seviye İstatistikleri**
```sql
SELECT 
    tl.name as threat_level,
    COUNT(DISTINCT dt.file_id) as file_count,
    ROUND(AVG(dt.threat_score), 4) as avg_threat_score,
    ROUND(AVG(dt.entropy), 4) as avg_entropy
FROM detected_threats dt
JOIN threat_levels tl ON dt.threat_level_id = tl.id
JOIN files f ON dt.file_id = f.id
GROUP BY tl.name
HAVING COUNT(DISTINCT dt.file_id) >= 1
ORDER BY avg_threat_score DESC;
```

**Açıklama:**
> "GROUP BY ile tehdit seviyelerine göre grupluyoruz. HAVING ile en az 1 dosyası olan seviyeleri filtreliyoruz. Agregasyon fonksiyonları (COUNT, AVG) kullanıyoruz."

**Beklenen Çıktı:**
- MEDIUM: 5 dosya, avg_score: 0.0054, avg_entropy: 4.5

---

#### **SORGU 4: Karmaşık Sorgu - Sistem Özeti (UNION)**
```sql
SELECT 
    'Toplam Dosya' as metric,
    COUNT(*) as value,
    ROUND(SUM(size) / 1048576.0, 2) as total_mb
FROM files
UNION ALL
SELECT 'Toplam Peer', COUNT(*), NULL FROM peers
UNION ALL
SELECT 'Aktif Tehdit', COUNT(*), NULL 
FROM detected_threats WHERE marked_safe = 0
UNION ALL
SELECT 'Toplam Aktivite', COUNT(*), NULL FROM activity_log
UNION ALL
SELECT 'Dosya Versiyonları', COUNT(*), NULL FROM file_versions;
```

**Açıklama:**
> "UNION ALL ile farklı tabloların istatistiklerini tek sonuçta birleştiriyoruz. Sistem sağlığının genel görünümünü sağlıyor."

**Beklenen Çıktı:**
- Toplam Dosya: 19 (0.03 MB)
- Toplam Peer: 3
- Aktif Tehdit: 5
- Toplam Aktivite: 0
- Dosya Versiyonları: 19

---

### **4. Sonuç ve Sorular (1 dakika)**

**Söylenecekler:**
> "Özetlemek gerekirse:"
> - ✅ 14 tablolu normalize edilmiş veritabanı
> - ✅ Foreign key ilişkileri ve referential integrity
> - ✅ Nested queries, multi-table JOIN'ler
> - ✅ GROUP BY/HAVING ile agregasyon
> - ✅ Gerçek zamanlı çalışan sistem
> 
> "Sorularınızı alabilirim."

---

## 🎯 Sunum İpuçları

### **Hazırlık (Sunum Öncesi)**
1. ✅ Bilgisayarınızı açık tutun
2. ✅ Terminal hazır (sqlite3 kurulu)
3. ✅ Veritabanı yolu: `/home/rei/.local/share/sentinelfs/sentinel.db`
4. ✅ `sunum_sorgulari.sql` dosyasını açık tutun
5. ✅ ER diyagramını gösterebilecek durumda olun

### **Demo Sırasında**
1. **Sorguları kopyala-yapıştır** (yazma, hata yapma riski)
2. **Sonuçları yorumla** (sadece gösterme, açıkla)
3. **Teknik terimleri vurgula** (subquery, JOIN, GROUP BY, HAVING)
4. **Gerçek dünya bağlantısı kur** (neden bu sorgu önemli?)

### **Olası Sorular ve Cevaplar**

**S: Neden SQLite kullandınız?**
> "Embedded sistem, dosya bazlı, ACID uyumlu, C++ entegrasyonu kolay, transaction desteği var."

**S: Tehdit tespiti nasıl çalışıyor?**
> "ONNX Runtime ile ML modeli, dosya entropisini ve pattern'leri analiz ediyor. Sonuçlar detected_threats tablosuna kaydediliyor."

**S: Peer'lar nasıl senkronize oluyor?**
> "P2P mesh network, activity_log her operasyonu kaydediyor, file_versions ile versiyon kontrolü yapıyoruz."

**S: Performans optimizasyonu yaptınız mı?**
> "Evet, 20+ indeks, composite indeksler, WAL mode, prepared statements kullanıyoruz."

---

## 📝 Hızlı Komutlar (Cheat Sheet)

```bash
# Veritabanını aç
sqlite3 /home/rei/.local/share/sentinelfs/sentinel.db

# Tabloları listele
.tables

# Şema göster
.schema files

# Sorguyu dosyadan çalıştır
.read sunum_sorgulari.sql

# Çıktıyı formatla
.mode column
.headers on

# Çıkış
.quit
```

---

## ⏱️ Zaman Yönetimi

| Bölüm | Süre | Kümülatif |
|-------|------|-----------|
| Giriş | 2 dk | 2 dk |
| Veritabanı Tasarımı | 3 dk | 5 dk |
| Sorgu Gösterimi | 4 dk | 9 dk |
| Sonuç | 1 dk | 10 dk |

**Önemli:** Her sorgu için max 1 dakika ayırın. Zaman kalırsa ekstra sorgu gösterin.

---

## ✅ Son Kontrol Listesi

- [ ] Bilgisayar şarjda/şarjlı
- [ ] Terminal açık ve test edildi
- [ ] Veritabanı erişilebilir
- [ ] Sorgu dosyası hazır
- [ ] ER diyagramı görüntülenebilir
- [ ] Yedek plan var (internet kesilirse, vb.)

---

**Başarılar! 🚀**
