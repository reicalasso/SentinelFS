# SentinelFS Monitoring & Operations Guide

Bu doküman SentinelFS daemon'u için Prometheus metrikleri, Grafana dashboard'u, health-check endpoint'leri, systemd yapılandırması ve log yönetimini kapsar.

## İçindekiler

1. [Prometheus Metrikleri](#prometheus-metrikleri)
2. [Grafana Dashboard](#grafana-dashboard)
3. [Health-Check Endpoint'leri](#health-check-endpointleri)
4. [Systemd Yapılandırması](#systemd-yapılandırması)
5. [Log Formatı ve Rotation](#log-formatı-ve-rotation)

---

## Prometheus Metrikleri

SentinelFS daemon'u `/metrics` endpoint'i üzerinden Prometheus formatında metrikler sunar.

### Endpoint Bilgileri

- **URL**: `http://<host>:<metrics_port>/metrics`
- **Default Port**: `9100` (config ile değiştirilebilir)
- **Format**: Prometheus text exposition format (version 0.0.4)

### Mevcut Metrikler

#### Daemon Bilgileri
| Metrik | Tip | Açıklama |
|--------|-----|----------|
| `sentinelfs_info` | gauge | Daemon versiyon bilgisi (label: version) |
| `sentinelfs_uptime_seconds` | counter | Daemon uptime (saniye) |

#### Sync Metrikleri
| Metrik | Tip | Açıklama |
|--------|-----|----------|
| `sentinelfs_files_watched_total` | gauge | İzlenen dosya sayısı |
| `sentinelfs_files_synced_total` | counter | Toplam senkronize edilen dosya |
| `sentinelfs_files_modified_total` | counter | Algılanan dosya değişikliği |
| `sentinelfs_files_deleted_total` | counter | Algılanan dosya silme |
| `sentinelfs_sync_errors_total` | counter | Sync hataları |
| `sentinelfs_conflicts_detected_total` | counter | Algılanan çakışmalar |

#### Network Metrikleri
| Metrik | Tip | Açıklama |
|--------|-----|----------|
| `sentinelfs_bytes_uploaded_total` | counter | Toplam upload (byte) |
| `sentinelfs_bytes_downloaded_total` | counter | Toplam download (byte) |
| `sentinelfs_peers_discovered_total` | counter | Keşfedilen peer sayısı |
| `sentinelfs_peers_connected` | gauge | Bağlı peer sayısı |
| `sentinelfs_peers_disconnected_total` | counter | Kopan bağlantı sayısı |
| `sentinelfs_transfers_completed_total` | counter | Başarılı transfer sayısı |
| `sentinelfs_transfers_failed_total` | counter | Başarısız transfer sayısı |
| `sentinelfs_deltas_sent_total` | counter | Gönderilen delta sayısı |
| `sentinelfs_deltas_received_total` | counter | Alınan delta sayısı |
| `sentinelfs_remesh_cycles_total` | counter | Auto-remesh döngü sayısı |
| `sentinelfs_active_transfers` | gauge | Aktif transfer sayısı |

#### Güvenlik Metrikleri
| Metrik | Tip | Açıklama |
|--------|-----|----------|
| `sentinelfs_anomalies_detected_total` | counter | ML ile algılanan anomali |
| `sentinelfs_suspicious_activities_total` | counter | Şüpheli aktivite |
| `sentinelfs_sync_paused_total` | counter | Güvenlik nedeniyle duraklatma |
| `sentinelfs_auth_failures_total` | counter | Kimlik doğrulama hatası |
| `sentinelfs_encryption_errors_total` | counter | Şifreleme hatası |

#### Performans Metrikleri
| Metrik | Tip | Açıklama |
|--------|-----|----------|
| `sentinelfs_sync_latency_ms` | gauge | Ortalama sync gecikmesi (ms) |
| `sentinelfs_delta_compute_time_ms` | gauge | Delta hesaplama süresi (ms) |
| `sentinelfs_transfer_speed_kbps` | gauge | Transfer hızı (KB/s) |
| `sentinelfs_memory_usage_mb` | gauge | Peak bellek kullanımı (MB) |
| `sentinelfs_cpu_usage_percent` | gauge | CPU kullanımı (%) |
| `sentinelfs_remesh_rtt_improvement_ms` | gauge | RTT iyileştirmesi (ms) |

### Prometheus Yapılandırması

`prometheus.yml` içine ekleyin:

```yaml
scrape_configs:
  - job_name: 'sentinelfs'
    static_configs:
      - targets: ['localhost:9100']
    scrape_interval: 15s
    scrape_timeout: 10s
```

Birden fazla SentinelFS instance için:

```yaml
scrape_configs:
  - job_name: 'sentinelfs'
    static_configs:
      - targets:
          - 'node1.example.com:9100'
          - 'node2.example.com:9100'
          - 'node3.example.com:9100'
        labels:
          cluster: 'production'
```

---

## Grafana Dashboard

### Kurulum

1. Grafana'yı açın ve **Dashboards > Import** seçin
2. `docs/grafana/sentinelfs-dashboard.json` dosyasını yükleyin
3. Prometheus data source'u seçin
4. **Import** butonuna tıklayın

### Dashboard Panelleri

#### Overview Satırı
- **Uptime**: Daemon çalışma süresi
- **Connected Peers**: Bağlı peer sayısı
- **Files Watched**: İzlenen dosya sayısı
- **Active Transfers**: Aktif transfer sayısı
- **Sync Latency**: Ortalama gecikme
- **Sync Errors**: Hata sayısı

#### Sync Activity
- **File Operations Rate**: Dosya işlemleri/dakika (sync, modify, delete)
- **Transfer Operations**: Transfer başarı/başarısızlık oranı

#### Network
- **Network Throughput**: Upload/download hızı
- **Peer Connections**: Bağlantı geçmişi

#### Performance
- **Latency & Compute Time**: Gecikme metrikleri
- **Memory Usage**: Bellek kullanımı
- **CPU Usage**: İşlemci kullanımı

#### Security & Errors
- **Security Events**: Anomali, şüpheli aktivite, auth hatası
- **Errors & Conflicts**: Sync hataları, şifreleme hataları, çakışmalar

#### Delta Sync & Auto-Remesh
- **Delta Sync Operations**: Delta gönderme/alma
- **Auto-Remesh Performance**: RTT iyileştirme ve remesh döngüleri

### Alerting Örnekleri

Grafana'da alert rule oluşturmak için:

```yaml
# High Error Rate Alert
- alert: SentinelFSHighErrorRate
  expr: rate(sentinelfs_sync_errors_total[5m]) > 0.1
  for: 5m
  labels:
    severity: warning
  annotations:
    summary: "High sync error rate detected"
    description: "Error rate is {{ $value }} errors/sec"

# No Connected Peers
- alert: SentinelFSNoPeers
  expr: sentinelfs_peers_connected == 0
  for: 5m
  labels:
    severity: critical
  annotations:
    summary: "No peers connected"
    description: "SentinelFS has no connected peers for 5 minutes"

# High Memory Usage
- alert: SentinelFSHighMemory
  expr: sentinelfs_memory_usage_mb > 512
  for: 10m
  labels:
    severity: warning
  annotations:
    summary: "High memory usage"
    description: "Memory usage is {{ $value }}MB"
```

---

## Health-Check Endpoint'leri

Production ortamları için Kubernetes-uyumlu health-check endpoint'leri:

### Liveness Probe (`/healthz`)

**Amaç**: Daemon process'inin hayatta olup olmadığını kontrol eder.

```bash
curl -s http://localhost:9100/healthz
# Başarılı: HTTP 200, Body: "ok"
# Başarısız: HTTP 503, Body: "unhealthy"
```

**Kontrol ettikleri**:
- Daemon main loop çalışıyor mu

**Kubernetes yapılandırması**:
```yaml
livenessProbe:
  httpGet:
    path: /healthz
    port: 9100
  initialDelaySeconds: 10
  periodSeconds: 10
  timeoutSeconds: 5
  failureThreshold: 3
```

### Readiness Probe (`/readyz`)

**Amaç**: Daemon'un trafik almaya hazır olup olmadığını kontrol eder.

```bash
curl -s http://localhost:9100/readyz
# Başarılı: HTTP 200, Body: "ready"
# Başarısız: HTTP 503, Body: "not ready"
```

**Kontrol ettikleri**:
- Daemon running
- Network plugin aktif
- Storage plugin aktif
- Database erişilebilir

**Kubernetes yapılandırması**:
```yaml
readinessProbe:
  httpGet:
    path: /readyz
    port: 9100
  initialDelaySeconds: 15
  periodSeconds: 10
  timeoutSeconds: 5
  failureThreshold: 3
```

### Startup Probe (Önerilen)

Yavaş başlayan ortamlar için:
```yaml
startupProbe:
  httpGet:
    path: /healthz
    port: 9100
  initialDelaySeconds: 5
  periodSeconds: 5
  failureThreshold: 30  # 150 saniyeye kadar başlangıç süresi
```

---

## Systemd Yapılandırması

### Unit Dosyası

`/etc/systemd/system/sentinel_daemon.service`:

```ini
[Unit]
Description=SentinelFS P2P File Synchronization Daemon
After=network-online.target
Wants=network-online.target
Documentation=https://github.com/sentinelfs/sentinelfs

[Service]
Type=simple
User=sentinelfs
Group=sentinelfs
ExecStart=/usr/local/bin/sentinel_daemon --config /etc/sentinelfs/sentinel.conf
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5
TimeoutStartSec=60
TimeoutStopSec=30

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=sentinel_daemon

# Security hardening
RuntimeDirectory=sentinelfs
RuntimeDirectoryMode=0755
AmbientCapabilities=CAP_NET_BIND_SERVICE
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=read-only
PrivateTmp=yes
ProtectKernelTunables=yes
ProtectControlGroups=yes
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6
SystemCallFilter=@system-service
ReadWritePaths=/var/lib/sentinelfs /var/log/sentinelfs

# Resource limits
MemoryMax=1G
CPUQuota=80%

[Install]
WantedBy=multi-user.target
```

### Kurulum

```bash
# Kullanıcı ve grup oluştur
sudo useradd -r -s /sbin/nologin sentinelfs

# Dizinleri oluştur
sudo mkdir -p /etc/sentinelfs /var/lib/sentinelfs /var/log/sentinelfs
sudo chown sentinelfs:sentinelfs /var/lib/sentinelfs /var/log/sentinelfs

# Service dosyasını kopyala
sudo cp sentinel_daemon.service /etc/systemd/system/

# Systemd'yi yenile ve başlat
sudo systemctl daemon-reload
sudo systemctl enable sentinel_daemon
sudo systemctl start sentinel_daemon
```

### Yönetim Komutları

```bash
# Durum kontrolü
sudo systemctl status sentinel_daemon

# Logları görüntüle
sudo journalctl -u sentinel_daemon -f

# Yeniden başlat
sudo systemctl restart sentinel_daemon

# Yapılandırmayı yeniden yükle (HUP sinyali)
sudo systemctl reload sentinel_daemon
```

---

## Log Formatı ve Rotation

### Log Formatı

SentinelFS daemon'u şu formatta log üretir:

```
[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [COMPONENT] Message
```

**Örnek**:
```
[2025-12-04 14:30:22.456] [INFO] [DaemonCore] Daemon initialization complete
[2025-12-04 14:30:22.460] [INFO] [Network] TCP listener started on port 42000
[2025-12-04 14:30:25.123] [DEBUG] [FileWatcher] File modified: /home/user/sync/document.txt
[2025-12-04 14:30:25.200] [WARN] [Security] Rate limit exceeded for peer abc123
[2025-12-04 14:30:30.500] [ERROR] [Transfer] Failed to transfer file: connection timeout
```

### Log Seviyeleri

| Seviye | Kullanım |
|--------|----------|
| `DEBUG` | Detaylı debugging bilgisi |
| `INFO` | Normal operasyonel mesajlar |
| `WARN` | Uyarılar (işlem devam ediyor) |
| `ERROR` | Hatalar (işlem başarısız) |
| `FATAL` | Kritik hatalar (daemon durur) |

### Journald ile Log Yönetimi

Systemd journald kullanıldığında:

```bash
# Son 100 satır
sudo journalctl -u sentinel_daemon -n 100

# Canlı takip
sudo journalctl -u sentinel_daemon -f

# Belirli zaman aralığı
sudo journalctl -u sentinel_daemon --since "2025-12-04 14:00" --until "2025-12-04 15:00"

# Sadece hatalar
sudo journalctl -u sentinel_daemon -p err

# JSON formatında export
sudo journalctl -u sentinel_daemon -o json --since today > sentinel_logs.json
```

### Journal Retention Yapılandırması

`/etc/systemd/journald.conf`:

```ini
[Journal]
# Maksimum disk kullanımı
SystemMaxUse=500M

# Tek log dosyası boyutu
SystemMaxFileSize=50M

# Retention süresi
MaxRetentionSec=30day

# Sıkıştırma
Compress=yes
```

Değişiklik sonrası:
```bash
sudo systemctl restart systemd-journald
```

### Dosya Tabanlı Log Rotation (Alternatif)

Journald yerine dosya tabanlı loglama için:

1. **Config'de log dosyası belirtin**:
   ```conf
   log_file = /var/log/sentinelfs/sentinel.log
   log_level = info
   ```

2. **Logrotate yapılandırması** (`/etc/logrotate.d/sentinelfs`):
   ```
   /var/log/sentinelfs/*.log {
       daily
       rotate 14
       compress
       delaycompress
       missingok
       notifempty
       create 0640 sentinelfs sentinelfs
       sharedscripts
       postrotate
           systemctl reload sentinel_daemon 2>/dev/null || true
       endscript
   }
   ```

3. **Manuel rotation testi**:
   ```bash
   sudo logrotate -d /etc/logrotate.d/sentinelfs  # Dry run
   sudo logrotate -f /etc/logrotate.d/sentinelfs  # Force rotate
   ```

### Log Analizi

#### Hata sayısı analizi
```bash
sudo journalctl -u sentinel_daemon --since today | grep -c ERROR
```

#### En sık görülen hatalar
```bash
sudo journalctl -u sentinel_daemon -p err --since "1 hour ago" | \
  awk '{print $NF}' | sort | uniq -c | sort -rn | head -10
```

#### Peer bağlantı istatistikleri
```bash
sudo journalctl -u sentinel_daemon --since today | \
  grep -E "(connected|disconnected)" | wc -l
```

---

## Troubleshooting

### Yaygın Sorunlar

#### Metrics endpoint erişilemiyor
```bash
# Port kontrolü
sudo ss -tlnp | grep 9100

# Firewall kontrolü
sudo ufw status
sudo iptables -L -n | grep 9100
```

#### Health check başarısız
```bash
# Detaylı kontrol
curl -v http://localhost:9100/healthz
curl -v http://localhost:9100/readyz

# Daemon loglarını kontrol
sudo journalctl -u sentinel_daemon -n 50 --no-pager
```

#### Yüksek bellek kullanımı
```bash
# Mevcut kullanım
curl -s http://localhost:9100/metrics | grep memory

# Process detayı
ps aux | grep sentinel_daemon
```

### Destek Bundle Oluşturma

Sorun bildirimi için gerekli bilgileri toplamak:

```bash
#!/bin/bash
BUNDLE_DIR="/tmp/sentinelfs-support-$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BUNDLE_DIR"

# Sistem bilgisi
uname -a > "$BUNDLE_DIR/system.txt"
cat /etc/os-release >> "$BUNDLE_DIR/system.txt"

# Daemon durumu
systemctl status sentinel_daemon > "$BUNDLE_DIR/status.txt" 2>&1

# Son loglar
journalctl -u sentinel_daemon --since "1 hour ago" > "$BUNDLE_DIR/logs.txt" 2>&1

# Metrikler
curl -s http://localhost:9100/metrics > "$BUNDLE_DIR/metrics.txt" 2>&1

# Config (hassas bilgiler çıkarılmış)
grep -v -E "(password|secret|key)" /etc/sentinelfs/sentinel.conf > "$BUNDLE_DIR/config.txt" 2>&1

# Arşivle
tar -czf "${BUNDLE_DIR}.tar.gz" -C /tmp "$(basename $BUNDLE_DIR)"
echo "Support bundle created: ${BUNDLE_DIR}.tar.gz"
```
