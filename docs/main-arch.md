---
title: "SentinelFS Main Architecture Diagram"
description: "Visual representation of the SentinelFS architecture with detailed component explanations"
date: "29.09.2025"
---

# 🏗️ SentinelFS Main Architecture

## 📋 Table of Contents
- [Architecture Diagram](#architecture-diagram)
- [Component Descriptions](#component-descriptions)
- [Data Flow](#data-flow)
- [Security Layer](#security-layer)

## Architecture Diagram

```mermaid
graph TB
    classDef client fill:#E0F2FE,stroke:#0284C7,stroke-width:1px,color:#0F172A,font-weight:bold;
    classDef fuse fill:#FEF3C7,stroke:#D97706,stroke-width:1px,color:#7C2D12,font-weight:bold;
    classDef core fill:#EDE9FE,stroke:#7C3AED,stroke-width:1px,color:#4C1D95,font-weight:bold;
    classDef infra fill:#DCFCE7,stroke:#16A34A,stroke-width:1px,color:#064E3B,font-weight:bold;
    classDef storage fill:#FFE4E6,stroke:#F43F5E,stroke-width:1px,color:#7F1D1D,font-weight:bold;
    classDef quarantine fill:#FEE2E2,stroke:#EF4444,stroke-width:1px,color:#7F1D1D,font-weight:bold;
    classDef shared fill:#F8FAFC,stroke:#4B5563,stroke-width:1px,color:#111827,font-weight:bold;

    subgraph "Client Layer"
        direction LR
        APP[Applications]
        CLI[Command Line Tools]
        API_CLIENT[API Clients]
    end

    subgraph "FUSE Layer"
        direction LR
        FUSE_DRIVER[FUSE Driver]
        CACHE[Cache Manager]
        VFS[VFS Abstraction]
    end

    subgraph "Core Services"
        direction LR
        SEC[Security Engine]
        AI[AI Analyzer]
        NET[Network Manager]
        DB[(Audit Database)]
    end

    subgraph "Infrastructure"
        direction LR
        API[REST API]
        RBAC[RBAC Engine]
        MON[Prometheus]
        GRAF[Grafana]
    end

    subgraph "Storage Nodes"
        direction LR
        NODE1["Node 1<br/>(Metadata)"]
        NODE2["Node 2<br/>(Data)"]
        NODE3["Node 3<br/>(Replica)"]
    end

    subgraph "Quarantine"
        UF[Unsafe Files]
    end

    APP -->|POSIX I/O| FUSE_DRIVER
    CLI -->|Admin Ops| API
    API_CLIENT -->|Management Calls| API

    FUSE_DRIVER -->|Read/Write Caching| CACHE
    FUSE_DRIVER -->|Virtual FS Mapping| VFS
    CACHE -->|Metadata Hints| SEC
    CACHE -.->|Anomaly Signals| AI
    VFS -->|Access Check| SEC

    SEC -->|Behavioral Request| AI
    AI -->|Risk Score| SEC
    SEC -->|Routing Decision| NET
    SEC -->|Audit Events| DB
    SEC -->|Policy Check| RBAC
    SEC -->|Quarantine Directive| UF

    AI -->|Model Telemetry| DB
    AI -->|Feedback Loop| NET

    NET -->|Metadata Ops| NODE1
    NET -->|Primary Data| NODE2
    NET -->|Replica Sync| NODE3

    API -->|Policy Updates| SEC
    API -->|Role Management| RBAC
    API -->|Audit Queries| DB
    API -->|Quarantine Review| UF

    RBAC -->|Auth Context| SEC
    RBAC -->|Access Grants| API
    RBAC -->|Policy Store| DB
    DB -->|Policy Retrieval| RBAC

    SEC -->|Metrics| MON
    AI -->|Metrics| MON
    NET -->|Metrics| MON
    API -.->|Health Checks| MON

    MON -->|Dashboards| GRAF
    MON -->|Alert Hooks| API
    API -->|Dashboard Embeds| GRAF

    UF -->|Release Workflow| API

    class APP,CLI,API_CLIENT client;
    class FUSE_DRIVER,CACHE,VFS fuse;
    class SEC,AI,NET core;
    class DB shared;
    class API,RBAC,MON,GRAF infra;
    class NODE1,NODE2,NODE3 storage;
    class UF quarantine;
```

## Component Descriptions

### Client Layer
- **Applications**: Standard applications that access the file system through POSIX-compliant operations
- **Command Line Tools**: Administrative tools for system management
- **API Clients**: Applications that interact with the REST API for advanced operations

### FUSE Layer
- **FUSE Driver**: Implements the FUSE protocol to interface with the Linux kernel
- **Cache Manager**: Handles read/write caching to improve performance
- **VFS Abstraction**: Provides virtual file system abstractions

### Core Services
- **Security Engine**: Performs real-time threat detection and access control
- **AI Analyzer**: Applies machine learning models for behavioral analysis
- **Network Manager**: Manages node discovery, routing, and health monitoring
- **Audit Database**: Stores immutable audit logs and security events

### Infrastructure
- **REST API**: Provides administrative and operational interfaces
- **RBAC Engine**: Manages role-based access control
- **Prometheus**: Collects and stores metrics
- **Grafana**: Provides visualization dashboards

### Storage Nodes
- **Metadata Node (Node 1)**: Manages file metadata and directory structure
- **Data Node (Node 2)**: Stores actual file data
- **Replica Node (Node 3)**: Maintains replicated data for high availability

### Quarantine
- **Unsafe Files**: Isolated storage for files flagged as potentially malicious

## Data Flow

The diagram illustrates the flow of data and operations through the system:

1. **Client Applications** interact with the system through standard POSIX operations
2. **FUSE Driver** handles the translation between POSIX operations and system services
3. **Cache Manager** optimizes read/write operations
4. **Security Engine** performs real-time threat detection before data reaches storage
5. **AI Analyzer** provides behavioral analysis to detect anomalies
6. **Network Manager** routes operations to appropriate storage nodes
7. **Storage Nodes** provide distributed storage with replication
8. **Audit Database** maintains immutable logs of all operations

## Security Layer

The architecture includes multiple security layers:

- **Security Engine** performs real-time scanning using YARA rules and entropy analysis
- **AI Analyzer** detects behavioral anomalies that might indicate security threats
- **RBAC Engine** enforces access controls based on user roles and permissions
- **Quarantine System** isolates potentially malicious files
- **Audit Database** maintains immutable logs for compliance and forensic analysis

This architecture ensures that security is integrated at every level of the system rather than being an afterthought, creating a zero-trust environment where all operations are verified and logged.

_last updated 01.10.2025_