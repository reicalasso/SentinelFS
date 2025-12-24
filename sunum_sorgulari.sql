-- ============================================================================
-- SentinelFS - YMH339 Proje Sunumu SQL Sorguları
-- Grup: [Grup Adınız]
-- Tarih: 25 Aralık 2025
-- ============================================================================

-- ============================================================================
-- BÖLÜM 1: NESTED QUERIES (Alt Sorgular)
-- ============================================================================

-- SORGU 1: En büyük dosya ortalamasından daha büyük dosyaları bul
-- Açıklama: Subquery ile ortalama dosya boyutunu bulup, ondan büyük dosyaları listeler
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
ORDER BY size DESC;

-- SORGU 2: En yüksek versiyona sahip dosyaları listele
-- Açıklama: Nested query ile en çok değişiklik gören dosyaları bulur
SELECT 
    path,
    size,
    version,
    datetime(modified, 'unixepoch') as last_modified,
    CASE 
        WHEN version >= 5 THEN 'Çok Aktif'
        WHEN version >= 3 THEN 'Aktif'
        ELSE 'Normal'
    END as activity_level
FROM files
WHERE version > (
    SELECT AVG(version)
    FROM files
)
ORDER BY version DESC, modified DESC
LIMIT 10;

-- SORGU 3: Tehdit tespit edilen dosyaların boyut analizi (Nested)
-- Açıklama: Nested query ile tehditli dosyaların boyut dağılımını gösterir
SELECT 
    f.path,
    f.size,
    ROUND(f.size / 1024.0, 2) as size_kb,
    f.version,
    COUNT(dt.id) as threat_count,
    MAX(dt.threat_score) as max_threat_score
FROM files f
JOIN detected_threats dt ON f.id = dt.file_id
WHERE f.size > (
    SELECT AVG(size) 
    FROM files f2
    JOIN detected_threats dt2 ON f2.id = dt2.file_id
)
GROUP BY f.id, f.path, f.size, f.version
ORDER BY threat_count DESC, f.size DESC
LIMIT 10;

-- SORGU 4: Tehdit tespit edilen dosyaların versiyon bilgileri
-- Açıklama: Alt sorgu ile tehdit tespit edilen dosyaları bulup, detaylarını gösterir
SELECT 
    f.path,
    f.version,
    dt.threat_score,
    tt.name as threat_type,
    tl.name as threat_level,
    datetime(dt.detected_at) as detection_date,
    dt.entropy,
    dt.marked_safe
FROM detected_threats dt
JOIN files f ON dt.file_id = f.id
JOIN threat_types tt ON dt.threat_type_id = tt.id
JOIN threat_levels tl ON dt.threat_level_id = tl.id
WHERE dt.file_id IN (
    SELECT DISTINCT file_id 
    FROM detected_threats 
    WHERE marked_safe = 0
)
ORDER BY dt.threat_score DESC, dt.detected_at DESC;


-- ============================================================================
-- BÖLÜM 2: JOIN QUERIES (Tablo Birleştirme)
-- ============================================================================

-- SORGU 5: Dosya ve tehdit detaylarının birleşik görünümü (3-way JOIN)
-- Açıklama: Files, detected_threats ve threat_types tablolarını birleştirerek tam tehdit geçmişi
SELECT 
    f.path as file_path,
    f.size,
    f.version,
    tt.name as threat_type,
    tl.name as threat_level,
    dt.threat_score,
    dt.entropy,
    datetime(dt.detected_at) as detection_time,
    CASE WHEN dt.marked_safe = 1 THEN 'Güvenli İşaretlendi' ELSE 'Aktif Tehdit' END as status
FROM detected_threats dt
INNER JOIN files f ON dt.file_id = f.id
INNER JOIN threat_types tt ON dt.threat_type_id = tt.id
INNER JOIN threat_levels tl ON dt.threat_level_id = tl.id
ORDER BY dt.detected_at DESC, dt.threat_score DESC
LIMIT 20;

