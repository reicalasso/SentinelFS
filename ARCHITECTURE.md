# SentinelFS Daemon Architecture (Post-Refactoring)

```
┌─────────────────────────────────────────────────────────────────┐
│                    sentinel_daemon.cpp (main)                    │
│  • CLI argument parsing                                          │
│  • Configuration validation                                      │
│  • Component initialization                                      │
│  • Background threads coordination                               │
│  • Signal handling & cleanup                                     │
└────────────────────┬────────────────────────────────────────────┘
                     │
         ┌───────────┼───────────────────┐
         │           │                   │
         ▼           ▼                   ▼
┌────────────┐ ┌────────────┐   ┌──────────────┐
│ DaemonCore │ │IPCHandler  │   │EventHandlers │
│            │ │            │   │              │
│ • Plugin   │ │ • Unix     │   │ • Event      │
│   loading  │ │   socket   │   │   subscript  │
│ • Init     │ │ • Command  │   │ • Protocol   │
│ • Run loop │ │   routing  │   │   handling   │
│ • Shutdown │ │ • STATUS   │   │ • Delta sync │
│            │ │ • PAUSE    │   │ • Discovery  │
└─────┬──────┘ │ • RESUME   │   └──────┬───────┘
      │        │ • CONNECT  │          │
      │        └────────────┘          │
      │                                │
      │  ┌─────────────────────────────┘
      │  │
      ▼  ▼
┌──────────────────────────────────────────────┐
│              EventBus (Core)                 │
│  • Pub/Sub event system                      │
│  • Decouples components                      │
└───────┬──────────────────────────────────────┘
        │
        │  Publishes events to:
        │  • PEER_DISCOVERED
        │  • FILE_MODIFIED
        │  • DATA_RECEIVED
        │  • ANOMALY_DETECTED
        │
        ▼
┌───────────────────────────────────────────────┐
│            PluginLoader (Core)                │
│  • Dynamic .so loading                        │
│  • Interface casting                          │
└───────┬───────────────────────────────────────┘
        │
        │  Loads plugins:
        │
        ├─────────────┬──────────────┬─────────────┐
        ▼             ▼              ▼             ▼
  ┌────────────┐  ┌───────────┐  ┌──────────┐  ┌──────────┐
  │ Storage    │  │ Network   │  │Filesystem│  │    ML    │
  │  Plugin    │  │  Plugin   │  │  Plugin  │  │  Plugin  │
  └────────────┘  └───────────┘  └──────────┘  └──────────┘
  │IStorageAPI │  │INetworkAPI│  │IFileAPI  │  │IPlugin   │
  └────────────┘  └───────────┘  └──────────┘  └──────────┘

```

## Component Interaction Flow

### Startup Sequence
```
1. main() parses CLI arguments → DaemonConfig
2. DaemonCore(config).initialize()
   ├─ Load plugins via PluginLoader
   ├─ Configure network (session code, encryption)
   └─ Start network services & filesystem watching
3. EventHandlers(eventBus, plugins).setupHandlers()
   └─ Subscribe to all event types
4. IPCHandler(socket, plugins).start()
   └─ Listen for CLI commands
5. Start background threads (RTT, status display)
6. DaemonCore.run() - main event loop
```

### File Modification Flow
```
Filesystem Plugin detects change
    │
    ├─ Publishes "FILE_MODIFIED" event
    │
    ▼
EventHandlers receives event
    │
    ├─ Check ignore list (prevent loops)
    ├─ Check if sync enabled
    │
    ├─ Broadcast "UPDATE_AVAILABLE" to peers
    │
    ▼
Network Plugin sends notification
```

### Peer Discovery Flow
```
Network Plugin receives UDP broadcast
    │
    ├─ Verify session code
    ├─ Publishes "PEER_DISCOVERED" event
    │
    ▼
EventHandlers receives event
    │
    ├─ Parse peer info (ID, IP, PORT)
    ├─ Add to storage plugin
    │
    ├─ Auto-connect via network plugin
    │
    ▼
Peer added to active connection pool
```

### IPC Command Flow
```
CLI sends command → Unix socket
    │
    ▼
IPCHandler receives command
    │
    ├─ Parse command type
    │
    ├─ Route to handler:
    │   ├─ STATUS → Query plugins
    │   ├─ PAUSE → Callback to EventHandlers
    │   ├─ RESUME → Callback to EventHandlers
    │   ├─ CONNECT → Network plugin
    │   └─ PEERS → Storage plugin
    │
    ├─ Format response
    │
    ▼
Send response back to CLI
```

### Delta Sync Protocol Flow
```
Peer A: File modified
    │
    ├─ EVENT: FILE_MODIFIED
    │
    ▼
    Send: UPDATE_AVAILABLE|filename
    
Peer B: Receives notification
    │
    ├─ Calculate signature of local file
    │
    ▼
    Send: REQUEST_DELTA|filename|<signatures>
    
Peer A: Receives request
    │
    ├─ Calculate delta using DeltaEngine
    │
    ▼
    Send: DELTA_DATA|filename|<instructions>
    
Peer B: Receives delta
    │
    ├─ Apply delta using DeltaEngine
    ├─ Add to ignore list (prevent loop)
    │
    ▼
    Write patched file
```

## Thread Architecture

```
┌────────────────────────────────────────────────────┐
│                   Main Thread                      │
│  • DaemonCore.run() - signal handling              │
│  • Blocks until shutdown signal                    │
└────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────┐
│                   IPC Thread                       │
│  • IPCHandler server loop                          │
│  • Accept connections, process commands            │
│  • Thread-safe via EventBus/Plugin interfaces      │
└────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────┐
│                   RTT Thread                       │
│  • Periodic latency measurement (15s interval)     │
│  • Update peer latencies in storage                │
│  • Reconnect disconnected peers                    │
└────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────┐
│                  Status Thread                     │
│  • Broadcast presence (5s interval)                │
│  • Display peer status (30s interval)              │
│  • Non-blocking status updates                     │
└────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────┐
│             Plugin Internal Threads                │
│  • Network: TCP server, discovery listener         │
│  • Filesystem: inotify watcher                     │
│  • Each plugin manages own concurrency             │
└────────────────────────────────────────────────────┘
```

## Data Flow Summary

```
User Input (CLI) → IPCHandler → Plugin APIs → Action
     ↓
User Query (STATUS) → IPCHandler → Plugin State → Response

File Change → Filesystem Plugin → EventBus → EventHandlers → Network Plugin
                                     ↓
                            Storage Plugin (logging)

Network Data → Network Plugin → EventBus → EventHandlers → DeltaEngine → Filesystem
                                    ↓
                           Storage Plugin (peer tracking)

Anomaly → ML Plugin → EventBus → EventHandlers → Disable Sync
```

## Key Design Patterns Used

1. **Dependency Injection**: Plugins injected into handlers
2. **Observer Pattern**: EventBus pub/sub
3. **Strategy Pattern**: Conflict resolution strategies
4. **Facade Pattern**: DaemonCore hides complexity
5. **Command Pattern**: IPC command routing
6. **Factory Pattern**: PluginLoader creates instances
