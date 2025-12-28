# Installation Guide

This guide provides detailed installation instructions for SentinelFS on various platforms.

## Table of Contents

- [System Requirements](#system-requirements)
- [Linux Installation](#linux-installation)
- [macOS Installation](#macos-installation)
- [Windows Installation](#windows-installation)
- [Building from Source](#building-from-source)
- [Post-Installation](#post-installation)
- [Troubleshooting](#troubleshooting)

---

## System Requirements

### Minimum Requirements

- **CPU**: 64-bit processor (x86_64 or ARM64)
- **RAM**: 512 MB available memory
- **Disk**: 500 MB free space for installation
- **Network**: IPv4 or IPv6 connectivity
- **OS**: 
  - Linux: Ubuntu 20.04+, Debian 11+, Fedora 35+, or equivalent
  - macOS: 11.0 (Big Sur) or later
  - Windows: 10 (64-bit) or later

### Recommended Requirements

- **CPU**: Multi-core processor (4+ cores)
- **RAM**: 2 GB available memory
- **Disk**: 2 GB free space (for sync folder)
- **Network**: Broadband connection (10+ Mbps)

---

## Linux Installation

### Option 1: AppImage (Recommended)

AppImage is a universal package format that works on most Linux distributions without installation.

```bash
# Download the latest AppImage
wget https://github.com/reicalasso/SentinelFS/releases/latest/download/SentinelFS-1.0.0-x64.AppImage

# Make it executable
chmod +x SentinelFS-1.0.0-x64.AppImage

# Run the application
./SentinelFS-1.0.0-x64.AppImage
```

**Optional: Integrate with system**
```bash
# Move to applications directory
sudo mv SentinelFS-1.0.0-x64.AppImage /opt/sentinelfs/

# Create desktop entry
cat > ~/.local/share/applications/sentinelfs.desktop << EOF
[Desktop Entry]
Name=SentinelFS
Comment=Secure P2P File Synchronization
Exec=/opt/sentinelfs/SentinelFS-1.0.0-x64.AppImage
Icon=sentinelfs
Terminal=false
Type=Application
Categories=Utility;FileTools;Network;
EOF
```

### Option 2: Debian/Ubuntu Package

```bash
# Download the .deb package
wget https://github.com/reicalasso/SentinelFS/releases/latest/download/SentinelFS-1.0.0-amd64.deb

# Install the package
sudo dpkg -i SentinelFS-1.0.0-amd64.deb

# Install dependencies if needed
sudo apt-get install -f

# Enable and start the daemon
sudo systemctl enable sentinel_daemon
sudo systemctl start sentinel_daemon

# Check status
sudo systemctl status sentinel_daemon
```

### Option 3: Fedora/RHEL Package

```bash
# Download the .rpm package
wget https://github.com/reicalasso/SentinelFS/releases/latest/download/SentinelFS-1.0.0-x86_64.rpm

# Install the package
sudo dnf install SentinelFS-1.0.0-x86_64.rpm

# Enable and start the daemon
sudo systemctl enable sentinel_daemon
sudo systemctl start sentinel_daemon
```

### Option 4: Arch Linux (AUR)

```bash
# Using yay
yay -S sentinelfs

# Or using paru
paru -S sentinelfs

# Start the daemon
sudo systemctl enable --now sentinel_daemon
```

---

## macOS Installation

### Option 1: Homebrew (Recommended)

```bash
# Add the SentinelFS tap
brew tap reicalasso/sentinelfs

# Install SentinelFS
brew install sentinelfs

# Start the daemon
brew services start sentinelfs
```

### Option 2: DMG Installer

```bash
# Download the DMG
curl -LO https://github.com/reicalasso/SentinelFS/releases/latest/download/SentinelFS-1.0.0.dmg

# Open the DMG and drag SentinelFS to Applications
open SentinelFS-1.0.0.dmg

# Launch from Applications folder
open /Applications/SentinelFS.app
```

---

## Windows Installation

### Option 1: MSI Installer (Recommended)

1. Download `SentinelFS-1.0.0-x64.msi` from [releases](https://github.com/reicalasso/SentinelFS/releases)
2. Double-click the installer
3. Follow the installation wizard
4. Launch SentinelFS from Start Menu

### Option 2: Portable ZIP

```powershell
# Download the portable ZIP
Invoke-WebRequest -Uri "https://github.com/reicalasso/SentinelFS/releases/latest/download/SentinelFS-1.0.0-win64.zip" -OutFile "SentinelFS.zip"

# Extract to desired location
Expand-Archive -Path SentinelFS.zip -DestinationPath "C:\Program Files\SentinelFS"

# Run the application
& "C:\Program Files\SentinelFS\sentinelfs.exe"
```

---

## Building from Source

### Prerequisites

Install required dependencies for your platform:

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git \
    libboost-all-dev libssl-dev libsqlite3-dev \
    onnxruntime-dev \
    nodejs npm
```

#### Fedora/RHEL
```bash
sudo dnf install -y \
    gcc-c++ cmake git \
    boost-devel openssl-devel sqlite-devel \
    onnxruntime-devel \
    nodejs npm
```

#### macOS
```bash
brew install cmake boost openssl sqlite3 onnxruntime node
```

### Build Steps

```bash
# Clone the repository
git clone https://github.com/reicalasso/SentinelFS.git
cd SentinelFS/SentinelFS

# Run the automated build script
./scripts/start_safe.sh --rebuild

# Or build manually
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

### Build GUI

```bash
cd gui
npm install
npm run build:linux    # For Linux
npm run build:mac      # For macOS
npm run build:win      # For Windows
```

---

## Post-Installation

### Initial Configuration

1. **Create configuration directory**:
   ```bash
   mkdir -p ~/.config/sentinelfs
   ```

2. **Copy default configuration**:
   ```bash
   cp /etc/sentinelfs/sentinel.conf.example ~/.config/sentinelfs/sentinel.conf
   ```

3. **Edit configuration**:
   ```bash
   nano ~/.config/sentinelfs/sentinel.conf
   ```

4. **Set sync folder**:
   ```ini
   [general]
   sync_folder = ~/SentinelFS
   ```

5. **Generate session code**:
   ```bash
   sentinel_cli generate-session
   ```

6. **Add session code to config**:
   ```ini
   [network]
   session_code = SESSION-XXXX-XXXX-XXXX-XXXX
   ```

### Starting the Service

#### Linux (systemd)
```bash
# Start daemon
sudo systemctl start sentinel_daemon

# Enable on boot
sudo systemctl enable sentinel_daemon

# Check status
sudo systemctl status sentinel_daemon
```

#### macOS (launchd)
```bash
# Start daemon
brew services start sentinelfs

# Check status
brew services list | grep sentinelfs
```

#### Windows (Service)
```powershell
# Start service
Start-Service SentinelFS

# Set to start automatically
Set-Service -Name SentinelFS -StartupType Automatic
```

### Verify Installation

```bash
# Check daemon status
sentinel_cli status

# View version
sentinel_cli --version

# Check logs
tail -f ~/.local/share/sentinelfs/daemon.log
```

---

## Troubleshooting

### Common Issues

#### Daemon Won't Start

**Problem**: Daemon fails to start with "Address already in use"

**Solution**:
```bash
# Check if another instance is running
ps aux | grep sentinel_daemon

# Kill existing process
pkill sentinel_daemon

# Or change port in config
nano ~/.config/sentinelfs/sentinel.conf
# Change listen_port to different value
```

#### Permission Denied Errors

**Problem**: Cannot write to sync folder

**Solution**:
```bash
# Check folder permissions
ls -la ~/SentinelFS

# Fix permissions
chmod 755 ~/SentinelFS
chown $USER:$USER ~/SentinelFS
```

#### Missing Dependencies

**Problem**: "error while loading shared libraries"

**Solution**:
```bash
# Ubuntu/Debian
sudo apt-get install -f

# Fedora/RHEL
sudo dnf install --best --allowerasing sentinelfs

# macOS
brew reinstall sentinelfs
```

#### GUI Won't Launch

**Problem**: GUI fails to start

**Solution**:
```bash
# Check if daemon is running
sentinel_cli status

# Start daemon manually
sentinel_daemon &

# Check GUI logs
cat ~/.local/share/sentinelfs/gui.log
```

### Getting Help

If you encounter issues not covered here:

1. Check the [FAQ](docs/FAQ.md)
2. Search [GitHub Issues](https://github.com/reicalasso/SentinelFS/issues)
3. Ask in [GitHub Discussions](https://github.com/reicalasso/SentinelFS/discussions)
4. Review logs in `~/.local/share/sentinelfs/daemon.log`

---

## Uninstallation

### Linux (Debian/Ubuntu)
```bash
sudo apt-get remove sentinelfs
sudo apt-get purge sentinelfs  # Remove config files too
```

### Linux (Fedora/RHEL)
```bash
sudo dnf remove sentinelfs
```

### macOS
```bash
brew uninstall sentinelfs
```

### Windows
```powershell
# Via Control Panel
# Settings > Apps > SentinelFS > Uninstall

# Or via PowerShell
Get-Package SentinelFS | Uninstall-Package
```

### Remove User Data
```bash
# Linux/macOS
rm -rf ~/.config/sentinelfs
rm -rf ~/.local/share/sentinelfs

# Windows
Remove-Item -Recurse -Force "$env:APPDATA\sentinelfs"
Remove-Item -Recurse -Force "$env:LOCALAPPDATA\sentinelfs"
```

---

**Next Steps**: See [Usage Guide](docs/USAGE.md) for information on using SentinelFS.
