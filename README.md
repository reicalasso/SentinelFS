## 🛡️ SentinelFS Neo

Lightweight, production-grade **P2P file synchronization** built around a modular daemon, plugin-driven services, and an Electron/React control plane.

Licensing: [MIT](LICENSE)

---

## 📸 Screenshots

| Dashboard |
|:--------:|
| ![Dashboard](docs/images/dashboard.png) |

| Dashboard | Files | Transfers |
|:---------:|:-----:|:---------:|
| ![Dashboard](docs/images/251203_01h53m17s_screenshot.png) | ![Files](docs/images/251203_01h53m36s_screenshot.png) | ![Transfers](docs/images/251203_01h54m57s_screenshot.png) |

| Peers | Settings | Network Config |
|:-----:|:--------:|:--------------:|
| ![Peers](docs/images/251203_01h55m07s_screenshot.png) | ![Settings](docs/images/251203_01h55m26s_screenshot.png) | ![Network](docs/images/251203_01h55m39s_screenshot.png) |

| Security | Advanced | Logs |
|:--------:|:--------:|:----:|
| ![Security](docs/images/251203_01h55m48s_screenshot.png) | ![Advanced](docs/images/251203_01h55m56s_screenshot.png) | ![Logs](docs/images/251203_01h56m05s_screenshot.png) |

---

## ⚙️ Overview

SentinelFS ships as a daemon + GUI pair. The daemon owns the sync loop, plugins, and IPC channel, while the Electron frontend observes status, issues commands, and visualizes network state. Design principles:

- **Deterministic state**: Plugins communicate through an event bus and return explicit status codes.
- **Minimal dependencies**: Only SQLite, OpenSSL, ONNX runtime (optional) plus standard C++/Electron toolchain.
- **Single-demo topology**: Auto-remesh rebalances peers based on observed RTT/jitter/packet-loss and adapts transfer queues accordingly.
- **Low-overhead sync**: Delta engine uses Adler32 rolling checksums + SHA-256 block verification before applying patches.

---

## 📦 Repository Layout

```
SentinelFS/
├── app/                  # CLI entry (sentinel_cli) + daemon binary
│   ├── cli/
│   └── daemon/            # sentinel_daemon (IPC server bootstrapping plugins)
├── core/                 # Shared C++ libraries + interfaces
│   ├── include/           # IPlugin, IStorageAPI, INetworkAPI, IFileAPI
│   ├── network/           # Discovery, delta engine, bandwidth controller
│   ├── security/          # Crypto helpers, session code, key derivation
│   ├── sync/              # EventHandlers, FileSyncHandler, conflict resolution
│   └── utils/             # Logger, EventBus, MetricsCollector, PluginManager
├── plugins/               # Built-in plugin implementations
│   ├── filesystem/        # inotify/FSEvents/ReadDirectoryChangesW watchers
│   ├── network/           # UDP discovery, TCP remesh, mesh health scoring
│   ├── storage/           # SQLite store, peer tables, sync queue
│   └── ml/                # ONNX Runtime + IsolationForest anomaly detection
├── gui/                   # Electron + React dashboard
│   ├── electron/          # Main process (IPC + daemon controller)
│   └── src/               # React app (Dashboard, Settings, Logs)
└── tests/                 # Unit + integration suites
```

---

## ✨ Core Features

1. **Networking**
   - UDP discovery + TCP fallback for mesh discovery and handoff (`UDPDiscovery` plugin).
   - Auto-remesh using periodic RTT threads with jitter/loss scoring to keep topology healthy.
   - Session codes + AES-256-CBC encryption + HMAC integrity checks ensure private meshes.

2. **Delta Sync Engine**
   - Adler32 rolling checksums guide block-level diffs.
   - SHA-256 verifies every chunk before applying to prevent corruption.
   - Storage plugin tracks `sync_queue`, `files`, `file_versions`, and peer metadata atomically (SQLite, indexed on `file_hash`, `version`, `session_id`).

3. **Real-Time Monitoring & Controls (Electron GUI)**
   - Dashboard with bandwidth metrics, sync status, peer list, transfers, and activity feed.
   - File explorer lets users manage watched directories (add/remove), search files, and inspect metadata.
   - Transfers page surfaces upload/download queues and historical logs.
   - Settings panels expose sync toggles, bandwidth limits, network/security configs, and advanced danger-zone actions (reset settings, refresh daemon).

