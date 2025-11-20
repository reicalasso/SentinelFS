# 06 – QoS & Bandwidth Limiting

## Amaç
- upload/download hız limitlerini gerçekten uygulayan bir rate-limiting katmanı eklemek.
- CLI/IPС üzerinden gelen limit değişikliklerini desteklemek.

## Gereksinimler
- DaemonConfig’teki `uploadLimit` / `downloadLimit` alanları kullanılacak.
- IPC `UPLOAD-LIMIT` ve `DOWNLOAD-LIMIT` komutları gerçek implementasyona sahip olacak.

## Plan
1. **Rate Limiter Tasarımı**
   - [ ] Basit bir token-bucket veya leaky-bucket implementasyonu yaz.
   - [ ] Ayrı limiter örnekleri: upload ve download için iki farklı kova.
   - [ ] Zaman bazlı refill mekanizmasını netleştir (örn. 100ms per tick).

2. **Network Katmanı Entegrasyonu**
   - [ ] TCPHandler’da send path’ine upload limiter’ı entegre et.
   - [ ] Download için okuma tarafında pacing gerekiyorsa, uygun noktaya limiter ekle.
   - [ ] Limit 0 (unlimited) iken limiter bypass edilecek.

3. **Daemon & IPC Entegrasyonu**
   - [ ] IPCHandler’ın `UPLOAD-LIMIT` / `DOWNLOAD-LIMIT` komutlarını, runtime’da config güncelleyecek şekilde doldur.
   - [ ] Yeni limit değerlerini network plugin’e/daemon’a aktaran setter’lar ekle.

4. **Test & Doğrulama**
   - [ ] Unit test: rate limiter’ın teorik hız ile pratikteki byte akışını karşılaştır.
   - [ ] İNTEG: iki peer arasında farklı limit değerlerinde transfer sürelerini ölç.
   - [ ] Metrics: anlık efektif throughput, drop edilen/bekletilen byte sayısı.
