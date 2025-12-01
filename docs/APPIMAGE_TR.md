# SentinelFS AppImage - Kullanım Kılavuzu

## 🚀 Hızlı Başlangıç

### AppImage Oluşturma

Linux sisteminde:

```bash
# Projeyi klonlayın
git clone https://github.com/reicalasso/SentinelFS.git
cd SentinelFS

# AppImage'i oluşturun
./scripts/build_appimage.sh
```

### Çalıştırma

```bash
# Çalıştırılabilir yapın
chmod +x SentinelFS-*.AppImage

# Başlatın
./SentinelFS-*.AppImage
```

## 📦 İçerik

AppImage şunları içerir:

- **Electron GUI**: Modern web tabanlı arayüz
- **sentinel_daemon**: Ana senkronizasyon daemon'u
- **sentinel_cli**: Komut satırı aracı
- **Tüm pluginler**: filesystem, network, storage, ml
- **Konfigürasyon**: Varsayılan sentinel.conf

## 🔧 Nasıl Çalışır?

1. **AppImage başlatıldığında**:
   - Kendini geçici bir konuma mount eder
   - Electron GUI başlar

2. **GUI daemon'u başlatır**:
   - Socket üzerinden çalışan daemon'u kontrol eder
   - Yoksa daemon process'ini spawn eder
   - `SENTINELFS_PLUGIN_DIR` ile plugin dizinini ayarlar

3. **Daemon başlar**:
   - Unix socket ile IPC oluşturur
   - Pluginleri yükler
   - Sync engine'i başlatır

4. **GUI bağlanır**:
   - Daemon'a Unix socket üzerinden bağlanır
   - Gerçek zamanlı metrikleri gösterir
   - Komutları gönderir (sync başlat, config değiştir, vb.)

## 🎯 Özellikler

### Otomatik Daemon Yönetimi
- GUI açıldığında daemon otomatik başlar
- GUI kapandığında daemon arka planda çalışmaya devam eder
- Tekrar GUI açıldığında mevcut daemon'a bağlanır

### Taşınabilir
- Tek dosya, kurulum gerektirmez
- Tüm bağımlılıklar dahil
- Root yetkisi gerektirmez

### Konfigürasyon
Daemon config dosyasını şu sırayla arar:
1. `~/.config/sentinelfs/sentinel.conf` (kullanıcı config)
2. AppImage içindeki `resources/config/sentinel.conf` (varsayılan)

## 🐛 Sorun Giderme

### AppImage başlamıyor

```bash
# İzinleri kontrol edin
chmod +x SentinelFS-*.AppImage

# Debug modda çalıştırın
./SentinelFS-*.AppImage --verbose

# İçeriği çıkarın ve inceleyin
./SentinelFS-*.AppImage --appimage-extract
cd squashfs-root
```

### Daemon başlamıyor

```bash
# Log dosyasını kontrol edin
tail -f ~/.local/share/sentinelfs/logs/daemon.log

# Socket yolunu doğrulayın
ls -la /run/user/$(id -u)/sentinelfs/
# veya
ls -la /tmp/sentinelfs/
```

### Plugin yüklenmiyor

```bash
# AppImage içindeki pluginleri listeleyin
./SentinelFS-*.AppImage --appimage-extract
ls squashfs-root/resources/lib/plugins/
```

## 📋 Sistem Gereksinimleri

- Linux (Ubuntu 20.04+, Fedora 35+, Debian 11+)
- FUSE2 (genellikle önceden yüklü)
- 512 MB RAM (minimum)
- 100 MB disk alanı

FUSE yoksa:
```bash
# Ubuntu/Debian
sudo apt-get install fuse libfuse2

# Fedora/RHEL
sudo dnf install fuse fuse-libs
```

## 🏗️ Geliştirme

Değişiklikleri test etmek için:

```bash
# C++ bileşenlerini yeniden derleyin
cmake --build build_release --parallel

# GUI'yi ve AppImage'i yeniden oluşturun
cd gui
npm run build
```

## 🔄 Otomatik Build (GitHub Actions)

Her commit ve PR'de otomatik olarak AppImage oluşturulur:
- `.github/workflows/appimage.yml` workflow dosyasına bakın
- Artifacts sekmesinden indirebilirsiniz
- Tag'lerde (v1.0.0 gibi) otomatik release oluşturulur

## 📝 Notlar

### Avantajlar
✅ Tek dosya dağıtım  
✅ Bağımlılık sorunu yok  
✅ Sistem kurulumu gerektirmez  
✅ Çoklu versiyon yan yana çalışabilir  
✅ Kolay dağıtım (GitHub Releases, web sunucu)  

### Sınırlamalar
⚠️ FUSE gerektirir (modern Linux'larda var)  
⚠️ İlk başlatma 1-2 saniye sürer  
⚠️ Daemon crash'i socket dosyası bırakabilir  

## 🔗 Kaynaklar

- [AppImage Dökümantasyonu](https://docs.appimage.org/)
- [Electron Builder](https://www.electron.build/)
- [SentinelFS Mimari](../ARCHITECTURE.md)
- [İngilizce AppImage Guide](APPIMAGE.md)

## 🎬 Hızlı Demo

```bash
# 1. AppImage oluştur (sistem yüklemesi olmadan)
./scripts/build_appimage.sh

# Alternatif: Manuel CMake ile
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release -DSKIP_SYSTEM_INSTALL=ON
cmake --build build_release --target appimage

# 2. Çalıştır
./SentinelFS-*.AppImage

# 3. GUI'de:
#    - Sync klasörü ekle
#    - Session code oluştur
#    - Diğer peer'lara code'u paylaş
#    - Otomatik senkronizasyon başlar!
```

## 💡 İpuçları

- AppImage'i `/usr/local/bin` veya `~/bin` gibi PATH'teki dizine kopyalayın
- Desktop shortcut için: `.desktop` dosyasını `~/.local/share/applications/` dizinine kopyalayın
- Çoklu instance çalıştırmak için farklı config dizinleri kullanın

## 🆘 Destek

- Issues: https://github.com/reicalasso/SentinelFS/issues
- Discussions: https://github.com/reicalasso/SentinelFS/discussions