-- SORGU 6: Peer durumu ve sistem bilgileri (Multi-table JOIN)
-- Açıklama: Peers ve status_types tablolarını birleştirerek peer detaylarını gösterir
SELECT 
    p.peer_id,
    p.name as peer_name,
    p.address,
    p.port,
    st.name as status,
    p.latency as latency_ms,
    datetime(p.last_seen, 'unixepoch') as last_seen_date,
    (SELECT COUNT(*) FROM files) as total_files_in_system,
    (SELECT COUNT(*) FROM detected_threats WHERE marked_safe = 0) as active_threats,
    CASE 
        WHEN p.latency < 50 THEN 'Mükemmel'
        WHEN p.latency < 100 THEN 'İyi'
        WHEN p.latency < 200 THEN 'Orta'
        ELSE 'Yavaş'
    END as connection_quality
FROM peers p
INNER JOIN status_types st ON p.status_id = st.id
ORDER BY p.latency ASC;

-- SORGU 7: Tehdit analizi (LEFT JOIN ile tüm dosyaları göster)
-- Açıklama: Tüm dosyaları listeler, tehdit varsa detaylarını gösterir
SELECT 
    f.path,
    f.size,
    f.version,
    tt.name as threat_type,
    tl.name as threat_level,
    dt.threat_score,
    dt.entropy,
    datetime(dt.detected_at) as threat_detected,
    dt.marked_safe
FROM files f
LEFT JOIN detected_threats dt ON f.id = dt.file_id
LEFT JOIN threat_types tt ON dt.threat_type_id = tt.id
LEFT JOIN threat_levels tl ON dt.threat_level_id = tl.id
WHERE dt.id IS NOT NULL
ORDER BY dt.threat_score DESC, dt.detected_at DESC;

-- SORGU 8: Dosya ve tehdit karşılaştırması (SELF JOIN benzeri)
-- Açıklama: Aynı dosyanın farklı tehdit tespitlerini karşılaştırır
SELECT 
    f.path,
    f.version,
    dt1.threat_score as latest_threat_score,
    dt1.entropy as latest_entropy,
    tt1.name as latest_threat_type,
    datetime(dt1.detected_at) as latest_detection,
    dt2.threat_score as previous_threat_score,
    tt2.name as previous_threat_type,
    datetime(dt2.detected_at) as previous_detection,
    ROUND(dt1.threat_score - COALESCE(dt2.threat_score, 0), 4) as score_change
FROM detected_threats dt1
JOIN files f ON dt1.file_id = f.id
JOIN threat_types tt1 ON dt1.threat_type_id = tt1.id
LEFT JOIN detected_threats dt2 ON dt1.file_id = dt2.file_id 
    AND dt1.id > dt2.id
LEFT JOIN threat_types tt2 ON dt2.threat_type_id = tt2.id
WHERE dt1.marked_safe = 0
ORDER BY f.path, dt1.detected_at DESC
LIMIT 15;


-- ============================================================================
-- BÖLÜM 3: GROUP BY / HAVING (Gruplama ve Filtreleme)
-- ============================================================================

-- SORGU 9: Peer durumu ve sistem istatistikleri
-- Açıklama: Her peer'in durum bilgisi ve sistem geneli istatistikler
SELECT 
    p.peer_id,
    p.name as peer_name,
    p.address,
    p.port,
    st.name as status,
    p.latency,
    datetime(p.last_seen, 'unixepoch') as last_seen,
    (SELECT COUNT(*) FROM files) as total_files_in_system,
    (SELECT COUNT(*) FROM detected_threats WHERE marked_safe = 0) as active_threats_in_system,
    CASE 
        WHEN p.latency < 50 THEN 'İyi Bağlantı'
        WHEN p.latency < 100 THEN 'Orta Bağlantı'
        ELSE 'Yavaş Bağlantı'
    END as connection_quality
FROM peers p
LEFT JOIN status_types st ON p.status_id = st.id
ORDER BY p.latency ASC;

