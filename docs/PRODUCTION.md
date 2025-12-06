# SentinelFS Production Deployment Kılavuzu

**Versiyon:** 1.0.0  
**Son Güncelleme:** Aralık 2025

---

## İçindekiler

1. [Genel Bakış](#1-genel-bakış)
2. [Sistem Gereksinimleri](#2-sistem-gereksinimleri)
3. [Kurulum](#3-kurulum)
4. [Yapılandırma](#4-yapılandırma)
5. [Güvenlik](#5-güvenlik)
6. [High Availability](#6-high-availability)
7. [Backup & Recovery](#7-backup--recovery)
8. [Performans Tuning](#8-performans-tuning)
9. [Sorun Giderme](#9-sorun-giderme)

---

## 1. Genel Bakış

Bu kılavuz, SentinelFS'in production ortamında güvenli ve verimli bir şekilde dağıtılması için gereken adımları içerir.

### Production Checklist

```
□ Sistem gereksinimleri karşılandı
□ Güvenlik sertlandırması yapıldı
□ TLS/SSL yapılandırıldı
□ Firewall kuralları oluşturuldu
□ Monitoring kuruldu
□ Backup stratejisi belirlendi
□ Logrotation yapılandırıldı
□ Resource limitleri ayarlandı
□ Health check'ler test edildi
□ Disaster recovery planı hazır
```

---

## 2. Sistem Gereksinimleri

### 2.1 Donanım Gereksinimleri

| Bileşen | Minimum | Önerilen | High-Performance |
|:--------|:--------|:---------|:-----------------|
| **CPU** | 2 core | 4 core | 8+ core |
| **RAM** | 2 GB | 4 GB | 8+ GB |
| **Disk** | 10 GB SSD | 50 GB SSD | 100+ GB NVMe |
| **Network** | 100 Mbps | 1 Gbps | 10 Gbps |

### 2.2 Yazılım Gereksinimleri

| Bileşen | Desteklenen Versiyon |
|:--------|:---------------------|
| **Linux Kernel** | 5.4+ |
| **glibc** | 2.31+ |
| **OpenSSL** | 1.1.1+ veya 3.0+ |
| **libsodium** | 1.0.18+ |
| **SQLite** | 3.35+ |

### 2.3 Desteklenen Dağıtımlar

| Dağıtım | Versiyon | Destek |
|:--------|:---------|:-------|
| Ubuntu | 20.04 LTS, 22.04 LTS, 24.04 LTS | ✅ |
| Debian | 11, 12 | ✅ |
| Fedora | 38, 39, 40 | ✅ |
| Arch Linux | Rolling | ✅ |
| CentOS Stream | 9 | ✅ |
| RHEL | 8, 9 | ✅ |

---

## 3. Kurulum

### 3.1 Paket Kurulumu (Önerilen)

```bash
# Debian/Ubuntu
sudo dpkg -i sentinelfs-1.0.0-linux-x86_64.deb

# Fedora/RHEL
sudo rpm -i sentinelfs-1.0.0-linux-x86_64.rpm

# Arch Linux
sudo pacman -U sentinelfs-1.0.0-linux-x86_64.pkg.tar.zst
```

### 3.2 AppImage Kurulumu

```bash
# AppImage indir ve çalıştırılabilir yap
chmod +x SentinelFS-1.0.0-x86_64.AppImage

# Desktop integration (isteğe bağlı)
./SentinelFS-1.0.0-x86_64.AppImage --appimage-extract
sudo mv squashfs-root /opt/sentinelfs
sudo ln -sf /opt/sentinelfs/AppRun /usr/local/bin/sentinel_daemon
```

### 3.3 Manual Kurulum

```bash
# Binary'leri kopyala
sudo install -m 755 sentinel_daemon /usr/bin/
sudo install -m 755 sentinel_cli /usr/bin/

# Yapılandırma dizinleri
sudo mkdir -p /etc/sentinelfs
sudo mkdir -p /var/lib/sentinelfs
sudo mkdir -p /var/log/sentinelfs
sudo mkdir -p /run/sentinelfs

# Kullanıcı oluştur
sudo useradd -r -s /sbin/nologin -d /var/lib/sentinelfs sentinelfs
sudo chown -R sentinelfs:sentinelfs /var/lib/sentinelfs /var/log/sentinelfs

# Systemd service
sudo install -m 644 sentinelfs.service /etc/systemd/system/
sudo systemctl daemon-reload
```

---

## 4. Yapılandırma

### 4.1 Ana Yapılandırma Dosyası

`/etc/sentinelfs/sentinel.conf`:

```ini
#=============================================================================
# SentinelFS Production Configuration
#=============================================================================

#-----------------------------------------------------------------------------
# Temel Ayarlar
#-----------------------------------------------------------------------------
device_name = production-node-01
sync_folder = /data/sentinelfs
database_path = /var/lib/sentinelfs/sentinel.db

#-----------------------------------------------------------------------------
# Network Ayarları
#-----------------------------------------------------------------------------
listen_port = 8082
# External IP otomatik algılanır, gerekirse override:
# external_ip = 203.0.113.100

# Peer discovery
discovery_port = 8083

# Metrics endpoint
metrics_port = 9100

#-----------------------------------------------------------------------------
# Security Ayarları
#-----------------------------------------------------------------------------
# Session code kimlik doğrulama için
session_code = <GÜÇLÜ_RASTGELE_KOD>

# AES-256-CBC encryption için key
encryption_key = <32_BYTE_HEX_KEY>

# HMAC-SHA256 integrity için key
hmac_key = <64_BYTE_HEX_KEY>

#-----------------------------------------------------------------------------
# Performance Ayarları
#-----------------------------------------------------------------------------
# Worker thread sayısı (0 = CPU core sayısı)
worker_threads = 0

# Delta sync chunk boyutu
chunk_size = 4096

# Maximum eşzamanlı transfer
max_concurrent_transfers = 10

# Transfer buffer boyutu
transfer_buffer_size = 65536

#-----------------------------------------------------------------------------
# Resource Limitleri
#-----------------------------------------------------------------------------
# Maximum bellek kullanımı (bytes)
max_memory = 536870912

# Maximum açık dosya sayısı
max_open_files = 1024

# Bandwidth limiti (bytes/sec, 0 = sınırsız)
bandwidth_limit = 0

#-----------------------------------------------------------------------------
# Logging
#-----------------------------------------------------------------------------
log_level = INFO
log_path = /var/log/sentinelfs/sentinel.log
```

### 4.2 Plugin Yapılandırması

`/etc/sentinelfs/plugins.conf`:

```ini
#-----------------------------------------------------------------------------
# Network Plugin
#-----------------------------------------------------------------------------
[Network]
enabled = true
max_connections = 50
connection_timeout = 30
reconnect_interval = 10
udp_discovery = true
tcp_keepalive = true

#-----------------------------------------------------------------------------
# Storage Plugin
#-----------------------------------------------------------------------------
[Storage]
enabled = true
database_journal_mode = WAL
database_synchronous = NORMAL
vacuum_interval = 86400

#-----------------------------------------------------------------------------
# Filesystem Plugin
#-----------------------------------------------------------------------------
[Filesystem]
enabled = true
inotify_buffer_size = 8192
debounce_interval = 100
exclude_patterns = *.tmp,*.swp,.git/*,node_modules/*

#-----------------------------------------------------------------------------
# ML Plugin
#-----------------------------------------------------------------------------
[ML]
enabled = true
model_path = /var/lib/sentinelfs/anomaly_model.onnx
inference_interval = 60
anomaly_threshold = 0.7
```

### 4.3 Ortam Değişkenleri

```bash
# Systemd environment file: /etc/sentinelfs/environment
SENTINELFS_CONFIG=/etc/sentinelfs/sentinel.conf
SENTINELFS_LOG_LEVEL=INFO
SENTINELFS_DATA_DIR=/var/lib/sentinelfs
```

---

## 5. Güvenlik

### 5.1 Key Generation

```bash
# Encryption key (256-bit)
openssl rand -hex 32 > /etc/sentinelfs/encryption.key
chmod 600 /etc/sentinelfs/encryption.key

# HMAC key (512-bit)
openssl rand -hex 64 > /etc/sentinelfs/hmac.key
chmod 600 /etc/sentinelfs/hmac.key

# Session code
openssl rand -base64 32 | tr -dc 'a-zA-Z0-9' | head -c 16
```

### 5.2 Firewall Kuralları

```bash
# UFW (Ubuntu/Debian)
sudo ufw allow 8082/tcp comment 'SentinelFS TCP'
sudo ufw allow 8083/udp comment 'SentinelFS Discovery'
sudo ufw allow from 192.168.1.0/24 to any port 9100 proto tcp comment 'SentinelFS Metrics'

# firewalld (RHEL/Fedora)
sudo firewall-cmd --permanent --add-port=8082/tcp
sudo firewall-cmd --permanent --add-port=8083/udp
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="192.168.1.0/24" port protocol="tcp" port="9100" accept'
sudo firewall-cmd --reload

# iptables
sudo iptables -A INPUT -p tcp --dport 8082 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 8083 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 9100 -s 192.168.1.0/24 -j ACCEPT
```

### 5.3 SELinux Policy (RHEL/Fedora)

```bash
# SELinux boolean
sudo setsebool -P sentinelfs_network_connect 1

# Custom policy module
sudo semodule -i sentinelfs.pp
```

### 5.4 AppArmor Profile (Debian/Ubuntu)

`/etc/apparmor.d/usr.bin.sentinel_daemon`:

```
#include <tunables/global>

/usr/bin/sentinel_daemon {
  #include <abstractions/base>
  #include <abstractions/nameservice>
  #include <abstractions/openssl>

  /usr/bin/sentinel_daemon mr,
  
  /etc/sentinelfs/ r,
  /etc/sentinelfs/** r,
  
  /var/lib/sentinelfs/ rw,
  /var/lib/sentinelfs/** rwk,
  
  /var/log/sentinelfs/ rw,
  /var/log/sentinelfs/** rw,
  
  /run/sentinelfs/ rw,
  /run/sentinelfs/** rw,
  
  # Sync folder - adjust path as needed
  /data/sentinelfs/ rw,
  /data/sentinelfs/** rwk,
  
  network inet stream,
  network inet dgram,
}
```

---

## 6. High Availability

### 6.1 Multi-Node Deployment

```
┌─────────────────────────────────────────────────────────────┐
│                    P2P Mesh Network                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │
│   │   Node 1    │────│   Node 2    │────│   Node 3    │     │
│   │  Primary    │    │   Backup    │    │   Backup    │     │
│   └──────┬──────┘    └──────┬──────┘    └──────┬──────┘     │
│          │                  │                  │            │
│          └──────────────────┼──────────────────┘            │
│                             │                               │
│                    Full Mesh Connectivity                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 Load Balancer Yapılandırması (HAProxy)

```
frontend sentinelfs_frontend
    bind *:8082
    mode tcp
    default_backend sentinelfs_backend

backend sentinelfs_backend
    mode tcp
    balance roundrobin
    option tcp-check
    server node1 192.168.1.101:8082 check
    server node2 192.168.1.102:8082 check
    server node3 192.168.1.103:8082 check
```

### 6.3 Relay Server Yapılandırması

NAT arkasındaki peer'lar için relay server:

```yaml
# docker-compose.yml
version: '3.8'
services:
  relay:
    image: sentinelfs/relay:1.0.0
    ports:
      - "8090:8090"
    environment:
      - RELAY_SECRET=<RELAY_SECRET>
      - MAX_CONNECTIONS=100
    restart: unless-stopped
```

---

## 7. Backup & Recovery

### 7.1 Backup Stratejisi

```bash
#!/bin/bash
# /usr/local/bin/sentinelfs-backup.sh

BACKUP_DIR="/backup/sentinelfs"
DATE=$(date +%Y%m%d_%H%M%S)

# Database backup
sqlite3 /var/lib/sentinelfs/sentinel.db ".backup ${BACKUP_DIR}/sentinel_${DATE}.db"

# Configuration backup
tar -czf "${BACKUP_DIR}/config_${DATE}.tar.gz" /etc/sentinelfs/

# Key backup (güvenli depolama gerekli!)
cp /etc/sentinelfs/*.key "${BACKUP_DIR}/keys_${DATE}/"
chmod 600 "${BACKUP_DIR}/keys_${DATE}/"*

# Eski backup'ları temizle (30 gün)
find "${BACKUP_DIR}" -name "*.db" -mtime +30 -delete
find "${BACKUP_DIR}" -name "*.tar.gz" -mtime +30 -delete
```

### 7.2 Crontab Yapılandırması

```cron
# Daily backup at 02:00
0 2 * * * /usr/local/bin/sentinelfs-backup.sh >> /var/log/sentinelfs-backup.log 2>&1

# Weekly integrity check on Sunday at 03:00
0 3 * * 0 sqlite3 /var/lib/sentinelfs/sentinel.db "PRAGMA integrity_check;" >> /var/log/sentinelfs-integrity.log 2>&1
```

### 7.3 Recovery Prosedürü

```bash
# 1. Servisi durdur
sudo systemctl stop sentinelfs

# 2. Database'i geri yükle
cp /backup/sentinelfs/sentinel_YYYYMMDD.db /var/lib/sentinelfs/sentinel.db
chown sentinelfs:sentinelfs /var/lib/sentinelfs/sentinel.db

# 3. Yapılandırmayı geri yükle
tar -xzf /backup/sentinelfs/config_YYYYMMDD.tar.gz -C /

# 4. Servisi başlat
sudo systemctl start sentinelfs

# 5. Durumu kontrol et
sudo systemctl status sentinelfs
sentinel_cli status
```

---

## 8. Performans Tuning

### 8.1 Kernel Parametreleri

`/etc/sysctl.d/99-sentinelfs.conf`:

```ini
# Network tuning
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
net.core.netdev_max_backlog = 5000
net.ipv4.tcp_rmem = 4096 87380 16777216
net.ipv4.tcp_wmem = 4096 65536 16777216
net.ipv4.tcp_congestion_control = bbr
net.core.default_qdisc = fq

# File descriptors
fs.file-max = 1000000
fs.inotify.max_user_watches = 524288
fs.inotify.max_user_instances = 512

# Memory
vm.swappiness = 10
vm.dirty_ratio = 60
vm.dirty_background_ratio = 2
```

```bash
# Uygula
sudo sysctl -p /etc/sysctl.d/99-sentinelfs.conf
```

### 8.2 File Descriptor Limitleri

`/etc/security/limits.d/sentinelfs.conf`:

```
sentinelfs soft nofile 65535
sentinelfs hard nofile 65535
sentinelfs soft nproc 4096
sentinelfs hard nproc 4096
```

### 8.3 I/O Scheduler

```bash
# NVMe için optimal scheduler
echo "none" | sudo tee /sys/block/nvme0n1/queue/scheduler

# SSD için
echo "mq-deadline" | sudo tee /sys/block/sda/queue/scheduler
```

### 8.4 Benchmark

```bash
# Transfer hızı testi
sentinel_cli benchmark transfer --size 1GB --iterations 5

# Delta sync testi
sentinel_cli benchmark delta --file /path/to/large/file

# Network latency testi
sentinel_cli benchmark latency --peer PEER_ID
```

---

## 9. Sorun Giderme

### 9.1 Yaygın Sorunlar

#### Daemon Başlamıyor

```bash
# Yapılandırma syntax kontrolü
sentinel_daemon --config /etc/sentinelfs/sentinel.conf --check

# Permission kontrolü
ls -la /var/lib/sentinelfs/
ls -la /etc/sentinelfs/

# Port kullanım kontrolü
ss -tlnp | grep -E '8082|8083|9100'

# SELinux/AppArmor kontrolü
sudo dmesg | grep -i denied
```

#### Peer Bağlantı Sorunu

```bash
# Network bağlantısı test et
nc -zv peer-ip 8082

# Firewall kontrolü
sudo iptables -L -n | grep 8082

# Discovery paketleri
sudo tcpdump -i any udp port 8083
```

#### Yüksek CPU Kullanımı

```bash
# Process analizi
top -H -p $(pgrep sentinel_daemon)

# Strace
strace -c -p $(pgrep sentinel_daemon) -e trace=file,network

# Sync queue kontrolü
sentinel_cli status --detailed
```

### 9.2 Debug Mode

```bash
# Verbose logging ile başlat
sentinel_daemon --config /etc/sentinelfs/sentinel.conf --log-level TRACE

# Core dump etkinleştir
ulimit -c unlimited
echo "/var/crash/core.%e.%p" | sudo tee /proc/sys/kernel/core_pattern
```

### 9.3 Log Analizi

```bash
# Hata sayısı
grep -c ERROR /var/log/sentinelfs/sentinel.log

# Son hatalar
grep ERROR /var/log/sentinelfs/sentinel.log | tail -20

# Bağlantı istatistikleri
grep -E "connected|disconnected" /var/log/sentinelfs/sentinel.log | tail -50

# Transfer istatistikleri
grep "transfer" /var/log/sentinelfs/sentinel.log | tail -50
```

---

## Ek: Hızlı Başvuru

### Önemli Dosya Konumları

| Dosya | Konum |
|:------|:------|
| Daemon binary | `/usr/bin/sentinel_daemon` |
| CLI binary | `/usr/bin/sentinel_cli` |
| Ana yapılandırma | `/etc/sentinelfs/sentinel.conf` |
| Plugin yapılandırması | `/etc/sentinelfs/plugins.conf` |
| Database | `/var/lib/sentinelfs/sentinel.db` |
| Log dosyası | `/var/log/sentinelfs/sentinel.log` |
| Systemd service | `/etc/systemd/system/sentinelfs.service` |

### Önemli Komutlar

```bash
# Servis yönetimi
sudo systemctl {start|stop|restart|status} sentinelfs

# Durum kontrolü
sentinel_cli status

# Peer listesi
sentinel_cli peers

# Manuel sync tetikle
sentinel_cli sync --force

# Health check
curl http://localhost:9100/healthz
curl http://localhost:9100/readyz
```

---

**Production Kılavuzu Sonu**

*SentinelFS Operations Team - Aralık 2025*
