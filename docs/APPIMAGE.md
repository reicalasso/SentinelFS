# SentinelFS AppImage Build Guide

**Version:** 1.0.0

---

## Overview

AppImage is a portable application format for Linux that bundles SentinelFS with all dependencies.

### Advantages

| Feature | Benefit |
|:--------|:--------|
| **Portability** | Run on any Linux distribution |
| **No Installation** | No root access required |
| **Self-Contained** | All dependencies bundled |

---

## Build Process

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install cmake build-essential nodejs npm
sudo apt-get install libfuse2-dev patchelf

# Download tools
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage appimagetool-x86_64.AppImage
```

### Build Steps
```bash
# 1. Build daemon
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
make -j$(nproc)
make install DESTDIR=../AppDir

# 2. Build GUI
cd ../gui
npm install && npm run build
npm run electron:build --linux --dir

# 3. Copy to AppDir
cp -r dist-linux*/* ../AppDir/

# 4. Create AppImage
../linuxdeploy-x86_64.AppImage --appdir ../AppDir
../appimagetool-x86_64.AppImage ../AppDir SentinelFS-1.0.0-x86_64.AppImage
```

---

## Distribution

### Upload to GitHub
```bash
# Create release
gh release create v1.0.0 SentinelFS-1.0.0-x86_64.AppImage \
  --title "SentinelFS v1.0.0" \
  --notes "Initial release"

# Generate checksum
sha256sum SentinelFS-1.0.0-x86_64.AppImage > SentinelFS-1.0.0-x86_64.AppImage.sha256
gh release upload v1.0.0 SentinelFS-1.0.0-x86_64.AppImage.sha256
```

---

## Usage

See [APPIMAGE_TR.md](APPIMAGE_TR.md) for user documentation.

---

*SentinelFS Build Team - December 2025*