-- SORGU 10: Dosya boyutu kategorilerine göre dağılım
-- Açıklama: Dosyaları boyut kategorilerine ayırıp, her kategorideki dosya sayısını gösterir
SELECT 
    CASE 
        WHEN size < 1024 THEN 'Küçük (< 1KB)'
        WHEN size < 1048576 THEN 'Orta (1KB - 1MB)'
        WHEN size < 10485760 THEN 'Büyük (1MB - 10MB)'
        ELSE 'Çok Büyük (> 10MB)'
    END as size_category,
    COUNT(*) as file_count,
    ROUND(SUM(size) / 1024.0, 2) as total_size_kb,
    ROUND(AVG(size), 2) as avg_size_bytes,
    MIN(size) as min_size,
    MAX(size) as max_size,
    ROUND(AVG(version), 2) as avg_version
FROM files
GROUP BY size_category
ORDER BY MIN(size);

-- SORGU 11: Tehdit seviyelerine göre analiz
-- Açıklama: Tehdit seviyelerine göre dosya sayısı ve ortalama tehdit skorları
SELECT 
    tl.name as threat_level,
    COUNT(DISTINCT dt.file_id) as file_count,
    ROUND(AVG(dt.threat_score), 4) as avg_threat_score,
    ROUND(AVG(dt.entropy), 4) as avg_entropy,
    ROUND(AVG(f.size) / 1024.0, 2) as avg_file_size_kb,
    SUM(CASE WHEN dt.marked_safe = 1 THEN 1 ELSE 0 END) as marked_safe_count,
    MIN(datetime(dt.detected_at)) as first_detection,
    MAX(datetime(dt.detected_at)) as last_detection
FROM detected_threats dt
JOIN threat_levels tl ON dt.threat_level_id = tl.id
JOIN files f ON dt.file_id = f.id
GROUP BY tl.name
HAVING COUNT(DISTINCT dt.file_id) >= 1
ORDER BY avg_threat_score DESC;

-- SORGU 12: Tehdit tiplerine göre dağılım ve istatistikler
-- Açıklama: Her tehdit tipinin kullanım istatistiklerini gösterir
SELECT 
    tt.name as threat_type,
    COUNT(dt.id) as detection_count,
    COUNT(DISTINCT dt.file_id) as unique_files,
    ROUND(AVG(dt.threat_score), 4) as avg_threat_score,
    ROUND(AVG(dt.entropy), 4) as avg_entropy,
    ROUND(100.0 * COUNT(dt.id) / (SELECT COUNT(*) FROM detected_threats), 2) as percentage,
    MIN(datetime(dt.detected_at)) as first_detection,
    MAX(datetime(dt.detected_at)) as last_detection,
    SUM(CASE WHEN dt.marked_safe = 1 THEN 1 ELSE 0 END) as marked_safe_count
FROM threat_types tt
LEFT JOIN detected_threats dt ON tt.id = dt.threat_type_id
GROUP BY tt.name
HAVING COUNT(dt.id) > 0
ORDER BY detection_count DESC;


-- ============================================================================
-- BÖLÜM 4: KARMAŞIK SORGULAR (Kombinasyonlar)
-- ============================================================================

-- SORGU 13: Dosya güvenlik ve aktivite raporu (Nested + JOIN + GROUP BY)
-- Açıklama: Her dosya için versiyon sayısı, aktivite ve tehdit durumunu gösterir
SELECT 
    f.path,
    f.size,
    f.version as current_version,
    COUNT(DISTINCT fv.version) as version_count,
    COUNT(DISTINCT al.peer_id) as peer_access_count,
    COUNT(DISTINCT dt.id) as threat_detection_count,
    MAX(dt.threat_score) as max_threat_score,
    MAX(datetime(fv.created_at, 'unixepoch')) as latest_version_date,
    MAX(datetime(al.timestamp, 'unixepoch')) as last_activity,
    CASE 
        WHEN COUNT(DISTINCT dt.id) > 0 THEN 'Tehdit Var'
        WHEN COUNT(DISTINCT al.id) = 0 THEN 'Aktivite Yok'
        ELSE 'Normal'
    END as health_status
FROM files f
LEFT JOIN file_versions fv ON f.id = fv.file_id
LEFT JOIN activity_log al ON f.id = al.file_id
LEFT JOIN detected_threats dt ON f.id = dt.file_id AND dt.marked_safe = 0
GROUP BY f.id, f.path, f.size, f.version
HAVING version_count > 1 OR threat_detection_count > 0 OR peer_access_count > 0
ORDER BY threat_detection_count DESC, version_count DESC
LIMIT 15;

