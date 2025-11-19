# SentinelFS-Neo Development Roadmap

This document outlines the 6-month development plan for SentinelFS-Neo, structured into 2-week sprints.

## Phase 1: Core Infrastructure & File System (Months 1-2)
*Goal: A prototype that watches the local file system, maintains metadata, and runs basic CLI commands.*

### Sprint 1 (Weeks 1-2): Project Setup & Core Architecture [Completed]
- [x] Create project directory structure.
- [x] Define `IPlugin`, `IFileAPI`, `INetworkAPI` interfaces.
- [x] Implement `PluginLoader` and `EventBus`.
- [x] Add `Logger` module (RQ-UI.F-001).
- [x] Add `Config` manager (JSON/YAML).

### Sprint 2 (Weeks 3-4): Storage Layer [Completed - Product Level]
- [x] **Task:** Integrate SQLite into `StoragePlugin` (RQ-DB.F-002).
- [x] **Task:** Design database schema for files, peers, and config (RQ-DB.F-003, RQ-DB.F-005).
- [x] **Task:** Implement basic CRUD operations (Create/Read/Update/Delete).
- [x] **Optimization:** Refactor to use Prepared Statements (`sqlite3_prepare_v2`) for security (SQL Injection prevention).

### Sprint 3 (Weeks 5-6): File System Monitoring [Completed]
- [x] **Task:** Integrate `inotify` (Linux) into `FilesystemPlugin` (RQ-UI.F-002).
- [x] **Task:** Capture file changes and publish via `EventBus` (RQ-F.F-010).
- [x] **Task:** Implement file hashing (SHA-256) (RQ-F.F-009).

### Sprint 4 (Weeks 7-8): Delta Engine [Completed]
- [x] **Task:** Implement block splitting and Rolling Checksum (Adler32) (RQ-F.F-004, RQ-F.F-012).
- [x] **Task:** Implement delta calculation algorithm.

## Phase 2: Network & P2P Communication (Months 3-4)
*Goal: Devices can discover each other and transfer data.*

### Sprint 5 (Weeks 9-10): Peer Discovery [Completed]
- [x] **Task:** Implement UDP Broadcast in `NetworkPlugin` (RQ-F.F-011).
- [ ] **Task:** Implement "Session Code" validation (RQ-F.F-002).
- [x] **Task:** Cache discovered peers in `StoragePlugin` (RQ-DB.F-011).

### Sprint 6 (Weeks 11-12): Connection & Data Transfer [In Progress]
- [x] **Task:** Establish reliable TCP transfer infrastructure.
- [x] **Task:** Implement Shared-Key handshake (RQ-F.F-008).
- [x] **Task:** Implement Daemon logic to coordinate plugins.
- [ ] **Task:** Transfer and reassemble delta blocks.

### Sprint 7 (Weeks 13-14): Auto-Remesh Algorithm (v1)
- [ ] **Task:** Implement RTT (Ping) measurement (RQ-F.F-013).
- [ ] **Task:** Implement dynamic peer selection based on latency (RQ-F.F-003).
- [ ] **Task:** Handle topology changes automatically.

## Phase 3: Intelligence & Optimization (Months 5-6)
*Goal: Add intelligence, security, and final polish.*

### Sprint 8 (Weeks 15-16): ML Integration
- [ ] **Task:** Integrate ONNX Runtime into `MLPlugin` (RQ-NF.F-005).
- [ ] **Task:** Train and convert anomaly detection model (e.g., Isolation Forest).
- [ ] **Task:** Feed file access events to the model (RQ-F.F-005).

### Sprint 9 (Weeks 17-18): Security & Response
- [ ] **Task:** Stop sync on ransomware-like activity (RQ-F.F-006).
- [ ] **Task:** Trigger alarms/logs on anomalies (RQ-F.F-014).

### Sprint 10 (Weeks 19-20): CLI & Integration
- [ ] **Task:** Enhance CLI (`status`, `peers`, `logs`, `config`) (RQ-UI.F-001).
- [ ] **Task:** Improve Daemon-CLI IPC.

### Sprint 11 (Weeks 21-22): Testing & Documentation
- [ ] **Task:** End-to-End synchronization tests.
- [ ] **Task:** Performance testing.
- [ ] **Task:** Complete documentation.

### Sprint 12 (Weeks 23-24): Final Release
- [ ] **Task:** Bug fixes and stability improvements.
- [ ] **Task:** Final presentation preparation.
