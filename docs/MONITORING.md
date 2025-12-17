# SentinelFS İzleme Kılavuzu

**Versiyon:** 1.0.0

---

## 📊 Metrikler

### Endpoint
- **URL**: `http://<host>:9100/metrics`
- **Format**: Prometheus text exposition
- **Güncelleme**: Real-time

### Önemli Metrikler

| Kategori | Metrik | Açıklama |
|:---------|:-------|:---------|
| **Genel** | `sentinelfs_peers_connected` | Bağlı peer sayısı |
| **Sync** | `sentinelfs_files_synced_total` | Senkronize dosya |
| **Network** | `sentinelfs_bytes_uploaded_total` | Toplam upload |
| **Güvenlik** | `sentinelfs_anomalies_detected_total` | Tespit edilen anomali |
| **Performans** | `sentinelfs_sync_latency_ms` | Sync gecikmesi |

---

## 📈 Prometheus Yapılandırması

### `prometheus.yml`
```yaml
scrape_configs:
  - job_name: 'sentinelfs'
    static_configs:
      - targets: ['localhost:9100']
    scrape_interval: 15s
```

### Çoklu Instance
```yaml
scrape_configs:
  - job_name: 'sentinelfs'
    static_configs:
      - targets:
          - 'node1.local:9100'
          - 'node2.local:9100'
```

---

## 🎯 Grafana Dashboard

### Kurulum
1. Grafana → Dashboards → Import
2. `sentinelfs-dashboard.json` yükle
3. Prometheus data source seç

### Panel Listesi
- **Uptime** - Daemon çalışma süresi
- **Connected Peers** - Aktif bağlantılar
- **Transfer Rate** - Upload/Download hızı
- **Error Rate** - Hata oranı
- **Security Events** - Güvenlik olayları

---

## 🔍 Health Check'ler

### Liveness (`/healthz`)
```bash
curl http://localhost:9100/healthz
# Response: "ok" (HTTP 200)
```

### Readiness (`/readyz`)
```bash
curl http://localhost:9100/readyz
# Response: "ready" (HTTP 200)
```

### Kubernetes
```yaml
livenessProbe:
  httpGet:
    path: /healthz
    port: 9100
  initialDelaySeconds: 10
  periodSeconds: 10

readinessProbe:
  httpGet:
    path: /readyz
    port: 9100
  initialDelaySeconds: 15
```

---

## 🛠️ Systemd Yapılandırması

### Service Dosyası
`/etc/systemd/system/sentinelfs.service`:

```ini
[Unit]
Description=SentinelFS P2P File Synchronization
After=network-online.target

[Service]
Type=simple
User=sentinelfs
ExecStart=/usr/bin/sentinel_daemon --config /etc/sentinelfs/sentinel.conf
Restart=on-failure
RestartSec=5

# Security
NoNewPrivileges=true
ProtectSystem=strict
ReadWritePaths=/var/lib/sentinelfs /var/log/sentinelfs

[Install]
WantedBy=multi-user.target
```

### Servis Yönetimi
```bash
sudo systemctl enable sentinelfs
sudo systemctl start sentinelfs
sudo systemctl status sentinelfs
sudo journalctl -u sentinelfs -f
```

---

## 📝 Log Yönetimi

### Log Seviyeleri
- `TRACE` - Detaylı debug
- `DEBUG` - Debug bilgisi
- `INFO` - Normal operasyon
- `WARNING` - Potansiyel sorun
- `ERROR` - Hata durumu

### Log Rotation
`/etc/logrotate.d/sentinelfs`:

```
/var/log/sentinelfs/*.log {
    daily
    rotate 14
    compress
    missingok
    notifempty
    create 0640 sentinelfs sentinelfs
}
```

---

## 🚨 Alerting

### Prometheus Kuralları
```yaml
groups:
  - name: sentinelfs
    rules:
      - alert: SentinelFSDown
        expr: up{job="sentinelfs"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "SentinelFS daemon is down"

      - alert: SentinelFSNoPeers
        expr: sentinelfs_peers_connected == 0
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "No peers connected"

      - alert: SentinelFSHighErrorRate
        expr: rate(sentinelfs_sync_errors_total[5m]) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High sync error rate"
```

---

## 🔧 Sorun Giderme

### Metrikler Görünmüyor
```bash
curl -v http://localhost:9100/metrics
sudo iptables -L -n | grep 9100
```

### Health Check Başarısız
```bash
curl -s http://localhost:9100/readyz
journalctl -u sentinelfs -n 50
```

---

*SentinelFS Operations Team - Aralık 2025*
