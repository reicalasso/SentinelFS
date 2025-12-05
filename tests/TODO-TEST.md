# Missing Tests

## Core Components

### Network
- [x] `AutoRemeshManager`: Test network topology changes management.
- [ ] `BandwidthLimiter`: Test bandwidth limiting logic.
- [ ] `DeltaEngine`: Test delta calculation engine.
- [ ] `DeltaSync`: Test delta synchronization.
- [ ] `NetworkManager`: Test network management.

### Security
- [x] `SessionCode`: Test session code generation and validation.
- [ ] `TLSContext`: Test TLS/SSL context creation and configuration.
- [ ] `Crypto`: Test cryptographic operations.
### Sync
- [x] `DeltaSerialization`: Test serialization/deserialization of delta updates.
- [ ] `DeltaSyncProtocolHandler`: Test delta sync protocol logic.
- [ ] `EventHandlers`: Test event handlers triggering.
- [ ] `FileSyncHandler`: Unit test for file sync logic.
- [ ] `ConflictResolver`: Test conflict resolution logic.
- [ ] `OfflineQueue`: Test offline queue management.col logic.
- [ ] `EventHandlers`: Test event handlers triggering.
### Utils
- [ ] `HealthEndpoint`: Test health check endpoint.
- [ ] `Compression`: Test compression utilities.
- [ ] `Config`: Test configuration loading and parsing.
- [ ] `EventBus`: Test event bus publish/subscribe.
- [ ] `Logger`: Test logging functionality.
- [ ] `MetricsCollector`: Test metrics collection.
### Filesystem
- [ ] `FSEventsWatcher` / `InotifyWatcher`: Platform-specific watcher tests.
- [ ] `FileHasher`: Test file hashing.
- [ ] `Win32Watcher`: Test Windows specific watcher (if applicable).
- [ ] `PluginManager`: Test plugin management.
- [ ] `SHA256`: Test SHA256 hashing.
- [ ] `ThreadPool`: Test thread pool execution.
### Utils
- [ ] `HealthEndpoint`: Test health check endpoint.

## Plugins

### Filesystem
- [ ] `FSEventsWatcher` / `InotifyWatcher`: Platform-specific watcher tests.

### ML
- [x] `AnomalyDetector`: Test anomaly detection algorithm.

### Network Plugin
- [ ] `HandshakeProtocol`: Test handshake protocol.
- [ ] `TCPHandler` / `TCPRelay`: Test TCP connection and relay.
### Storage Plugin
- [ ] `SQLiteHandler`: Unit tests for database operations.
- [ ] `DeviceManager`: Test device management.
- [ ] `PeerManager`: Test peer management.
- [ ] `SessionManager`: Test session management.
- [ ] `FileMetadataManager`: Test file metadata management.
- [ ] `FileAccessLogManager`: Test file access logs.
- [ ] `SyncQueueManager`: Test sync queue management.
- [ ] `ConflictManager`: Test conflict management in storage.ement.
- [ ] `FileAccessLogManager`: Test file access logs.
- [ ] `SyncQueueManager`: Test sync queue management.