4. **ML Anomaly Detection (optional)**
   - IsolationForest scoring normalized between 0–1.
   - Runs asynchronously via dedicated worker thread using ONNX Runtime when enabled.
   - GUI surfaces beta notice while the feature evaluates peer behavior.

---

## 🧱 Build & Run

### Prerequisites

- GCC/Clang targeting C++17
- CMake 3.12+
- SQLite3 development libraries
- OpenSSL 1.1+
- Node.js 18+ and npm/yarn for GUI

### Quick Start: AppImage (Recommended for Linux)

**Single portable file with GUI + Daemon + CLI - no installation required!**

```bash
# Build the AppImage
./scripts/build_appimage.sh

# Run it
./SentinelFS-*.AppImage
```

The AppImage automatically:
- Starts the daemon in the background
- Launches the GUI
- Includes all plugins and dependencies
- Requires no root privileges

📖 See [AppImage Guide](docs/APPIMAGE.md) for detailed instructions.

### Build Daemon (Manual)

```bash
cmake -S . -B build -DSKIP_SYSTEM_INSTALL=ON
cmake --build build --parallel
```

For system-wide installation (requires sudo):
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

Do not forget to set the plugin directory before running:

```bash
export SENTINELFS_PLUGIN_DIR=$(pwd)/plugins
./build/app/daemon/sentinel_daemon --port 8080 --discovery-port 9999
```

Session code workflow (recommended):

```bash
./build/app/daemon/sentinel_daemon --generate-code
./build/app/daemon/sentinel_daemon --session-code ABC123 --encrypt
```

### Run GUI (development)

```bash
cd gui
npm install
npm run dev
```

In packaged mode (AppImage), Electron grabs `sentinel_daemon` from `process.resourcesPath/bin`.

---

## 🧭 Architecture Highlights

1. **DaemonCore**
   - Initializes configuration, loads plugins via `PluginLoader`, starts the event loop, and coordinates shutdown.

2. **EventBus**
   - Pub/sub for `PEER_DISCOVERED`, `FILE_MODIFIED`, `DATA_RECEIVED`, `ANOMALY_DETECTED`.
   - Keeps components decoupled and deterministic.

3. **Plugin System**
   - Storage, Network, Filesystem, and optional ML plugins implement their APIs and run in isolated threads.
   - PluginLoader ensures nightly compatibility via explicit interfaces (`IStorageAPI`, `INetworkAPI`, `IFileAPI`).

4. **IPC Handler**
   - Unix socket server processes commands (`STATUS`, `METRICS`, `FILES`, `PEERS`, `CONFIG`, `SET_CONFIG`, `DISCOVER`, `PAUSE`, `RESUME`).
   - Routes responses to Electron/CLI, wrapping arrays into objects (`{ files: [] }`, etc.) for clarity.

5. **Delta Sync Flow**
   - Filesystem plugin publishes `FILE_MODIFIED` → EventHandlers triggers `UPDATE_AVAILABLE` → Peers exchange `REQUEST_DELTA`/`DELTA_DATA` → DeltaEngine patches and updates ignore lists.

Threading strategy keeps background workers (RTT, status, IPC, plugin-specific watchers) without busy loops, relying on `std::thread` + `std::mutex` + `std::lock_guard`.

---

## 📡 IPC + CLI Commands

- `STATUS_JSON`: snapshot of daemon, mesh state, and peer health.
- `METRICS_JSON`: bandwidth/upload/download statistics.
- `PEERS_JSON`: active peer list.
- `FILES_JSON`: watched files and metadata.
- `CONFIG_JSON`: current configuration values.
- `SET_CONFIG key=value`: update runtime config atomically.
- `DISCOVER`: trigger discovery round.
- `PAUSE` / `RESUME`: control synchronization loop.
- GUI listens for `daemon-status`, `daemon-log`, `daemon-data` events and updates React state accordingly (`App.tsx`).

---

## 🧪 Testing

```bash
cmake --build build --target tests
ctest --output-on-failure

./tests/discovery_test
./tests/delta_test
./tests/storage_test
```

---

## 📚 Additional Notes

- Database schema (SQLite) includes `device`, `session`, `files`, `file_version`, `sync_queue`, `peers`, `watched_folders`, and `file_access_log`. All metadata changes are wrapped in atomic transactions.
- The ML subsystem is gated behind a build flag; enable it only when ONNX Runtime is available.
- Keep new classes scoped to `sfs::core`, `sfs::net`, `sfs::delta`, `sfs::fs`, `sfs::db`, or `sfs::ml` namespaces per coding guidelines.