# SentinelFS Production Dağıtım Kılavuzu

**Versiyon:** 1.0.0

---

## 📋 Gereksinimler

### Sistem
| Bileşen | Minimum | Önerilen |
|:--------|:--------|:---------|
| CPU | 2 core | 4 core |
| RAM | 2 GB | 4 GB |
| Disk | 10 GB SSD | 50 GB SSD |
| Network | 100 Mbps | 1 Gbps |

### Yazılım
- Linux Kernel 5.4+
- OpenSSL 1.1.1+
- SQLite 3.35+
- CMake 3.15+

### Desteklenen Dağıtımlar
- Ubuntu 20.04+ LTS
- Debian 11+
- Fedora 38+
- RHEL 8+

---

## 🚀 Kurulum

### Paket Kurulumu
```bash
# Debian/Ubuntu
sudo dpkg -i sentinelfs-1.0.0-linux-x86_64.deb

# Fedora/RHEL
sudo rpm -i sentinelfs-1.0.0-linux-x86_64.rpm
```

### AppImage
```bash
chmod +x SentinelFS-1.0.0-x86_64.AppImage
./SentinelFS-1.0.0-x86_64.AppImage
```

### Manuel Kurulum
```bash
# Binary'leri kopyala
sudo install -m 755 sentinel_daemon /usr/bin/
sudo install -m 755 sentinel_cli /usr/bin/

# Dizinleri oluştur
sudo mkdir -p /etc/sentinelfs /var/lib/sentinelfs /var/log/sentinelfs
sudo useradd -r -s /sbin/nologin sentinelfs
sudo chown -R sentinelfs:sentinelfs /var/lib/sentinelfs
```

---

## ⚙️ Yapılandırma

### Ana Konfigürasyon
`/etc/sentinelfs/sentinel.conf`:

```ini
device_name = production-node-01
sync_folder = /data/sentinelfs
database_path = /var/lib/sentinelfs/sentinel.db
listen_port = 8082
discovery_port = 8083
metrics_port = 9100
session_code = <GÜÇLÜ_RASTGELE_KOD>
encryption_key = <32_BYTE_HEX_KEY>
hmac_key = <64_BYTE_HEX_KEY>
worker_threads = 0
chunk_size = 4096
max_concurrent_transfers = 10
log_level = INFO
log_path = /var/log/sentinelfs/sentinel.log
```

### Servis Yapılandırması
```bash
# Systemd service
sudo systemctl enable sentinelfs
sudo systemctl start sentinelfs
sudo systemctl status sentinelfs
```

---

## 🔒 Güvenlik

### Anahtar Üretimi
```bash
# Encryption key
openssl rand -hex 32 > /etc/sentinelfs/encryption.key
chmod 600 /etc/sentinelfs/encryption.key

# HMAC key
openssl rand -hex 64 > /etc/sentinelfs/hmac.key
chmod 600 /etc/sentinelfs/hmac.key
```

### Firewall
```bash
# UFW
sudo ufw allow 8082/tcp
sudo ufw allow 8083/udp
sudo ufw allow from 192.168.1.0/24 to any port 9100
```

---

## 🔄 Yedekleme

### Script
```bash
#!/bin/bash
BACKUP_DIR="/backup/sentinelfs"
DATE=$(date +%Y%m%d_%H%M%S)

# Database backup
sqlite3 /var/lib/sentinelfs/sentinel.db ".backup ${BACKUP_DIR}/sentinel_${DATE}.db"

# Config backup
tar -czf "${BACKUP_DIR}/config_${DATE}.tar.gz" /etc/sentinelfs/
```

### Cron
```cron
0 2 * * * /usr/local/bin/sentinelfs-backup.sh
```

---

## 📊 Performans

### Kernel Parametreleri
`/etc/sysctl.d/99-sentinelfs.conf`:

```ini
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
fs.file-max = 1000000
fs.inotify.max_user_watches = 524288
vm.swappiness = 10
```

---

## 🛠️ Sorun Giderme

### Daemon Başlamıyor
```bash
# Konfigürasyon kontrolü
sentinel_daemon --config /etc/sentinelfs/sentinel.conf --check

# Port kontrolü
ss -tlnp | grep -E '8082|8083|9100'
```

### Bağlantı Sorunları
```bash
# Network test
nc -zv peer-ip 8082

# Firewall kontrolü
sudo iptables -L -n | grep 8082
```

---

## 📁 Önemli Dosyalar

| Dosya | Konum |
|:------|:------|
| Daemon | `/usr/bin/sentinel_daemon` |
| Konfigürasyon | `/etc/sentinelfs/sentinel.conf` |
| Database | `/var/lib/sentinelfs/sentinel.db` |
| Log | `/var/log/sentinelfs/sentinel.log` |
| Service | `/etc/systemd/system/sentinelfs.service` |

---

*SentinelFS Operations Team - Aralık 2025*
