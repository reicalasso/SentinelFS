# SentinelFS İzleme ve Operasyon Kılavuzu

**Versiyon:** 1.0.0  
**Son Güncelleme:** Aralık 2025

---

## İçindekiler

1. [Genel Bakış](#1-genel-bakış)
2. [Prometheus Metrikleri](#2-prometheus-metrikleri)
3. [Grafana Dashboard](#3-grafana-dashboard)
4. [Health-Check Endpoint'leri](#4-health-check-endpointleri)
5. [Systemd Yapılandırması](#5-systemd-yapılandırması)
6. [Log Yönetimi](#6-log-yönetimi)
7. [Alerting Kuralları](#7-alerting-kuralları)

---

## 1. Genel Bakış

SentinelFS, production ortamları için kapsamlı izleme altyapısı sunar:

```
┌─────────────────────────────────────────────────────────────────┐
│                    İzleme Mimarisi                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────────┐    ┌─────────────────┐                    │
│   │  SentinelFS     │    │  SentinelFS     │                    │
│   │  Daemon 1       │    │  Daemon 2       │                    │
│   │  :9100/metrics  │    │  :9100/metrics  │                    │
│   └────────┬────────┘    └────────┬────────┘                    │
│            │                      │                             │
│            └──────────┬───────────┘                             │
│                       │                                         │
│            ┌──────────▼──────────┐                              │
│            │     Prometheus      │                              │
│            │   (Metric Storage)  │                              │
│            └──────────┬──────────┘                              │
│                       │                                         │
│            ┌──────────▼──────────┐                              │
│            │      Grafana        │                              │
│            │   (Visualization)   │                              │
│            └─────────────────────┘                              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Özellikler

- **Prometheus-uyumlu** metrik endpoint'i
- **Kubernetes-uyumlu** health check'ler
- **Grafana dashboard** hazır şablonları
- **Systemd integration** ile servis yönetimi
- **Structured logging** ile log analizi

---

## 2. Prometheus Metrikleri

### 2.1 Endpoint Bilgileri

| Özellik | Değer |
|:--------|:------|
| **URL** | `http://<host>:<metrics_port>/metrics` |
| **Varsayılan Port** | `9100` |
| **Format** | Prometheus text exposition (v0.0.4) |
| **Güncelleme** | Real-time |

### 2.2 Metrik Kategorileri

#### Daemon Bilgileri

| Metrik | Tip | Açıklama |
|:-------|:----|:---------|
| `sentinelfs_info` | gauge | Daemon versiyon bilgisi (label: version) |
| `sentinelfs_uptime_seconds` | counter | Daemon çalışma süresi (saniye) |
| `sentinelfs_start_time_seconds` | gauge | Daemon başlangıç timestamp'i |

#### Senkronizasyon Metrikleri

| Metrik | Tip | Açıklama |
|:-------|:----|:---------|
| `sentinelfs_files_watched_total` | gauge | İzlenen dosya sayısı |
| `sentinelfs_files_synced_total` | counter | Toplam senkronize edilen dosya |
| `sentinelfs_files_modified_total` | counter | Algılanan dosya değişikliği |
| `sentinelfs_files_deleted_total` | counter | Algılanan dosya silme |
| `sentinelfs_sync_errors_total` | counter | Senkronizasyon hataları |
| `sentinelfs_conflicts_detected_total` | counter | Algılanan çakışmalar |
| `sentinelfs_sync_queue_length` | gauge | Bekleyen sync işlemi sayısı |

#### Network Metrikleri

| Metrik | Tip | Açıklama |
|:-------|:----|:---------|
| `sentinelfs_bytes_uploaded_total` | counter | Toplam upload (byte) |
| `sentinelfs_bytes_downloaded_total` | counter | Toplam download (byte) |
| `sentinelfs_peers_discovered_total` | counter | Keşfedilen peer sayısı |
| `sentinelfs_peers_connected` | gauge | Bağlı peer sayısı |
| `sentinelfs_peers_disconnected_total` | counter | Kopan bağlantı sayısı |
| `sentinelfs_transfers_completed_total` | counter | Başarılı transfer |
| `sentinelfs_transfers_failed_total` | counter | Başarısız transfer |
| `sentinelfs_active_transfers` | gauge | Aktif transfer sayısı |

#### Delta Sync Metrikleri

| Metrik | Tip | Açıklama |
|:-------|:----|:---------|
| `sentinelfs_deltas_sent_total` | counter | Gönderilen delta sayısı |
| `sentinelfs_deltas_received_total` | counter | Alınan delta sayısı |
| `sentinelfs_delta_bytes_saved_total` | counter | Delta ile tasarruf (byte) |
| `sentinelfs_full_transfers_total` | counter | Tam dosya transferi sayısı |

#### Güvenlik Metrikleri

| Metrik | Tip | Açıklama |
|:-------|:----|:---------|
| `sentinelfs_anomalies_detected_total` | counter | ML ile algılanan anomali |
| `sentinelfs_suspicious_activities_total` | counter | Şüpheli aktivite |
| `sentinelfs_sync_paused_total` | counter | Güvenlik duraklatması |
| `sentinelfs_auth_failures_total` | counter | Kimlik doğrulama hatası |
| `sentinelfs_encryption_errors_total` | counter | Şifreleme hatası |

#### Performans Metrikleri

| Metrik | Tip | Açıklama |
|:-------|:----|:---------|
| `sentinelfs_sync_latency_ms` | gauge | Ortalama sync gecikmesi |
| `sentinelfs_delta_compute_time_ms` | gauge | Delta hesaplama süresi |
| `sentinelfs_transfer_speed_kbps` | gauge | Transfer hızı (KB/s) |
| `sentinelfs_memory_usage_bytes` | gauge | Bellek kullanımı |
| `sentinelfs_cpu_usage_percent` | gauge | CPU kullanımı |

#### Auto-Remesh Metrikleri

| Metrik | Tip | Açıklama |
|:-------|:----|:---------|
| `sentinelfs_remesh_cycles_total` | counter | Remesh döngü sayısı |
| `sentinelfs_remesh_rtt_improvement_ms` | gauge | RTT iyileştirmesi |
| `sentinelfs_network_partitions_detected` | counter | Ağ bölünmesi tespiti |

### 2.3 Prometheus Yapılandırması

`prometheus.yml` dosyasına ekleyin:

```yaml
scrape_configs:
  - job_name: 'sentinelfs'
    static_configs:
      - targets: ['localhost:9100']
    scrape_interval: 15s
    scrape_timeout: 10s
    metrics_path: /metrics
```

#### Çoklu Instance Yapılandırması

```yaml
scrape_configs:
  - job_name: 'sentinelfs'
    static_configs:
      - targets:
          - 'node1.local:9100'
          - 'node2.local:9100'
          - 'node3.local:9100'
        labels:
          cluster: 'home-network'
          environment: 'production'
    relabel_configs:
      - source_labels: [__address__]
        target_label: instance
        regex: '([^:]+):\d+'
        replacement: '${1}'
```

---

## 3. Grafana Dashboard

### 3.1 Dashboard Kurulumu

1. Grafana'yı açın → **Dashboards** → **Import**
2. `docs/grafana/sentinelfs-dashboard.json` dosyasını yükleyin
3. Prometheus data source'u seçin
4. **Import** butonuna tıklayın

### 3.2 Dashboard Panelleri

#### Overview Row

| Panel | Açıklama |
|:------|:---------|
| **Uptime** | Daemon çalışma süresi |
| **Connected Peers** | Aktif peer bağlantıları |
| **Files Watched** | İzlenen dosya sayısı |
| **Active Transfers** | Devam eden transferler |
| **Sync Latency** | Ortalama gecikme (ms) |
| **Error Rate** | Hata oranı |

#### Sync Activity Row

| Panel | Görselleştirme |
|:------|:---------------|
| **File Operations Rate** | Time series (sync/modify/delete per minute) |
| **Transfer Success Rate** | Gauge (başarı yüzdesi) |
| **Sync Queue Length** | Time series |

#### Network Row

| Panel | Görselleştirme |
|:------|:---------------|
| **Network Throughput** | Time series (upload/download MB/s) |
| **Peer Connections** | Time series (connected/discovered) |
| **Transfer Latency** | Heatmap |

#### Delta Sync Row

| Panel | Görselleştirme |
|:------|:---------------|
| **Delta vs Full Transfers** | Pie chart |
| **Bandwidth Saved** | Stat panel |
| **Delta Compute Time** | Time series |

#### Security Row

| Panel | Görselleştirme |
|:------|:---------------|
| **Security Events** | Time series (anomaly/suspicious/auth) |
| **Encryption Errors** | Counter |
| **Sync Pauses** | Event log |

### 3.3 Dashboard Değişkenleri

Dashboard'da kullanılan template değişkenleri:

| Değişken | Query | Açıklama |
|:---------|:------|:---------|
| `$instance` | `label_values(sentinelfs_info, instance)` | Daemon instance seçimi |
| `$interval` | Auto | Grafik granularity |

---

## 4. Health-Check Endpoint'leri

### 4.1 Liveness Probe (`/healthz`)

Daemon process'inin hayatta olduğunu kontrol eder.

```bash
# Test komutu
curl -s http://localhost:9100/healthz

# Başarılı yanıt
HTTP 200 OK
Body: "ok"

# Başarısız yanıt
HTTP 503 Service Unavailable
Body: "unhealthy"
```

**Kontrol Ettikleri:**
- Main loop çalışıyor mu
- Critical thread'ler aktif mi

**Kubernetes Yapılandırması:**

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

### 4.2 Readiness Probe (`/readyz`)

Daemon'un trafik almaya hazır olduğunu kontrol eder.

```bash
# Test komutu
curl -s http://localhost:9100/readyz

# Başarılı yanıt
HTTP 200 OK
Body: "ready"

# Başarısız yanıt
HTTP 503 Service Unavailable
Body: "not ready: database connection failed"
```

**Kontrol Ettikleri:**
- Daemon running
- Network plugin aktif
- Storage plugin aktif
- Database erişilebilir
- File watcher aktif

**Kubernetes Yapılandırması:**

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

### 4.3 Startup Probe

Yavaş başlayan ortamlar için:

```yaml
startupProbe:
  httpGet:
    path: /healthz
    port: 9100
  initialDelaySeconds: 5
  periodSeconds: 5
  failureThreshold: 30  # 150s max startup time
```

---

## 5. Systemd Yapılandırması

### 5.1 Service Dosyası

`/etc/systemd/system/sentinelfs.service`:

```ini
[Unit]
Description=SentinelFS P2P File Synchronization Daemon
Documentation=https://github.com/reicalasso/SentinelFS
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=sentinelfs
Group=sentinelfs

# Execution
ExecStart=/usr/bin/sentinel_daemon --config /etc/sentinelfs/sentinel.conf
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5
TimeoutStopSec=30

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=sentinelfs

# Security Hardening
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=read-only
PrivateTmp=true
PrivateDevices=true
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectControlGroups=true
RestrictRealtime=true
RestrictSUIDSGID=true
MemoryDenyWriteExecute=true
LockPersonality=true

# Paths
ReadWritePaths=/var/lib/sentinelfs /var/log/sentinelfs /run/sentinelfs
RuntimeDirectory=sentinelfs
StateDirectory=sentinelfs
LogsDirectory=sentinelfs

# Resources
MemoryMax=512M
CPUQuota=50%

[Install]
WantedBy=multi-user.target
```

### 5.2 Servis Yönetimi

```bash
# Servisi etkinleştir
sudo systemctl enable sentinelfs

# Servisi başlat
sudo systemctl start sentinelfs

# Durumu kontrol et
sudo systemctl status sentinelfs

# Logları görüntüle
sudo journalctl -u sentinelfs -f

# Yeniden başlat
sudo systemctl restart sentinelfs

# Durdur
sudo systemctl stop sentinelfs
```

### 5.3 Kullanıcı Oluşturma

```bash
# Sistem kullanıcısı oluştur
sudo useradd -r -s /sbin/nologin -d /var/lib/sentinelfs sentinelfs

# Dizinleri oluştur
sudo mkdir -p /var/lib/sentinelfs /var/log/sentinelfs /etc/sentinelfs
sudo chown sentinelfs:sentinelfs /var/lib/sentinelfs /var/log/sentinelfs
sudo chmod 750 /var/lib/sentinelfs /var/log/sentinelfs
```

---

## 6. Log Yönetimi

### 6.1 Log Formatı

SentinelFS structured JSON logging kullanır:

```json
{
  "timestamp": "2025-12-05T14:32:18.456Z",
  "level": "INFO",
  "module": "NetworkPlugin",
  "message": "Peer connected",
  "peer_id": "PEER_82844",
  "ip": "192.168.1.105",
  "port": 8082
}
```

### 6.2 Log Seviyeleri

| Seviye | Kullanım |
|:-------|:---------|
| `TRACE` | Detaylı debug bilgisi |
| `DEBUG` | Debug bilgisi |
| `INFO` | Normal operasyon |
| `WARNING` | Potansiyel sorun |
| `ERROR` | Hata durumu |
| `FATAL` | Kritik hata |

### 6.3 Log Rotation

`/etc/logrotate.d/sentinelfs`:

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
        systemctl reload sentinelfs > /dev/null 2>&1 || true
    endscript
}
```

### 6.4 Journal Filtreleme

```bash
# Son 100 satır
journalctl -u sentinelfs -n 100

# Bugünkü loglar
journalctl -u sentinelfs --since today

# Sadece hatalar
journalctl -u sentinelfs -p err

# JSON formatında export
journalctl -u sentinelfs -o json > sentinelfs-logs.json

# Follow mode
journalctl -u sentinelfs -f
```

---

## 7. Alerting Kuralları

### 7.1 Prometheus Alert Rules

`/etc/prometheus/rules/sentinelfs.yml`:

```yaml
groups:
  - name: sentinelfs
    interval: 30s
    rules:
      # High Error Rate
      - alert: SentinelFSHighErrorRate
        expr: rate(sentinelfs_sync_errors_total[5m]) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High sync error rate detected"
          description: "Error rate is {{ $value | printf \"%.2f\" }} errors/sec on {{ $labels.instance }}"

      # No Connected Peers
      - alert: SentinelFSNoPeers
        expr: sentinelfs_peers_connected == 0
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "No peers connected"
          description: "SentinelFS on {{ $labels.instance }} has no connected peers for 5 minutes"

      # High Memory Usage
      - alert: SentinelFSHighMemory
        expr: sentinelfs_memory_usage_bytes > 536870912  # 512MB
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "High memory usage"
          description: "Memory usage is {{ $value | humanize1024 }} on {{ $labels.instance }}"

      # Daemon Down
      - alert: SentinelFSDown
        expr: up{job="sentinelfs"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "SentinelFS daemon is down"
          description: "SentinelFS on {{ $labels.instance }} is not responding"

      # High Latency
      - alert: SentinelFSHighLatency
        expr: sentinelfs_sync_latency_ms > 500
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High sync latency"
          description: "Sync latency is {{ $value }}ms on {{ $labels.instance }}"

      # Anomaly Detected
      - alert: SentinelFSAnomalyDetected
        expr: increase(sentinelfs_anomalies_detected_total[5m]) > 0
        labels:
          severity: critical
        annotations:
          summary: "Security anomaly detected"
          description: "ML anomaly detection triggered on {{ $labels.instance }}"

      # Auth Failures
      - alert: SentinelFSAuthFailures
        expr: increase(sentinelfs_auth_failures_total[5m]) > 5
        labels:
          severity: warning
        annotations:
          summary: "Multiple authentication failures"
          description: "{{ $value }} auth failures in last 5 minutes on {{ $labels.instance }}"
```

### 7.2 Grafana Alert Rules

Grafana UI üzerinden alert oluşturmak için:

1. Dashboard'a gidin
2. Panel'e tıklayın → **Edit**
3. **Alert** tab'ına geçin
4. **Create alert rule from this panel**
5. Koşulları belirleyin
6. Notification channel seçin

---

## Ek: Sorun Giderme

### Metrikler Görünmüyor

```bash
# Endpoint erişilebilirliğini kontrol et
curl -v http://localhost:9100/metrics

# Firewall kurallarını kontrol et
sudo iptables -L -n | grep 9100

# Port kullanımını kontrol et
sudo ss -tlnp | grep 9100
```

### Health Check Başarısız

```bash
# Detaylı health status
curl -s http://localhost:9100/readyz | jq .

# Daemon loglarını kontrol et
journalctl -u sentinelfs -n 50

# Plugin durumlarını kontrol et
sentinel_cli status
```

### Yüksek Kaynak Kullanımı

```bash
# CPU/Memory analizi
top -p $(pgrep sentinel_daemon)

# Strace ile syscall analizi
strace -p $(pgrep sentinel_daemon) -c

# Peer bağlantılarını kontrol et
sentinel_cli peers
```

---

**İzleme Kılavuzu Sonu**

*SentinelFS Operations Team - Aralık 2025*
