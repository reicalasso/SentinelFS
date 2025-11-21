# 06 – QoS & Bandwidth Limiting

## Amaç
- upload/download hız limitlerini gerçekten uygulayan bir rate-limiting katmanı eklemek.
- CLI/IPС üzerinden gelen limit değişikliklerini desteklemek.

## Gereksinimler
- DaemonConfig’teki `uploadLimit` / `downloadLimit` alanları kullanılacak.
- IPC `UPLOAD-LIMIT` ve `DOWNLOAD-LIMIT` komutları gerçek implementasyona sahip olacak.

## Plan
1. **Rate Limiter Tasarımı**
   - [x] Basit bir token-bucket veya leaky-bucket implementasyonu yaz.
   - [x] Ayrı limiter örnekleri: upload ve download için iki farklı kova.
   - [x] Zaman bazlı refill mekanizmasını netleştir (örn. 100ms per tick).

2. **Network Katmanı Entegrasyonu**
   - [x] TCPHandler’da send path’ine upload limiter’ı entegre et.
   - [x] Download için okuma tarafında pacing gerekiyorsa, uygun noktaya limiter ekle.
   - [x] Limit 0 (unlimited) iken limiter bypass edilecek.

3. **Daemon & IPC Entegrasyonu**
   - [x] IPCHandler’ın `UPLOAD-LIMIT` / `DOWNLOAD-LIMIT` komutlarını, runtime’da config güncelleyecek şekilde doldur.
   - [x] Yeni limit değerlerini network plugin’e/daemon’a aktaran setter’lar ekle.

4. **Test & Doğrulama**
   - [x] Unit test: rate limiter’ın teorik hız ile pratikteki byte akışını karşılaştır.
   - [x] İNTEG: iki peer arasında farklı limit değerlerinde transfer sürelerini ölç.
   - [x] Metrics: anlık efektif throughput, drop edilen/bekletilen byte sayısı.
