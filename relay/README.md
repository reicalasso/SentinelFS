# SentinelFS Relay Server

Hafif, NAT-arkası peer'lar için relay/introducer sunucusu.

## Özellikler

- **Peer Keşfi**: WAN üzerinden peer'ların birbirini bulmasını sağlar
- **Session Tabanlı Kimlik Doğrulama**: Paylaşımlı session code ile güvenli bağlantı
- **Veri Aktarımı**: Direkt bağlantı kurulamayan peer'lar için relay
- **Heartbeat**: Otomatik ölü peer temizliği
- **REST API**: Bağlı peer listesi için HTTP endpoint

## Kullanım

### Direkt Çalıştırma

```bash
python3 relay_server.py --host 0.0.0.0 --port 9000
```

### Docker ile

```bash
# Build
docker build -t sentinelfs-relay .

# Run
docker run -d -p 9000:9000 --name sentinelfs-relay sentinelfs-relay

# Logs
docker logs -f sentinelfs-relay
```

### Docker Compose

```bash
docker-compose up -d
```

## Yapılandırma

Ortam değişkenleri:

| Değişken | Açıklama | Varsayılan |
|----------|----------|------------|
| `RELAY_HOST` | Dinlenecek adres | `0.0.0.0` |
| `RELAY_PORT` | Dinlenecek port | `9000` |
| `RELAY_LOG_LEVEL` | Log seviyesi | `INFO` |

## Protokol

### Mesaj Formatı

```
[4 bytes: length][1 byte: type][payload]
```

### Mesaj Tipleri

| Tip | Değer | Açıklama |
|-----|-------|----------|
| REGISTER | 0x01 | Peer kaydı |
| UNREGISTER | 0x02 | Kayıt silme |
| HEARTBEAT | 0x03 | Yaşam sinyali |
| PEER_LIST | 0x04 | Peer listesi sorgusu |
| DATA | 0x05 | Veri aktarımı |
| ACK | 0x06 | Onay |
| ERROR | 0xFF | Hata |

### Kayıt İsteği (JSON payload)

```json
{
  "peer_id": "unique-peer-id",
  "session_code": "shared-session",
  "public_endpoint": "optional-ip:port",
  "capabilities": {
    "nat_type": "symmetric|cone|open",
    "upnp": true,
    "local_endpoints": ["192.168.1.100:8080"]
  }
}
```

### Veri Aktarımı (JSON payload)

```json
{
  "target_peer": "target-peer-id",
  "data": "base64-encoded-data"
}
```

## REST API

### GET /peers

Session'daki tüm peer'ları listeler.

```bash
curl "http://relay:9000/peers?session=my-session"
```

Response:
```json
{
  "session": "my-session",
  "peers": [
    {
      "peer_id": "peer1",
      "public_endpoint": "1.2.3.4:12345",
      "capabilities": {...},
      "connected_at": "2024-01-01T12:00:00"
    }
  ],
  "count": 1
}
```

## NAT Traversal Stratejisi

### 1. UPnP (Otomatik Port Yönlendirme)
- İlk olarak UPnP ile port açmaya çalış
- Başarılı olursa direkt P2P bağlantı kur

### 2. TCP Hole Punching
- Her iki peer relay üzerinden koordine edilir
- Eşzamanlı bağlantı denemeleri yapılır
- Symmetric NAT'ta çalışmayabilir

### 3. Relay Fallback
- Direkt bağlantı kurulamazsa relay kullan
- Daha yüksek latency ama her zaman çalışır

## Güvenlik Notları

- Session code'lar yeterince uzun ve rastgele olmalı (min 16 karakter)
- Production'da TLS/SSL kullanın
- Rate limiting aktif edin
- Firewall kurallarını gözden geçirin

## Monitoring

Prometheus metrikleri için:
```bash
curl http://relay:9000/metrics
```

Metrikler:
- `relay_connected_peers`: Bağlı peer sayısı
- `relay_sessions_active`: Aktif session sayısı
- `relay_bytes_relayed`: Aktarılan toplam byte
- `relay_messages_total`: İşlenen mesaj sayısı
