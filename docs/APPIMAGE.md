# SentinelFS AppImage Build Guide

## Overview
SentinelFS AppImage combines the GUI (Electron-based) and the C++ daemon into a single, portable executable for Linux systems.

## Features
- **Portable**: Single-file distribution, no installation required
- **Self-contained**: Includes GUI, daemon, CLI, and all plugins
- **Auto-start daemon**: GUI automatically spawns and manages the daemon process
- **No dependencies**: All required libraries bundled within the AppImage

## Building the AppImage

### Prerequisites
- Linux system (Ubuntu 20.04+ recommended)
- Node.js 18+ and npm
- CMake 3.17+
- GCC/G++ with C++17 support
- OpenSSL development libraries

### Install Dependencies

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev nodejs npm

# Fedora/RHEL
sudo dnf install -y gcc-c++ cmake openssl-devel nodejs npm
```

### Build Command

```bash
# From project root
./scripts/build_appimage.sh
```

Or using CMake:

```bash
# Configure and build
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release
cmake --build build_release --parallel

# Build AppImage
cmake --build build_release --target appimage
```

### Output

The AppImage will be created in:
- `gui/dist/SentinelFS-*.AppImage`
- Copied to project root for convenience

## Running the AppImage

```bash
# Make executable (if not already)
chmod +x SentinelFS-*.AppImage

# Run
./SentinelFS-*.AppImage
```

## What's Included

The AppImage bundles:
1. **Electron GUI** - Modern web-based interface
2. **sentinel_daemon** - Core sync daemon
3. **sentinel_cli** - Command-line interface
4. **Plugins** - All compiled plugins (.so files)
5. **Config template** - Default sentinel.conf

## Architecture

```
SentinelFS.AppImage/
├── resources/
│   ├── bin/
│   │   ├── sentinel_daemon    # Main daemon
│   │   └── sentinel_cli       # CLI tool
│   ├── lib/
│   │   ├── libsentinel_core.a
│   │   └── plugins/
│   │       ├── filesystem_plugin.so
│   │       ├── network_plugin.so
│   │       ├── storage_plugin.so
│   │       └── ml_plugin.so
│   └── config/
│       └── sentinel.conf      # Default config
└── dist/
    └── [Electron GUI files]
```

## How It Works

1. **User launches AppImage**
   - AppImage mounts itself to a temporary location
   - Electron GUI starts

2. **GUI starts daemon**
   - Checks if daemon is already running via socket
   - If not, spawns daemon process with appropriate environment
   - Sets `SENTINELFS_PLUGIN_DIR` to bundled plugins

3. **Daemon initializes**
   - Creates Unix socket for IPC
   - Loads plugins from bundled location
   - Starts sync engine

4. **GUI connects**
   - Connects to daemon via Unix socket
   - Displays real-time metrics and status
   - Sends commands (start sync, change config, etc.)

## Configuration

The daemon looks for config in order:
1. `~/.config/sentinelfs/sentinel.conf` (user config)
2. Bundled `resources/config/sentinel.conf` (default)

## Troubleshooting

### AppImage won't start
```bash
# Check permissions
chmod +x SentinelFS-*.AppImage

# Run with debug output
./SentinelFS-*.AppImage --verbose

# Extract and inspect
./SentinelFS-*.AppImage --appimage-extract
```

### Daemon fails to start
```bash
# Check logs (location varies)
tail -f ~/.local/share/sentinelfs/logs/daemon.log

# Verify socket path
ls -la /run/user/$(id -u)/sentinelfs/
# or
ls -la /tmp/sentinelfs/
```

### Plugin loading issues
```bash
# List bundled plugins
./SentinelFS-*.AppImage --appimage-extract
ls squashfs-root/resources/lib/plugins/
```

## CI/CD Integration

See `.github/workflows/appimage.yml` for automated builds on every push/PR.

## Distribution

The AppImage can be distributed as-is:
- Upload to GitHub Releases
- Host on your own server
- Users download and run directly

No installation required, no root privileges needed.

## Known Limitations

1. **FUSE requirement**: AppImage requires FUSE to mount itself
   - Usually pre-installed on modern Linux
   - Install with: `sudo apt-get install fuse libfuse2`

2. **First run delay**: Initial daemon startup takes 1-2 seconds

3. **Socket cleanup**: If daemon crashes, socket file may remain
   - Clean manually: `rm /run/user/$(id -u)/sentinelfs/sentinel_daemon.sock`

## Development

To test changes:

```bash
# Rebuild C++ components
cmake --build build_release --parallel

# Rebuild GUI and AppImage
cd gui
npm run build

# The AppImage is automatically recreated
```

## Resources

- [AppImage Documentation](https://docs.appimage.org/)
- [Electron Builder](https://www.electron.build/)
- [SentinelFS Architecture](../ARCHITECTURE.md)
