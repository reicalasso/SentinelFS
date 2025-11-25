# 🛡️ SentinelFS Neo

**P2P Distributed File Sync with Auto-Remesh & Delta Transfer**

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C++-17-00599C.svg)](https://isocpp.org)
[![Electron](https://img.shields.io/badge/Electron-React-47848F.svg)](https://www.electronjs.org/)
[![SQLite](https://img.shields.io/badge/SQLite-3-003B57.svg)](https://sqlite.org/)

<p align="center">
  <img src="docs/251124_22h09m15s_screenshot.png" alt="SentinelFS GUI" width="800">
</p>

---

## 🚀 Overview

SentinelFS is a **lightweight, high-performance P2P file synchronization system** with:

- **Auto-Remesh Network** — Adaptive mesh topology based on RTT/jitter metrics
- **Delta Sync Engine** — rsync-style block-level transfers (Adler32 + SHA-256)
- **Real-time File Watching** — inotify (Linux), FSEvents (macOS), ReadDirectoryChangesW (Windows)
- **ML Anomaly Detection** — IsolationForest via ONNX Runtime (optional)
- **Modern GUI** — Electron + React + TailwindCSS

---

## 📦 Project Structure

```
SentinelFS/
├── core/                   # Shared C++ library
│   ├── include/            # Public interfaces (IPlugin, INetworkAPI, IStorageAPI, IFileAPI)
│   ├── network/            # Discovery, delta engine, bandwidth limiter
│   ├── security/           # Crypto, SessionCode
│   ├── sync/               # EventHandlers, FileSyncHandler, ConflictResolver
│   └── utils/              # Logger, EventBus, PluginManager, MetricsCollector
├── plugins/                # Runtime modules
│   ├── filesystem/         # OS file watchers
│   ├── network/            # TCP/UDP handlers
│   ├── storage/            # SQLite, PeerManager
│   └── ml/                 # ONNX anomaly detection
├── app/
│   ├── cli/                # sentinel_cli
│   └── daemon/             # sentinel_daemon (IPC server)
├── gui/                    # Electron + React frontend
│   ├── electron/           # Main process
│   └── src/                # React components
└── tests/                  # Unit & integration tests
```

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| **P2P Discovery** | UDP broadcast + TCP fallback |
| **Auto-Remesh** | Dynamic topology based on network metrics |
| **Delta Sync** | Only transfer changed blocks |
| **AES-256 Encryption** | End-to-end encrypted transfers |
| **Session Codes** | 6-character codes for peer authentication |
| **Bandwidth Limiting** | Configurable upload/download limits |
| **Real-time Monitoring** | GUI dashboard with live metrics |
| **ML Security** | Anomaly detection for suspicious activity |

---

## 🔧 Build & Run

### Requirements

- GCC/Clang (C++17)
- CMake 3.12+
- SQLite3-dev
- OpenSSL 1.1+
- Node.js 18+ (for GUI)

### Build Daemon

```bash
cd SentinelFS
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Run Daemon

```bash
# Set plugin directory and run
export SENTINELFS_PLUGIN_DIR=$(pwd)/plugins
./app/daemon/sentinel_daemon --port 8080 --discovery-port 9999

# With session code (recommended)
./app/daemon/sentinel_daemon --generate-code
./app/daemon/sentinel_daemon --session-code ABC123 --encrypt
```

### Run GUI

```bash
cd gui
npm install
npm run dev     # Development mode
npm run build   # Production build
```

---

## 🎮 GUI Features

### Dashboard
- Real-time network traffic graphs
- Sync status overview
- Recent activity feed

### My Files
- Hierarchical folder tree view
- Add/remove watched directories
- Search & filter files

### Network Mesh
- Connected peers list
- Peer discovery (Scan for Devices)
- Connection status indicators

### Transfers
- Active transfer queue
- Upload/download progress
- Transfer history

### Settings
- **General** — Sync toggle, configuration display
- **Network** — Bandwidth limits (upload/download)
- **Security** — Session code, AES-256 encryption toggle
- **Advanced** — System info, delta engine config, danger zone

---

## 🗄️ Database Schema

| Table | Purpose |
|-------|---------|
| `device` | Local device identity |
| `session` | Active sync sessions |
| `files` | File metadata & hashes |
| `file_version` | Version history |
| `sync_queue` | Pending transfers |
| `peers` | Known peer information |
| `watched_folders` | Monitored directories |
| `file_access_log` | ML training data |

---

## 🔒 Security

- **Session Code Authentication** — Peers must share the same 6-character code
- **AES-256-CBC Encryption** — All file transfers encrypted
- **HMAC Verification** — Message integrity checks
- **Key Derivation** — PBKDF2 from session code

---

## 📡 IPC Commands

The daemon exposes a Unix socket for GUI/CLI communication:

| Command | Description |
|---------|-------------|
| `STATUS_JSON` | Get daemon status |
| `METRICS_JSON` | Get bandwidth metrics |
| `PEERS_JSON` | List connected peers |
| `FILES_JSON` | List watched files |
| `CONFIG_JSON` | Get current configuration |
| `SET_CONFIG key=value` | Update configuration |
| `DISCOVER` | Trigger peer discovery |
| `PAUSE` / `RESUME` | Toggle synchronization |

---

## 🧬 Architecture

```
┌─────────────────────────────────────────────┐
│             GUI (Electron + React)          │
├─────────────────────────────────────────────┤
│                 IPC Socket                  │
├─────────────────────────────────────────────┤
│              Daemon (sentinel_daemon)       │
│  ┌─────────┬──────────┬──────────┬───────┐  │
│  │ Network │ Storage  │ FileSystem│  ML  │  │
│  │ Plugin  │ Plugin   │  Plugin  │Plugin │  │
│  └─────────┴──────────┴──────────┴───────┘  │
├─────────────────────────────────────────────┤
│              Core Library                   │
│  EventBus • Logger • PluginManager • Crypto │
└─────────────────────────────────────────────┘
```

---

## 🧪 Testing

```bash
cd build
ctest --output-on-failure

# Individual tests
./tests/discovery_test
./tests/delta_test
./tests/storage_test
```

---

## 📝 License

MIT License — See [LICENSE](LICENSE) for details.

---

<div align="center">
  <strong>SentinelFS Neo</strong><br>
  <em>Distributed systems meet real-time intelligence.</em><br><br>
  ⭐ Star this repo if you find it useful! ⭐
</div>