# 04 – DB Şeması & Transaction Tasarımı

## Amaç
- Metadata katmanını SentinelFS-Neo şemasına yaklaştırmak.
- Tüm kritik güncellemeleri atomic transaction’lar içinde yapmak.

## Gereksinimler
- Ek tablolar: Device, Session, FileVersion, SyncQueue, FileAccessLog.
- Mevcut tablolarla (files, peers, conflicts, config) uyumlu migration.

## Plan
1. **Şema Tasarımı**
   - [x] Device tablosu: device_id, name, last_seen, platform, version.
   - [x] Session tablosu: session_id, device_id, created_at, last_active, session_code hash.
   - [x] FileVersion tablosu: id, file_path, version, hash, timestamp, size, device_id.
   - [x] SyncQueue tablosu: id, file_path, op_type, status, created_at, last_retry, retry_count.
   - [x] FileAccessLog tablosu: id, file_path, op_type, device_id, timestamp.

2. **Migration Stratejisi**
   - [x] SQLiteHandler’da versiyonlama için basit bir `PRAGMA user_version` kullan.
   - [x] Eski versiyondan yeni versiyona geçiş SQL’lerini tanımla.
   - [x] Boş/temiz kurulum için doğrudan yeni şemayı oluşturan SQL seti hazırla.

3. **Transaction Kullanımı**
   - [x] FileMetadataManager, PeerManager, ConflictManager içindeki kritik CRUD akışlarını gözden geçir.
   - [x] Birden fazla tabloyu etkileyen işlemler için `BEGIN IMMEDIATE; ... COMMIT;` blokları ekle.
   - [x] Hata durumunda `ROLLBACK` çağrılarının doğru yapıldığından emin ol.

4. **Yeni API Yüzeyleri**
   - [x] IStorageAPI’ye gerekirse versiyon/sync queue ile ilgili minimal ek metotlar eklemeyi değerlendir.
   - [x] StoragePlugin tarafında yeni tablolar için küçük manager sınıfları (DeviceManager, SessionManager, SyncQueueManager, FileAccessLogManager) tasarla.

5. **Test & Doğrulama**
   - [x] Unit test: her manager için CRUD + edge case senaryoları.
   - [x] Migration test: eski bir DB dosyası ile açılışta otomatik upgrade senaryosu.
   - [x] Yük testi: çok sayıda file/peer ile indekslerin performansını ölç.
