# Changelog

All notable changes to SentinelFS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-01-XX

### Added

#### Core Features
- **P2P File Synchronization**: Decentralized peer-to-peer file sync without central servers
- **Delta Sync Engine**: Bandwidth-efficient binary diff/patch using Adler32 and SHA-256
- **End-to-End Encryption**: AES-256-CBC encryption for all file transfers
- **Session-Based Authentication**: Secure peer discovery and authentication using session codes
- **Auto-Remesh Network**: Intelligent network topology optimization based on RTT and jitter

#### Plugins
- **IronRoot Plugin**: Advanced filesystem monitoring with inotify/FSEvents support
- **NetFalcon Plugin**: Multi-transport network layer (TCP, with QUIC/WebRTC planned)
- **FalconStore Plugin**: High-performance SQLite-based storage with WAL mode
- **Zer0 Plugin**: ML-based threat detection using ONNX Runtime

#### Security
- **Real-Time Threat Detection**: Anomaly detection for ransomware and malicious patterns
- **Automatic Quarantine**: Suspicious files isolated automatically
- **Integrity Verification**: SHA-256 checksums for all transfers
- **Zero-Knowledge Architecture**: No central server stores user data

#### User Interface
- **Electron Desktop GUI**: Modern React-based desktop application
- **Real-Time Dashboard**: Live system metrics and peer status visualization
- **File Browser**: Intuitive file management interface
- **Peer Management**: View and manage connected peers
- **Activity Logs**: Comprehensive audit trail of all operations

#### CLI Tools
- **sentinel_daemon**: Main background service
- **sentinel_cli**: Command-line interface for daemon control
- Status monitoring, peer management, and system control commands

#### Monitoring & Observability
- **Prometheus Metrics**: Export system metrics for monitoring
- **Grafana Dashboards**: Pre-built visualization templates
- **Health Endpoints**: HTTP health check endpoints
- **Structured Logging**: JSON-formatted logs for analysis

#### Build & Deployment
- **CMake Build System**: Cross-platform build configuration
- **AppImage Support**: Self-contained Linux application packages
- **Debian Packages**: Native .deb packages for Debian/Ubuntu
- **Systemd Integration**: System service management
- **Docker Support**: Containerized deployment (planned)

#### Documentation
- Comprehensive README with installation and usage instructions
- Architecture documentation with system design details
- Database schema documentation with ER diagrams
- Production deployment guide
- Monitoring and observability guide
- AppImage build instructions
- Security and threat detection documentation

#### Testing
- 30+ unit tests covering core functionality
- Integration tests for end-to-end scenarios
- Delta sync engine tests
- Network protocol tests
- Storage layer tests
- Cryptography tests

### Changed
- Migrated from legacy plugin system to modern plugin architecture
- Improved error handling and logging throughout codebase
- Enhanced configuration management with validation
- Optimized database queries with proper indexing

### Security
- Implemented secure session code generation
- Added encryption key rotation support
- Enhanced input validation to prevent injection attacks
- Implemented rate limiting for network operations

### Performance
- Optimized delta sync algorithm for large files
- Reduced memory footprint of daemon process
- Improved concurrent file transfer handling
- Enhanced database performance with WAL mode

---

## [Unreleased]

### Planned Features
- **WebRTC Transport**: Direct peer connections with NAT traversal
- **QUIC Protocol**: Low-latency transport for improved performance
- **Mobile Apps**: iOS and Android clients
- **Web Interface**: Browser-based management console
- **Cloud Backup**: Optional encrypted cloud backup integration
- **Selective Sync**: Choose which folders to sync per device
- **Bandwidth Scheduling**: Time-based bandwidth allocation
- **Conflict Resolution UI**: Interactive conflict resolution interface
- **Plugin Marketplace**: Community-contributed plugins
- **Multi-Language Support**: Internationalization (i18n)

### Known Issues
- Large file transfers (>10GB) may experience memory pressure
- Windows support is experimental and not fully tested
- GUI performance degrades with >10,000 files in view
- Network discovery may fail on some restrictive networks

---

## Version History

### Version Numbering

SentinelFS follows [Semantic Versioning](https://semver.org/):
- **MAJOR**: Incompatible API changes
- **MINOR**: Backward-compatible functionality additions
- **PATCH**: Backward-compatible bug fixes

### Release Cycle

- **Major releases**: Annually or when breaking changes are necessary
- **Minor releases**: Quarterly with new features
- **Patch releases**: As needed for critical bug fixes

---

## Migration Guides

### Upgrading to 1.0.0

This is the initial stable release. No migration needed.

---

## Contributors

Thank you to all contributors who have helped build SentinelFS!

See [CONTRIBUTING.md](CONTRIBUTING.md) for information on how to contribute.

---

## License

SentinelFS is licensed under the SentinelFS Public License (SPL-1.0).
See [LICENSE](LICENSE) for details.
