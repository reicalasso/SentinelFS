# 🛡️ SentinelFS

**Enterprise-Grade P2P File Synchronization with Advanced Threat Detection**

SentinelFS is a high-performance, secure, and modular peer-to-peer file synchronization system designed for privacy, efficiency, and security. Built with modern C++ for the core daemon and Electron/React for the user interface, it combines raw performance with an intuitive user experience.

[![License](https://img.shields.io/badge/license-SPL--1.0-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C++-17%2F20-00599C?logo=c%2B%2B)](https://isocpp.org/)
[![TypeScript](https://img.shields.io/badge/TypeScript-5.0-3178C6?logo=typescript)](https://www.typescriptlang.org/)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey)](https://github.com/reicalasso/SentinelFS)

---

## 📋 Table of Contents

- [Overview](#overview)
- [Key Features](#-key-features)
- [Architecture](#-architecture)
- [Technology Stack](#-technology-stack)
- [Installation](#-installation)
- [Usage](#-usage)
- [Configuration](#-configuration)
- [Development](#-development)
- [Documentation](#-documentation)
- [Contributing](#-contributing)
- [License](#-license)

---

## Overview

SentinelFS eliminates the need for central servers by allowing devices to sync data directly in a peer-to-peer mesh network. All data transfers are protected with military-grade encryption, and an integrated machine learning engine provides real-time threat detection against ransomware and malicious file patterns.

### Project Statistics

| Metric | Value |
|--------|-------|
| **Core Codebase (C++)** | ~16,600 Lines |
| **UI Codebase (TypeScript)** | ~3,000 Lines |
| **Total Source Files** | ~150 Files |
| **Architecture** | Plugin-based P2P Mesh |
| **Encryption** | AES-256-CBC + HMAC-SHA256 |
| **Test Coverage** | 30+ Unit & Integration Tests |

---

## 🚀 Key Features

### 🔒 Security & Privacy

- **End-to-End Encryption**: All file transfers encrypted with AES-256-CBC
- **Zero-Knowledge Architecture**: No central server stores your data or metadata
- **Session-Based Authentication**: Secure handshake mechanism with session codes
- **Real-Time Threat Detection**: ML-powered anomaly detection using ONNX Runtime
- **Automatic Quarantine**: Suspicious files isolated automatically
- **Integrity Verification**: SHA-256 checksums for all file transfers

### ⚡ Performance & Efficiency

- **Delta Sync Engine**: Transfers only modified blocks using Adler32 rolling checksums
- **Bandwidth Optimization**: Saves up to 99% bandwidth on large file updates
- **Intelligent Auto-Remesh**: Automatic network topology optimization based on RTT and jitter
- **Low Resource Footprint**: Native C++ daemon with minimal CPU and memory usage
- **Concurrent Transfers**: Multi-threaded architecture for parallel file synchronization
- **Compression Support**: Optional data compression for reduced bandwidth usage

### 🧩 Modularity & Extensibility

- **Plugin Architecture**: Core functionality decoupled into independent plugins
  - **IronRoot**: Advanced filesystem monitoring
  - **NetFalcon**: Multi-transport network layer (TCP, QUIC, WebRTC)
  - **FalconStore**: High-performance SQLite-based storage
  - **Zer0**: ML-based threat detection and analysis
- **Cross-Platform Support**: Linux, macOS, and Windows compatibility
- **Modern GUI**: Electron-based desktop application with React
- **CLI Tools**: Comprehensive command-line interface for automation

### 📊 Monitoring & Management

- **Real-Time Dashboard**: Live system metrics and peer status
- **Activity Logging**: Comprehensive audit trail of all operations
- **Health Monitoring**: System health checks and performance metrics
- **Prometheus Integration**: Export metrics for monitoring systems
- **Grafana Dashboards**: Pre-built visualization templates

---

## 🏗️ Architecture

SentinelFS uses a modular, event-driven architecture:

```
┌─────────────────────────────────────────────────────────┐
│                   Electron GUI (React)                  │
│          Dashboard │ Files │ Peers │ Settings           │
└────────────────────────┬────────────────────────────────┘
                         │ IPC (Unix Socket)
                         ▼
┌─────────────────────────────────────────────────────────┐
│                  Sentinel Daemon (C++)                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ Plugin Mgr   │  │  Event Bus   │  │  IPC Handler │  │
│  └──────┬───────┘  └──────┬───────┘  └──────────────┘  │
│         │                 │                             │
│  ┌──────┴─────────────────┴──────────────────────┐     │
│  │              Core Libraries                    │     │
│  │  • DeltaEngine  • Crypto  • Sync  • Utils    │     │
│  └───────────────────────────────────────────────┘     │
└────────────────────────┬────────────────────────────────┘
                         │
         ┌───────────────┼───────────────┬───────────────┐
         ▼               ▼               ▼               ▼
   ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
   │ IronRoot │   │NetFalcon │   │FalconStore│   │   Zer0   │
   │(FileSync)│   │(Network) │   │ (Storage)│   │   (ML)   │
   └──────────┘   └──────────┘   └──────────┘   └──────────┘
```

### Core Components

1. **Daemon Core**: Main service managing plugins, events, and IPC
2. **Event Bus**: Pub/sub system for decoupled component communication
3. **Plugin System**: Dynamic loading of modular functionality
4. **Delta Sync Engine**: Efficient binary diff/patch algorithm
5. **Security Layer**: Encryption, authentication, and key management

For detailed architecture documentation, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## 🛠️ Technology Stack

### Backend (Daemon)

| Component | Technology | Purpose |
|-----------|------------|----------|
| **Language** | C++17/C++20 | Core daemon implementation |
| **Networking** | Boost.Asio | Async I/O and network operations |
| **Storage** | SQLite3 (WAL) | Metadata and state persistence |
| **Cryptography** | OpenSSL 1.1+ | Encryption and hashing |
| **ML Runtime** | ONNX Runtime | Threat detection models |
| **Build System** | CMake 3.15+ | Cross-platform builds |
| **Testing** | Google Test | Unit and integration tests |

### Frontend (GUI)

| Component | Technology | Purpose |
|-----------|------------|----------|
| **Framework** | Electron 28 | Desktop application wrapper |
| **UI Library** | React 18 | Component-based UI |
| **Language** | TypeScript 5 | Type-safe development |
| **Styling** | TailwindCSS 3 | Utility-first CSS |
| **Icons** | Lucide React | Modern icon set |
| **Charts** | Recharts | Data visualization |
| **Build Tool** | Vite 5 | Fast development builds |

---

## 📦 Installation

### System Requirements

- **OS**: Linux (Ubuntu 20.04+, Debian 11+, Fedora 35+), macOS 11+, or Windows 10+
- **RAM**: Minimum 512 MB, Recommended 2 GB
- **Disk**: 500 MB for installation
- **Network**: IPv4/IPv6 connectivity

### Option 1: Pre-built Packages (Recommended)

#### Linux AppImage
```bash
# Download latest release
wget https://github.com/reicalasso/SentinelFS/releases/latest/download/SentinelFS-1.0.0-x64.AppImage

# Make executable
chmod +x SentinelFS-1.0.0-x64.AppImage

# Run
./SentinelFS-1.0.0-x64.AppImage
```

#### Debian/Ubuntu (.deb)
```bash
# Download and install
wget https://github.com/reicalasso/SentinelFS/releases/latest/download/SentinelFS-1.0.0-amd64.deb
sudo dpkg -i SentinelFS-1.0.0-amd64.deb
sudo apt-get install -f  # Install dependencies

# Start daemon
sudo systemctl enable sentinel_daemon
sudo systemctl start sentinel_daemon
```

### Option 2: Build from Source

#### Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git \
    libboost-all-dev libssl-dev libsqlite3-dev \
    onnxruntime-dev \
    nodejs npm
```

**Fedora/RHEL:**
```bash
sudo dnf install -y \
    gcc-c++ cmake git \
    boost-devel openssl-devel sqlite-devel \
    onnxruntime-devel \
    nodejs npm
```

**macOS:**
```bash
brew install cmake boost openssl sqlite3 onnxruntime node
```

#### Build Steps

```bash
# Clone repository
git clone https://github.com/reicalasso/SentinelFS.git
cd SentinelFS/SentinelFS

# Quick build (recommended)
./scripts/start_safe.sh --rebuild

# Or manual build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

#### Build GUI

```bash
cd gui
npm install
npm run build:linux  # Creates AppImage and .deb packages
```

---

## 🚀 Usage

### Starting the System

#### Using the GUI (Recommended)
```bash
# Launch the desktop application
sentinelfs
# Or if using AppImage:
./SentinelFS-1.0.0-x64.AppImage
```

#### Using the Daemon Directly
```bash
# Start daemon
sentinel_daemon

# Or with custom config
sentinel_daemon --config /path/to/sentinel.conf
```

### Command-Line Interface

```bash
# Check daemon status
sentinel_cli status

# List connected peers
sentinel_cli peers

# Pause synchronization
sentinel_cli pause

# Resume synchronization
sentinel_cli resume

# Connect to a peer
sentinel_cli connect <peer_id> <ip> <port>

# View system health
sentinel_cli health
```

### Creating a Sync Network

1. **Generate a session code** on the first device:
   ```bash
   sentinel_cli generate-session
   # Output: SESSION-ABCD-1234-EFGH-5678
   ```

2. **Configure other devices** with the same session code in `sentinel.conf`:
   ```ini
   [network]
   session_code = SESSION-ABCD-1234-EFGH-5678
   ```

3. **Start daemons** on all devices - they will automatically discover each other

4. **Monitor connections** via GUI or CLI:
   ```bash
   sentinel_cli peers
   ```

---

## ⚙️ Configuration

### Configuration File Location

- **Linux**: `~/.config/sentinelfs/sentinel.conf`
- **macOS**: `~/Library/Application Support/sentinelfs/sentinel.conf`
- **Windows**: `%APPDATA%\sentinelfs\sentinel.conf`

### Example Configuration

```ini
[general]
sync_folder = ~/SentinelFS
log_level = info
log_file = ~/.local/share/sentinelfs/daemon.log

[network]
session_code = SESSION-YOUR-CODE-HERE
listen_port = 9000
discovery_port = 9001
enable_upnp = true
bandwidth_limit_mbps = 0  # 0 = unlimited

[security]
enable_encryption = true
encryption_algorithm = aes-256-cbc
enable_threat_detection = true
auto_quarantine = true

[sync]
enable_delta_sync = true
enable_compression = true
conflict_resolution = newest  # newest, oldest, manual
ignore_patterns = *.tmp,*.swp,.git/

[storage]
database_path = ~/.local/share/sentinelfs/sentinel.db
enable_wal = true
vacuum_interval_hours = 168  # Weekly

[monitoring]
enable_metrics = true
metrics_port = 9090
health_check_interval = 30
```

---

## 🔧 Development

### Project Structure

```
SentinelFS/
├── app/
│   ├── daemon/          # Main daemon executable
│   └── cli/             # Command-line interface
├── core/
│   ├── network/         # Delta sync, bandwidth control
│   ├── security/        # Encryption, authentication
│   ├── sync/            # File synchronization logic
│   └── utils/           # Utilities (logger, config, etc.)
├── plugins/
│   ├── ironroot/        # Filesystem monitoring plugin
│   ├── netfalcon/       # Advanced network transport
│   ├── falconstore/     # Storage backend
│   └── zer0/            # ML threat detection
├── gui/
│   ├── electron/        # Electron main process
│   └── src/             # React UI components
├── tests/
│   ├── unit/            # Unit tests
│   └── integration/     # Integration tests
├── scripts/             # Build and deployment scripts
├── config/              # Configuration templates
└── docs/                # Additional documentation
```

### Running Tests

```bash
# Build with tests
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Run all tests
ctest --output-on-failure

# Run specific test
./tests/test_delta_engine
```

### Development Workflow

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/amazing-feature`
3. **Make changes and test**: `ctest`
4. **Commit with clear messages**: `git commit -m 'Add amazing feature'`
5. **Push to your fork**: `git push origin feature/amazing-feature`
6. **Open a Pull Request**

### Code Style

- **C++**: Follow [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- **TypeScript**: Use ESLint with provided configuration
- **Formatting**: Use `clang-format` for C++ and `prettier` for TypeScript

---

## 📚 Documentation

- **[Architecture Overview](ARCHITECTURE.md)** - System design and component interaction
- **[Database Schema](docs/DATABASE_ER_DIAGRAM.md)** - SQLite database structure
- **[Production Deployment](docs/PRODUCTION.md)** - Production setup guide
- **[Monitoring Guide](docs/MONITORING.md)** - Prometheus and Grafana setup
- **[AppImage Build](docs/APPIMAGE.md)** - Creating AppImage packages
- **[Security Features](docs/security/)** - Threat detection and quarantine

---

## 🤝 Contributing

We welcome contributions! Please see our contributing guidelines:

1. **Report bugs** via [GitHub Issues](https://github.com/reicalasso/SentinelFS/issues)
2. **Suggest features** through issue discussions
3. **Submit pull requests** for bug fixes or features
4. **Improve documentation** - always appreciated!

### Development Setup

```bash
# Install development dependencies
./scripts/install_deps.sh

# Build in debug mode
./scripts/start_safe.sh --rebuild

# Run tests
cd build && ctest
```

---

## 📄 License

This project is licensed under the **SentinelFS Public License (SPL-1.0)** - see the [LICENSE](LICENSE) file for details.

The SPL-1.0 is a security-conscious, modernized extension of the MIT License that:
- Protects trademark and branding
- Prohibits malicious use
- Includes patent grant provisions
- Maintains open-source flexibility

---

## 🙏 Acknowledgments

- **Boost.Asio** for async networking
- **SQLite** for embedded database
- **OpenSSL** for cryptography
- **ONNX Runtime** for ML inference
- **Electron** and **React** for the GUI
- All contributors and users of SentinelFS

---

## 📞 Support

- **Documentation**: [docs/](docs/)
- **Issues**: [GitHub Issues](https://github.com/reicalasso/SentinelFS/issues)
- **Discussions**: [GitHub Discussions](https://github.com/reicalasso/SentinelFS/discussions)

---

<div align="center">

**Built with ❤️ for privacy and security**

[⬆ Back to Top](#️-sentinelfs)

</div>