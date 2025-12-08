# IronRoot - Advanced Filesystem Monitoring Plugin

IronRoot is a next-generation filesystem monitoring plugin for SentinelFS that provides advanced features beyond basic file watching.

## Features

### 🔍 Process-Aware Monitoring (fanotify)
- Know which process modified a file
- Get PID and process name for each event
- Requires `CAP_SYS_ADMIN` capability
- Automatic fallback to inotify

### ⚡ Event Debouncing
- Coalesce rapid successive modifications
- Configurable debounce window (default: 100ms)
- Maximum delay guarantee (default: 500ms)
- Reduces unnecessary sync operations

### 🔄 Atomic Write Detection
- Detect temp file → rename patterns
- Properly handle vim, emacs, and other editors
- Single event for atomic saves
- Patterns: `.file.tmp`, `file~`, `#file#`

### 📁 Extended Attributes (xattr)
- Read/write extended attributes
- Track xattr changes
- Sync metadata across peers

### 🔒 File Locking
- Advisory file locks
- Exclusive and shared modes
- Prevent concurrent modifications

### 💾 Memory-Mapped I/O
- Efficient large file handling
- Reduced memory copies
- Sequential access optimization

### 🔗 Symlink Support
- Detect symbolic links
- Read link targets
- Optional follow mode

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    IronRootPlugin                        │
├─────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐ │
│  │  Watcher    │  │  Debouncer  │  │    FileOps      │ │
│  │ (fanotify/  │→ │  (coalesce  │→ │  (read/write/   │ │
│  │  inotify)   │  │   events)   │  │   xattr/lock)   │ │
│  └─────────────┘  └─────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

## Usage

### Basic Usage
```cpp
#include "IronRootPlugin.h"

auto plugin = std::make_unique<IronRoot::IronRootPlugin>();
plugin->initialize(eventBus);
plugin->startWatching("/path/to/watch");
```

### Custom Configuration
```cpp
IronRoot::WatchConfig config;
config.recursive = true;
config.useFanotify = true;
config.debounce.window = std::chrono::milliseconds(200);
config.debounce.detectAtomicWrites = true;
config.ignorePatterns = {"*.log", "*.tmp", "node_modules/"};

plugin->setWatchConfig(config);
```

### Extended Attributes
```cpp
// Set custom metadata
plugin->setXattr("/path/file", "user.sentinel.synced", "true");

// Get all xattrs
auto xattrs = plugin->getXattrs("/path/file");
```

### File Locking
```cpp
// Lock file for exclusive access
if (plugin->lockFile("/path/file", true)) {
    // ... modify file ...
    plugin->unlockFile("/path/file");
}
```

### Atomic Writes
```cpp
// Write atomically (temp file + rename)
std::vector<uint8_t> data = ...;
plugin->writeFileAtomic("/path/file", data);
```

## Events

IronRoot publishes the following events to EventBus:

| Event | Description |
|-------|-------------|
| `FILE_CREATED` | New file created |
| `FILE_MODIFIED` | File content changed |
| `FILE_DELETED` | File removed |
| `FILE_RENAMED` | File moved/renamed |
| `FILE_ATTRIB_CHANGED` | Permissions/xattrs changed |

## Statistics

```cpp
auto stats = plugin->getStats();
std::cout << "Events processed: " << stats.eventsProcessed << std::endl;
std::cout << "Events debounced: " << stats.eventsDebounced << std::endl;
std::cout << "Atomic writes: " << stats.atomicWritesDetected << std::endl;
std::cout << "Dirs watched: " << stats.dirsWatched << std::endl;
```

## Requirements

- Linux kernel 2.6.37+ (fanotify)
- Linux kernel 2.6.13+ (inotify fallback)
- OpenSSL (for SHA-256 hashing)
- C++17 compiler

## Capabilities

For fanotify support (process info), run with:
```bash
sudo setcap cap_sys_admin+ep /path/to/sentinel_daemon
```

Or run as root. Without this capability, IronRoot falls back to inotify.

## Comparison with Old Plugin

| Feature | Old (filesystem) | IronRoot |
|---------|------------------|----------|
| File watching | ✅ inotify | ✅ fanotify + inotify |
| Process info | ❌ | ✅ |
| Debouncing | ❌ | ✅ |
| Atomic write detection | ❌ | ✅ |
| Extended attributes | ❌ | ✅ |
| File locking | ❌ | ✅ |
| Memory-mapped I/O | ❌ | ✅ |
| Batch processing | ❌ | ✅ |

## License

Part of SentinelFS - MIT License
