# 04 – DB Şeması & Transaction Tasarımı

## Amaç
- Metadata katmanını SentinelFS-Neo şemasına yaklaştırmak.
- Tüm kritik güncellemeleri atomic transaction’lar içinde yapmak.

## Gereksinimler
- Ek tablolar: Device, Session, FileVersion, SyncQueue, FileAccessLog.
- Mevcut tablolarla (files, peers, conflicts, config) uyumlu migration.

## Plan
1. **Şema Tasarımı**
   - [ ] Device tablosu: device_id, name, last_seen, platform, version.
   - [ ] Session tablosu: session_id, device_id, created_at, last_active, session_code hash.
   - [ ] FileVersion tablosu: id, file_path, version, hash, timestamp, size, device_id.
   - [ ] SyncQueue tablosu: id, file_path, op_type, status, created_at, last_retry, retry_count.
   - [ ] FileAccessLog tablosu: id, file_path, op_type, device_id, timestamp.

2. **Migration Stratejisi**
   - [ ] SQLiteHandler’da versiyonlama için basit bir `PRAGMA user_version` kullan.
   - [ ] Eski versiyondan yeni versiyona geçiş SQL’lerini tanımla.
   - [ ] Boş/temiz kurulum için doğrudan yeni şemayı oluşturan SQL seti hazırla.

3. **Transaction Kullanımı**
   - [ ] FileMetadataManager, PeerManager, ConflictManager içindeki kritik CRUD akışlarını gözden geçir.
   - [ ] Birden fazla tabloyu etkileyen işlemler için `BEGIN IMMEDIATE; ... COMMIT;` blokları ekle.
   - [ ] Hata durumunda `ROLLBACK` çağrılarının doğru yapıldığından emin ol.

4. **Yeni API Yüzeyleri**
   - [ ] IStorageAPI’ye gerekirse versiyon/sync queue ile ilgili minimal ek metotlar eklemeyi değerlendir.
   - [ ] StoragePlugin tarafında yeni tablolar için küçük manager sınıfları (DeviceManager, SessionManager, SyncQueueManager, FileAccessLogManager) tasarla.

5. **Test & Doğrulama**
   - [ ] Unit test: her manager için CRUD + edge case senaryoları.
   - [ ] Migration test: eski bir DB dosyası ile açılışta otomatik upgrade senaryosu.
   - [ ] Yük testi: çok sayıda file/peer ile indekslerin performansını ölç.
