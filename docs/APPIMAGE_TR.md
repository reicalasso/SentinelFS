# SentinelFS AppImage Kullanım Kılavuzu

**Versiyon:** 1.0.0  
**Son Güncelleme:** Aralık 2025

---

## İçindekiler

1. [AppImage Nedir?](#1-appimage-nedir)
2. [Kurulum](#2-kurulum)
3. [İlk Çalıştırma](#3-ilk-çalıştırma)
4. [Kullanım Modları](#4-kullanım-modları)
5. [Yapılandırma](#5-yapılandırma)
6. [Desktop Entegrasyonu](#6-desktop-entegrasyonu)
7. [Güncelleme](#7-güncelleme)
8. [Sorun Giderme](#8-sorun-giderme)

---

## 1. AppImage Nedir?

AppImage, Linux için taşınabilir bir uygulama formatıdır. SentinelFS AppImage, tüm bağımlılıkları içeren tek bir dosyadan oluşur ve herhangi bir Linux dağıtımında kurulum gerektirmeden çalışır.

### Avantajları

```
┌─────────────────────────────────────────────────────────────┐
│                   AppImage Avantajları                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ✓ Kurulum gerektirmez                                     │
│   ✓ Root yetkisi gerekmez                                   │
│   ✓ Tüm bağımlılıklar dahil                                 │
│   ✓ Her Linux dağıtımında çalışır                           │
│   ✓ Sistem kütüphanelerinden bağımsız                       │
│   ✓ Kolay güncelleme (dosyayı değiştir)                     │
│   ✓ Taşınabilir (USB'de çalışabilir)                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### İçerik

SentinelFS AppImage şunları içerir:

| Bileşen | Açıklama |
|:--------|:---------|
| **sentinel_daemon** | Ana senkronizasyon daemon'u |
| **sentinel_cli** | Komut satırı aracı |
| **sentinelfs-gui** | Electron tabanlı grafiksel arayüz |
| **Plugins** | Network, Storage, Filesystem, ML eklentileri |
| **Kütüphaneler** | OpenSSL, SQLite, libsodium, ONNX Runtime |

---

## 2. Kurulum

### 2.1 İndirme

```bash
# GitHub Releases'tan indir
wget https://github.com/reicalasso/SentinelFS/releases/download/v1.0.0/SentinelFS-1.0.0-x86_64.AppImage

# Checksum doğrula (önerilen)
wget https://github.com/reicalasso/SentinelFS/releases/download/v1.0.0/SentinelFS-1.0.0-x86_64.AppImage.sha256
sha256sum -c SentinelFS-1.0.0-x86_64.AppImage.sha256
```

### 2.2 Çalıştırılabilir Yapma

```bash
# Çalıştırma iznini ver
chmod +x SentinelFS-1.0.0-x86_64.AppImage
```

### 2.3 FUSE Gereksinimi

AppImage, FUSE (Filesystem in Userspace) gerektirir:

```bash
# Ubuntu/Debian
sudo apt-get install fuse libfuse2

# Fedora
sudo dnf install fuse fuse-libs

# Arch Linux
sudo pacman -S fuse2

# FUSE olmadan çalıştırma (alternatif)
./SentinelFS-1.0.0-x86_64.AppImage --appimage-extract
./squashfs-root/AppRun
```

---

## 3. İlk Çalıştırma

### 3.1 GUI ile Başlatma (Varsayılan)

```bash
# Grafiksel arayüzü başlat
./SentinelFS-1.0.0-x86_64.AppImage
```

GUI açıldığında:
1. **Session Code** girin (diğer peer'larla aynı olmalı)
2. **Sync Folder** seçin
3. **Start** butonuna tıklayın

### 3.2 Daemon ile Başlatma

```bash
# Daemon modunda başlat
./SentinelFS-1.0.0-x86_64.AppImage daemon --config ~/sentinel.conf

# Arka planda çalıştır
./SentinelFS-1.0.0-x86_64.AppImage daemon --config ~/sentinel.conf &
```

### 3.3 CLI ile Durum Kontrolü

```bash
# Daemon durumunu kontrol et
./SentinelFS-1.0.0-x86_64.AppImage cli status

# Bağlı peer'ları listele
./SentinelFS-1.0.0-x86_64.AppImage cli peers

# Yardım
./SentinelFS-1.0.0-x86_64.AppImage cli --help
```

---

## 4. Kullanım Modları

### 4.1 Komut Formatı

```bash
./SentinelFS-1.0.0-x86_64.AppImage [mod] [parametreler]
```

### 4.2 Modlar

| Mod | Komut | Açıklama |
|:----|:------|:---------|
| **GUI** | `./SentinelFS.AppImage` | Grafiksel arayüz (varsayılan) |
| **GUI** | `./SentinelFS.AppImage gui` | Grafiksel arayüz |
| **Daemon** | `./SentinelFS.AppImage daemon` | Senkronizasyon daemon'u |
| **CLI** | `./SentinelFS.AppImage cli` | Komut satırı aracı |

### 4.3 Daemon Parametreleri

```bash
./SentinelFS.AppImage daemon [parametreler]

# Parametreler:
  --config <path>      Yapılandırma dosyası
  --log-level <level>  Log seviyesi (TRACE/DEBUG/INFO/WARNING/ERROR)
  --foreground         Ön planda çalış (debug için)
  --check              Yapılandırmayı kontrol et ve çık
```

### 4.4 CLI Komutları

```bash
# Durum
./SentinelFS.AppImage cli status

# Peer yönetimi
./SentinelFS.AppImage cli peers
./SentinelFS.AppImage cli peers add <ip>:<port>
./SentinelFS.AppImage cli peers remove <peer_id>

# Senkronizasyon
./SentinelFS.AppImage cli sync --force
./SentinelFS.AppImage cli sync --status

# Güvenlik
./SentinelFS.AppImage cli session-code <code>
./SentinelFS.AppImage cli regenerate-keys

# Sistem
./SentinelFS.AppImage cli health
./SentinelFS.AppImage cli version
```

---

## 5. Yapılandırma

### 5.1 Yapılandırma Dosyası Oluşturma

```bash
# Şablon yapılandırma dosyasını çıkar
./SentinelFS.AppImage --appimage-extract
cp squashfs-root/usr/share/sentinelfs/config/sentinel.conf.template ~/sentinel.conf

# Veya manuel oluştur
cat > ~/sentinel.conf << 'EOF'
# SentinelFS Yapılandırması
device_name = my-laptop
sync_folder = ~/SentinelFS
database_path = ~/.sentinelfs/sentinel.db
listen_port = 8082
discovery_port = 8083
metrics_port = 9100
session_code = MY_SECRET_CODE
log_level = INFO
EOF
```

### 5.2 Yapılandırma Parametreleri

| Parametre | Varsayılan | Açıklama |
|:----------|:-----------|:---------|
| `device_name` | hostname | Cihaz adı |
| `sync_folder` | ~/SentinelFS | Senkronizasyon klasörü |
| `database_path` | ~/.sentinelfs/sentinel.db | Veritabanı konumu |
| `listen_port` | 8082 | TCP dinleme portu |
| `discovery_port` | 8083 | UDP keşif portu |
| `metrics_port` | 9100 | Prometheus metrikleri |
| `session_code` | (zorunlu) | Kimlik doğrulama kodu |
| `encryption_key` | (otomatik) | AES-256 şifreleme anahtarı |
| `hmac_key` | (otomatik) | HMAC-SHA256 anahtarı |
| `log_level` | INFO | Log seviyesi |

### 5.3 Veri Dizinleri

AppImage şu dizinleri kullanır:

| Dizin | İçerik |
|:------|:-------|
| `~/.sentinelfs/` | Veritabanı ve yapılandırma |
| `~/.sentinelfs/logs/` | Log dosyaları |
| `~/.config/sentinelfs/` | GUI ayarları |
| `~/SentinelFS/` | Varsayılan sync klasörü |

---

## 6. Desktop Entegrasyonu

### 6.1 Otomatik Entegrasyon

```bash
# Desktop dosyası oluştur
./SentinelFS.AppImage --appimage-extract
cp squashfs-root/sentinelfs.desktop ~/.local/share/applications/
cp squashfs-root/sentinelfs.png ~/.local/share/icons/

# Veya AppImageLauncher kullan
# https://github.com/TheAssassin/AppImageLauncher
```

### 6.2 PATH'e Ekleme

```bash
# Symlink oluştur
sudo ln -sf /path/to/SentinelFS-1.0.0-x86_64.AppImage /usr/local/bin/sentinelfs

# Artık şu şekilde çalıştırabilirsiniz:
sentinelfs daemon --config ~/sentinel.conf
sentinelfs cli status
```

### 6.3 Systemd User Service

```bash
# ~/.config/systemd/user/sentinelfs.service
mkdir -p ~/.config/systemd/user

cat > ~/.config/systemd/user/sentinelfs.service << 'EOF'
[Unit]
Description=SentinelFS P2P Sync
After=network-online.target

[Service]
Type=simple
ExecStart=/path/to/SentinelFS.AppImage daemon --config %h/sentinel.conf
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
EOF

# Etkinleştir ve başlat
systemctl --user daemon-reload
systemctl --user enable sentinelfs
systemctl --user start sentinelfs

# Durum kontrolü
systemctl --user status sentinelfs
```

### 6.4 Autostart (GUI)

```bash
# ~/.config/autostart/sentinelfs.desktop
mkdir -p ~/.config/autostart

cat > ~/.config/autostart/sentinelfs.desktop << 'EOF'
[Desktop Entry]
Type=Application
Name=SentinelFS
Exec=/path/to/SentinelFS.AppImage
Icon=sentinelfs
X-GNOME-Autostart-enabled=true
EOF
```

---

## 7. Güncelleme

### 7.1 Manuel Güncelleme

```bash
# Yeni versiyon indir
wget https://github.com/reicalasso/SentinelFS/releases/download/v1.1.0/SentinelFS-1.1.0-x86_64.AppImage

# Eski dosyayı değiştir
chmod +x SentinelFS-1.1.0-x86_64.AppImage
mv SentinelFS-1.1.0-x86_64.AppImage /path/to/SentinelFS.AppImage

# Çalışan daemon'u yeniden başlat
systemctl --user restart sentinelfs
```

### 7.2 Versiyon Kontrolü

```bash
# Mevcut versiyon
./SentinelFS.AppImage --version

# GitHub'da yeni versiyon kontrolü
curl -s https://api.github.com/repos/reicalasso/SentinelFS/releases/latest | grep tag_name
```

---

## 8. Sorun Giderme

### 8.1 Yaygın Hatalar

#### "Cannot mount AppImage"

```bash
# Neden: FUSE yüklü değil
# Çözüm 1: FUSE yükle
sudo apt-get install fuse libfuse2

# Çözüm 2: Extract ederek çalıştır
./SentinelFS.AppImage --appimage-extract
./squashfs-root/AppRun daemon --config ~/sentinel.conf
```

#### "Permission denied"

```bash
# Neden: Çalıştırma izni yok
chmod +x SentinelFS-1.0.0-x86_64.AppImage

# Neden: Dosya sistemi noexec mount edilmiş
# (tmpfs, bazı USB diskler)
# Çözüm: Başka bir konuma taşı
cp SentinelFS.AppImage ~/SentinelFS.AppImage
```

#### "Segmentation fault" veya crash

```bash
# Debug modunda çalıştır
SENTINELFS_DEBUG=1 ./SentinelFS.AppImage daemon --log-level TRACE

# Core dump etkinleştir
ulimit -c unlimited
./SentinelFS.AppImage daemon --config ~/sentinel.conf
# Crash sonrası: gdb ./squashfs-root/usr/bin/sentinel_daemon core
```

#### "Unable to create sandbox" (GUI)

```bash
# Electron sandbox hatası
# Çözüm: --no-sandbox flag'i
./SentinelFS.AppImage gui --no-sandbox
```

### 8.2 Log Kontrolü

```bash
# GUI logları
cat ~/.config/sentinelfs/logs/gui.log

# Daemon logları (systemd user service)
journalctl --user -u sentinelfs -f

# Daemon logları (manuel çalıştırma)
cat ~/.sentinelfs/logs/sentinel.log
```

### 8.3 Bağlantı Sorunları

```bash
# Firewall kontrolü
sudo iptables -L -n | grep -E '8082|8083'

# Port kullanım kontrolü
ss -tlnp | grep sentinel

# Peer keşif testi
./SentinelFS.AppImage cli peers discover

# Manuel peer ekleme
./SentinelFS.AppImage cli peers add 192.168.1.100:8082
```

### 8.4 Performans Sorunları

```bash
# CPU kullanımı
top -p $(pgrep -f sentinel_daemon)

# Bellek kullanımı
ps aux | grep sentinel_daemon

# I/O beklemeleri
iotop -p $(pgrep -f sentinel_daemon)

# Sync queue durumu
./SentinelFS.AppImage cli sync --status
```

---

## Ek: Hızlı Başvuru Kartı

### Temel Komutlar

```bash
# GUI başlat
./SentinelFS.AppImage

# Daemon başlat
./SentinelFS.AppImage daemon --config ~/sentinel.conf

# Durum kontrol
./SentinelFS.AppImage cli status

# Peer listele
./SentinelFS.AppImage cli peers

# Yardım
./SentinelFS.AppImage --help
./SentinelFS.AppImage daemon --help
./SentinelFS.AppImage cli --help
```

### Faydalı Alias'lar (~/.bashrc veya ~/.zshrc)

```bash
alias sfs='./path/to/SentinelFS.AppImage'
alias sfs-daemon='sfs daemon --config ~/sentinel.conf'
alias sfs-status='sfs cli status'
alias sfs-peers='sfs cli peers'
```

---

**AppImage Kullanım Kılavuzu Sonu**

*SentinelFS Team - Aralık 2025*
