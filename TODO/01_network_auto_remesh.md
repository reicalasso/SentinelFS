# 01 – Network Auto-Remesh & Metrics

## Amaç
- P2P mesh’i RTT, jitter ve packet-loss metriklerine göre otomatik yeniden şekillendirmek.
- Zayıf bağlantıları düşürüp, düşük gecikmeli ve stabil peer setini korumak.

## Gereksinimler
- Peer başına düzenli RTT ölçümü (zaten var) + jitter ve packet-loss tahmini.
- Storage katmanında latency/health metriklerinin saklanması (mevcut PeerInfo kullanılacak).
- Auto-remesh kararlarını veren merkezi ama basit bir bileşen (örn. `AutoRemeshManager`).

## Plan
1. **Metrik Toplama Katmanı**
   - [ ] TCPHandler içinde ping/pong tabanlı RTT ölçümü için mevcut API’yi gözden geçir.
   - [ ] Aynı ping akışından jitter ve packet-loss oranını tahmin edecek basit hesaplayıcı ekle.
   - [ ] PeerMetrics yapısı tasarla (RTT avg, RTT stddev, packet-loss %, lastSeen vs.).

2. **Metriklerin Saklanması**
   - [ ] `PeerInfo`yu bozmadan, gerekirse ayrı bir in-memory map ile runtime health bilgilerini tut.
   - [ ] Periyodik ölçümleri MetricsCollector’a ve sqlite’taki peers tablosuna yansıt.

3. **AutoRemeshManager Tasarımı**
   - [ ] `sfs::net` içinde `AutoRemeshManager` benzeri küçük bir sınıf tasarla.
   - [ ] Girdi: mevcut peer listesi, metrikler, hedef `max_active_peers` gibi bir konfig.
   - [ ] Çıktı: bağlanılacak ve koparılacak peer listeleri.
   - [ ] Basit politika: RTT + packet-loss’e göre skorla, en iyi N peer’i aktif tut.

4. **Daemon Entegrasyonu**
   - [ ] Daemon içinde ayrı bir thread/timer ile AutoRemeshManager’ı periyodik çalıştır.
   - [ ] Kararlara göre `INetworkAPI` üzerinden connect/disconnect çağrılarını yap.
   - [ ] Loglama: her remesh turunda kısa bir özet yaz (kaç peer eklendi/silindi).

5. **Test & Gözlemlenebilirlik**
   - [ ] UNIT: AutoRemeshManager için saf C++ unit test’leri (farklı metrik setleri için doğru kararlar?).
   - [ ] İNTEG: 3–4 sahte peer ile RTT’leri değiştirerek remesh davranışını gözlemle.
   - [ ] Metrics: remesh sayısı, remesh sonucu ortalama RTT iyileşmesi gibi metrikler ekle.