-- SORGU 14: Peer ve tehdit ilişki analizi (Nested + JOIN + Subquery)
-- Açıklama: Her peer için sistem geneli tehdit ve dosya istatistikleri
SELECT 
    p.peer_id,
    p.name,
    p.address,
    p.port,
    st.name as status,
    p.latency,
    datetime(p.last_seen, 'unixepoch') as last_seen,
    (SELECT COUNT(*) FROM files) as total_files,
    (SELECT COUNT(*) FROM detected_threats WHERE marked_safe = 0) as active_threats,
    (SELECT ROUND(AVG(size) / 1024.0, 2) FROM files) as avg_file_size_kb,
    (SELECT COUNT(DISTINCT threat_type_id) FROM detected_threats) as threat_types_detected,
    CASE 
        WHEN p.latency < 50 THEN 'Yüksek Performans'
        WHEN p.latency < 150 THEN 'Normal Performans'
        ELSE 'Düşük Performans'
    END as performance_rating
FROM peers p
INNER JOIN status_types st ON p.status_id = st.id
ORDER BY p.latency ASC;

-- SORGU 15: Tehdit tipi analizi (Multi-JOIN + Subquery + GROUP BY)
-- Açıklama: Tehdit tiplerinin detaylı analizi ve dağılımı
SELECT 
    tt.name as threat_type,
    COUNT(*) as total_detections,
    COUNT(DISTINCT dt.file_id) as affected_files,
    ROUND(AVG(dt.threat_score), 4) as avg_threat_score,
    ROUND(AVG(dt.entropy), 4) as avg_entropy,
    SUM(CASE WHEN dt.marked_safe = 1 THEN 1 ELSE 0 END) as marked_safe_count,
    SUM(CASE WHEN dt.marked_safe = 0 THEN 1 ELSE 0 END) as active_threats,
    ROUND(100.0 * SUM(CASE WHEN dt.marked_safe = 1 THEN 1 ELSE 0 END) / COUNT(*), 2) as false_positive_rate,
    MIN(datetime(dt.detected_at)) as first_detection,
    MAX(datetime(dt.detected_at)) as last_detection
FROM detected_threats dt
JOIN threat_types tt ON dt.threat_type_id = tt.id
JOIN files f ON dt.file_id = f.id
WHERE dt.detected_at > (
    SELECT MIN(detected_at) 
    FROM detected_threats
)
GROUP BY tt.name
HAVING COUNT(*) > 0
ORDER BY active_threats DESC, avg_threat_score DESC;

-- SORGU 16: Peer ağ topolojisi ve sistem sağlığı (Complex Multi-JOIN)
-- Açıklama: Peer'ların network durumu ve sistem geneli güvenlik metrikleri
SELECT 
    p.peer_id,
    p.name,
    p.address,
    p.port,
    st.name as status,
    p.latency,
    datetime(p.last_seen, 'unixepoch') as last_seen_date,
    (SELECT COUNT(*) FROM files) as system_total_files,
    (SELECT COUNT(*) FROM detected_threats WHERE marked_safe = 0) as system_active_threats,
    (SELECT ROUND(SUM(size) / 1048576.0, 2) FROM files) as system_total_mb,
    (SELECT COUNT(DISTINCT threat_level_id) FROM detected_threats) as threat_levels_present,
    ROUND((SELECT AVG(threat_score) FROM detected_threats WHERE marked_safe = 0), 4) as avg_system_threat_score,
    CASE 
        WHEN p.latency < 30 THEN 'Kritik Hızlı'
        WHEN p.latency < 100 THEN 'Hızlı'
        WHEN p.latency < 200 THEN 'Orta'
        ELSE 'Yavaş'
    END as network_performance
FROM peers p
INNER JOIN status_types st ON p.status_id = st.id
ORDER BY p.latency ASC;


-- ============================================================================
-- BÖLÜM 5: İSTATİSTİKSEL SORGULAR (Bonus)
-- ============================================================================

