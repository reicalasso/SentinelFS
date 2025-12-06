# SentinelFS AppImage Build Guide

**Version:** 1.0.0  
**Last Updated:** December 2025

---

## Table of Contents

1. [Overview](#1-overview)
2. [Prerequisites](#2-prerequisites)
3. [Build Process](#3-build-process)
4. [AppImage Structure](#4-appimage-structure)
5. [Distribution](#5-distribution)
6. [Usage](#6-usage)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. Overview

AppImage is a portable application format that allows SentinelFS to run on any Linux distribution without installation. A single `.AppImage` file contains the daemon, CLI tools, GUI, and all dependencies.

### Advantages

| Feature | Benefit |
|:--------|:--------|
| **Portability** | Run on any Linux distribution |
| **No Installation** | No root access required |
| **Self-Contained** | All dependencies bundled |
| **Sandboxed** | Isolated from system libraries |
| **Easy Updates** | Simply replace the file |

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│              SentinelFS AppImage Structure                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                    AppRun                           │   │
│   │            (Entry point script)                     │   │
│   └─────────────────────────────────────────────────────┘   │
│                           │                                 │
│   ┌───────────────────────┼───────────────────────────┐     │
│   │                       ▼                           │     │
│   │   ┌─────────────┐ ┌─────────────┐ ┌────────────┐  │     │
│   │   │   daemon    │ │    cli      │ │    gui     │  │     │
│   │   │  (binary)   │ │  (binary)   │ │ (electron) │  │     │
│   │   └─────────────┘ └─────────────┘ └────────────┘  │     │
│   │                                                   │     │
│   │   ┌───────────────────────────────────────────┐   │     │
│   │   │              Libraries                    │   │     │
│   │   │  libssl, libsqlite3, libonnxruntime...   │   │     │
│   │   └───────────────────────────────────────────┘   │     │
│   │                                                   │     │
│   │   ┌───────────────────────────────────────────┐   │     │
│   │   │           Plugins (*.so)                  │   │     │
│   │   │  network, storage, filesystem, ml        │   │     │
│   │   └───────────────────────────────────────────┘   │     │
│   └───────────────────────────────────────────────────┘     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Prerequisites

### 2.1 Build Requirements

| Tool | Version | Purpose |
|:-----|:--------|:--------|
| CMake | 3.16+ | Build system |
| GCC/Clang | GCC 10+ / Clang 12+ | C++20 compiler |
| linuxdeploy | Latest | AppImage bundler |
| appimagetool | Latest | AppImage creator |
| Node.js | 18+ | GUI build |
| pnpm/npm | Latest | Node package manager |

### 2.2 Install Build Tools

```bash
# Install build essentials (Ubuntu/Debian)
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    wget \
    file \
    fuse \
    libfuse2

# Install linuxdeploy
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage
sudo mv linuxdeploy-x86_64.AppImage /usr/local/bin/linuxdeploy

# Install appimagetool
wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x appimagetool-x86_64.AppImage
sudo mv appimagetool-x86_64.AppImage /usr/local/bin/appimagetool
```

### 2.3 Install Dependencies

```bash
# Core dependencies
sudo apt-get install -y \
    libssl-dev \
    libsqlite3-dev \
    libsodium-dev \
    zlib1g-dev

# GUI dependencies
sudo apt-get install -y \
    nodejs \
    npm

# Install pnpm
npm install -g pnpm
```

---

## 3. Build Process

### 3.1 Quick Build

```bash
# Clone repository
git clone https://github.com/reicalasso/SentinelFS.git
cd SentinelFS

# Build AppImage
./scripts/build_appimage.sh

# Output: SentinelFS-1.0.0-x86_64.AppImage
```

### 3.2 Manual Build Steps

#### Step 1: Build Daemon and CLI

```bash
# Create build directory
mkdir -p build && cd build

# Configure with Release mode
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_SHARED_LIBS=ON

# Build
make -j$(nproc)

# Install to AppDir
make install DESTDIR=../AppDir
cd ..
```

#### Step 2: Build GUI

```bash
cd gui

# Install dependencies
pnpm install

# Build for production
pnpm build

# Build Electron app
pnpm electron:build --linux --dir

cd ..
```

#### Step 3: Prepare AppDir

```bash
# Create AppDir structure
mkdir -p AppDir/usr/{bin,lib,share/applications,share/icons/hicolor/256x256/apps}

# Copy daemon and CLI
cp build/app/daemon/sentinel_daemon AppDir/usr/bin/
cp build/app/cli/sentinel_cli AppDir/usr/bin/

# Copy plugins
mkdir -p AppDir/usr/lib/sentinelfs/plugins
cp build/plugins/*/*.so AppDir/usr/lib/sentinelfs/plugins/

# Copy GUI
cp -r gui/dist-electron/* AppDir/usr/share/sentinelfs-gui/

# Copy config templates
mkdir -p AppDir/usr/share/sentinelfs/config
cp config/*.conf AppDir/usr/share/sentinelfs/config/
```

#### Step 4: Create Desktop Entry

```bash
cat > AppDir/usr/share/applications/sentinelfs.desktop << 'EOF'
[Desktop Entry]
Name=SentinelFS
Comment=P2P File Synchronization
Exec=sentinel_daemon
Icon=sentinelfs
Type=Application
Categories=Utility;Network;FileTransfer;
Terminal=false
StartupNotify=true
Keywords=sync;p2p;files;backup;
EOF
```

#### Step 5: Create AppRun Script

```bash
cat > AppDir/AppRun << 'EOF'
#!/bin/bash
SELF=$(readlink -f "$0")
HERE=${SELF%/*}

export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
export SENTINELFS_PLUGIN_PATH="${HERE}/usr/lib/sentinelfs/plugins"

case "$1" in
    daemon|--daemon)
        shift
        exec "${HERE}/usr/bin/sentinel_daemon" "$@"
        ;;
    cli|--cli)
        shift
        exec "${HERE}/usr/bin/sentinel_cli" "$@"
        ;;
    gui|--gui)
        shift
        exec "${HERE}/usr/share/sentinelfs-gui/sentinelfs" "$@"
        ;;
    *)
        # Default: start GUI
        exec "${HERE}/usr/share/sentinelfs-gui/sentinelfs" "$@"
        ;;
esac
EOF
chmod +x AppDir/AppRun
```

#### Step 6: Bundle Libraries

```bash
# Use linuxdeploy to bundle dependencies
linuxdeploy \
    --appdir AppDir \
    --executable AppDir/usr/bin/sentinel_daemon \
    --executable AppDir/usr/bin/sentinel_cli \
    --desktop-file AppDir/usr/share/applications/sentinelfs.desktop \
    --icon-file assets/sentinelfs-256.png
```

#### Step 7: Create AppImage

```bash
# Generate AppImage
appimagetool AppDir SentinelFS-1.0.0-x86_64.AppImage

# Verify
./SentinelFS-1.0.0-x86_64.AppImage --help
```

---

## 4. AppImage Structure

### 4.1 Directory Layout

```
AppDir/
├── AppRun                              # Entry point
├── sentinelfs.desktop                  # Desktop entry
├── sentinelfs.png                      # Icon
└── usr/
    ├── bin/
    │   ├── sentinel_daemon             # Main daemon
    │   └── sentinel_cli                # CLI tool
    ├── lib/
    │   ├── libssl.so.1.1               # OpenSSL
    │   ├── libsqlite3.so.0             # SQLite
    │   ├── libsodium.so.23             # Libsodium
    │   ├── libonnxruntime.so           # ONNX Runtime
    │   └── sentinelfs/
    │       └── plugins/
    │           ├── libnetwork_plugin.so
    │           ├── libstorage_plugin.so
    │           ├── libfilesystem_plugin.so
    │           └── libml_plugin.so
    └── share/
        ├── applications/
        │   └── sentinelfs.desktop
        ├── icons/
        │   └── hicolor/256x256/apps/
        │       └── sentinelfs.png
        └── sentinelfs/
            ├── config/
            │   ├── sentinel.conf.template
            │   └── plugins.conf.template
            └── gui/
                └── [Electron app files]
```

### 4.2 Bundled Libraries

| Library | Version | Size |
|:--------|:--------|:-----|
| libssl | 1.1.1+ | ~3 MB |
| libcrypto | 1.1.1+ | ~2 MB |
| libsqlite3 | 3.35+ | ~1 MB |
| libsodium | 1.0.18+ | ~500 KB |
| libonnxruntime | 1.15+ | ~50 MB |
| libstdc++ | GCC 10+ | ~2 MB |

---

## 5. Distribution

### 5.1 Release Checklist

```
□ Version number updated in CMakeLists.txt
□ CHANGELOG.md updated
□ All tests passing
□ AppImage built in Release mode
□ AppImage tested on multiple distributions
□ AppImage signed (optional)
□ SHA256 checksum generated
□ GitHub release created
```

### 5.2 Generate Checksum

```bash
# Generate SHA256
sha256sum SentinelFS-1.0.0-x86_64.AppImage > SentinelFS-1.0.0-x86_64.AppImage.sha256

# Verify
sha256sum -c SentinelFS-1.0.0-x86_64.AppImage.sha256
```

### 5.3 Sign AppImage (Optional)

```bash
# Sign with GPG
gpg --armor --detach-sign SentinelFS-1.0.0-x86_64.AppImage

# Verify signature
gpg --verify SentinelFS-1.0.0-x86_64.AppImage.asc
```

---

## 6. Usage

### 6.1 First Run

```bash
# Download AppImage
wget https://github.com/reicalasso/SentinelFS/releases/download/v1.0.0/SentinelFS-1.0.0-x86_64.AppImage

# Make executable
chmod +x SentinelFS-1.0.0-x86_64.AppImage

# Run GUI (default)
./SentinelFS-1.0.0-x86_64.AppImage

# Run daemon
./SentinelFS-1.0.0-x86_64.AppImage daemon --config ~/sentinel.conf

# Run CLI
./SentinelFS-1.0.0-x86_64.AppImage cli status
```

### 6.2 Desktop Integration

```bash
# Extract and integrate
./SentinelFS-1.0.0-x86_64.AppImage --appimage-extract-and-run

# Or manually add to path
sudo ln -sf /path/to/SentinelFS-1.0.0-x86_64.AppImage /usr/local/bin/sentinelfs
```

### 6.3 Command Reference

| Command | Description |
|:--------|:------------|
| `./SentinelFS.AppImage` | Start GUI |
| `./SentinelFS.AppImage daemon` | Start daemon |
| `./SentinelFS.AppImage cli <cmd>` | Run CLI command |
| `./SentinelFS.AppImage --help` | Show help |
| `./SentinelFS.AppImage --version` | Show version |
| `./SentinelFS.AppImage --appimage-extract` | Extract contents |

---

## 7. Troubleshooting

### 7.1 FUSE Not Available

```bash
# Error: Cannot mount AppImage, please check your FUSE setup

# Solution 1: Install FUSE
sudo apt-get install fuse libfuse2

# Solution 2: Extract and run
./SentinelFS-1.0.0-x86_64.AppImage --appimage-extract
./squashfs-root/AppRun
```

### 7.2 Library Not Found

```bash
# Error: error while loading shared libraries: libXXX.so

# Solution: Check bundled libraries
./SentinelFS-1.0.0-x86_64.AppImage --appimage-extract
ldd squashfs-root/usr/bin/sentinel_daemon

# Missing libraries can be added during build
```

### 7.3 Permission Denied

```bash
# Error: Permission denied

# Solution: Make executable
chmod +x SentinelFS-1.0.0-x86_64.AppImage

# Check file system (some don't support execute)
mount | grep $(df . --output=source | tail -1)
```

### 7.4 Sandbox Issues

```bash
# Error: Unable to create sandbox

# Solution: Run with --no-sandbox for Electron GUI
./SentinelFS-1.0.0-x86_64.AppImage gui --no-sandbox
```

### 7.5 Debug Mode

```bash
# Extract and run with verbose output
./SentinelFS-1.0.0-x86_64.AppImage --appimage-extract
SENTINELFS_DEBUG=1 ./squashfs-root/AppRun daemon --log-level TRACE
```

---

## Appendix: Build Script

Complete build script: `scripts/build_appimage.sh`

```bash
#!/bin/bash
set -e

VERSION="1.0.0"
ARCH="x86_64"
OUTPUT="SentinelFS-${VERSION}-${ARCH}.AppImage"

echo "Building SentinelFS AppImage v${VERSION}..."

# Clean previous build
rm -rf AppDir build_appimage

# Build daemon
mkdir -p build_appimage && cd build_appimage
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
make -j$(nproc)
make install DESTDIR=../AppDir
cd ..

# Build GUI
cd gui
pnpm install && pnpm build && pnpm electron:build --linux --dir
cd ..

# Setup AppDir
# ... (see manual steps above)

# Bundle and create AppImage
linuxdeploy --appdir AppDir
appimagetool AppDir ${OUTPUT}

echo "Built: ${OUTPUT}"
sha256sum ${OUTPUT}
```

---

**AppImage Build Guide End**

*SentinelFS Build Team - December 2025*
