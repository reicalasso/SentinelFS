## Karşılaştırma ve Durum

| Özellik            | NetworkPlugin | NetFalcon                                              |
|--------------------|--------------------|--------------------------------------------------------|
| Taşıyıcı           | Sadece TCP         | TCP + QUIC + WebRTC + Relay (genişletilebilir)        |
| Mimari             | Monolitik          | Katmanlı (Transport → Session → Plugin)               |
| Transport Seçimi   | Sabit              | Akıllı/Adaptif (RTT, packet loss, jitter)             |
| Multi-session      | ❌                 | ✅                                                     |
| Anahtar Döndürme   | ❌                 | ✅                                                     |
| Replay Koruması    | ❌                 | ✅                                                     |
| mDNS Keşfi         | ❌                 | Planlandı                                              |
| Failover           | ❌                 | ✅ Otomatik transport failover                         |