-- SORGU 17: Sistem genel sağlık durumu özeti (UNION ile birleştirme)
-- Açıklama: Tüm sistemin genel durumunu tek sorguda özetler
SELECT 
    'Toplam Dosya' as metric,
    COUNT(*) as count,
    ROUND(SUM(size) / 1048576.0, 2) as size_mb,
    'Sistem genelinde kayıtlı dosya sayısı' as description
FROM files
UNION ALL
SELECT 
    'Toplam Peer',
    COUNT(*),
    ROUND(AVG(latency), 2),
    'Ağdaki peer sayısı ve ortalama gecikme'
FROM peers
UNION ALL
SELECT 
    'Aktif Tehdit',
    COUNT(*),
    ROUND(AVG(threat_score), 4),
    'Güvenli işaretlenmemiş tehdit sayısı'
FROM detected_threats WHERE marked_safe = 0
UNION ALL
SELECT 
    'Toplam Tehdit Tespiti',
    COUNT(*),
    ROUND(AVG(entropy), 4),
    'Tüm tehdit tespit kayıtları'
FROM detected_threats
UNION ALL
SELECT 
    'Tehdit Türü Çeşitliliği',
    COUNT(DISTINCT threat_type_id),
    COUNT(DISTINCT threat_level_id),
    'Farklı tehdit tipi ve seviye sayısı'
FROM detected_threats;

-- SORGU 18: Zaman bazlı tehdit tespit trendi
-- Açıklama: Günlük tehdit tespit sayısını ve dosya değişikliklerini gösterir
SELECT 
    date(dt.detected_at) as detection_date,
    COUNT(dt.id) as threat_count,
    COUNT(DISTINCT dt.file_id) as unique_files,
    ROUND(AVG(dt.threat_score), 4) as avg_threat_score,
    ROUND(AVG(dt.entropy), 4) as avg_entropy,
    SUM(CASE WHEN dt.marked_safe = 1 THEN 1 ELSE 0 END) as false_positives,
    SUM(CASE WHEN dt.marked_safe = 0 THEN 1 ELSE 0 END) as active_threats,
    GROUP_CONCAT(DISTINCT tl.name) as threat_levels
FROM detected_threats dt
LEFT JOIN threat_levels tl ON dt.threat_level_id = tl.id
GROUP BY detection_date
ORDER BY detection_date DESC
LIMIT 30;


-- ============================================================================
-- NOTLAR VE AÇIKLAMALAR
-- ============================================================================

/*
SUNUM İÇİN ÖNERİLER:

1. NESTED QUERIES (Sorgu 1-4):
   - Sorgu 2 veya 3'ü gösterin (en etkileyici)
   - Subquery'nin nasıl çalıştığını açıklayın

2. JOIN QUERIES (Sorgu 5-8):
   - Sorgu 5 veya 6'yı gösterin (3-4 tablo birleştirme)
   - JOIN türlerini (INNER, LEFT) vurgulayın

3. GROUP BY/HAVING (Sorgu 9-12):
   - Sorgu 9'u gösterin (istatistiksel analiz)
   - HAVING ile filtrelemeyi açıklayın

4. KARMAŞIK SORGULAR (Sorgu 13-16):
   - Sorgu 13 veya 16'yı gösterin (tüm teknikleri birleştirir)
   - Sorgunun iş mantığını açıklayın

5. BONUS (Sorgu 17-18):
   - Zaman kalırsa Sorgu 17'yi gösterin (sistem özeti)

DEMO SIRASINDA:
- Her sorguyu çalıştırmadan önce ne yaptığını açıklayın
- Sonuçları yorumlayın (örn: "Görüldüğü gibi 3 cihaz aktif...")
- Sorgu performansını vurgulayın (indeksler sayesinde hızlı)
- Gerçek dünya kullanım senaryolarını anlatın

HAZIRLIK:
1. Bu dosyayı bilgisayarınıza kaydedin
2. SQLite veritabanınızı açın
3. Sorguları tek tek test edin
4. Hangi sorguları göstereceğinize karar verin (4-5 sorgu yeterli)
5. Her sorgunun çıktısını önceden görün
*/
