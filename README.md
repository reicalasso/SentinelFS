# SentinelFS: AI-Powered Distributed Security File System

**"Siber Tehdit Farkındalıklı, Ağ Koşullarına Uyarlanabilir, Yapay Zekâ Destekli Dağıtık Güvenlik Dosya Sistemi"**

## 🎯 Project Overview

SentinelFS is a distributed file system that integrates:
- **Real-time cyber threat detection**
- **Network-aware data replication** 
- **AI-powered behavioral anomaly detection**
- **Database-driven access control and audit logging**

Unlike traditional file systems, SentinelFS operates as a FUSE-mounted distributed file system (e.g., `/sentinel/data`).

## 🏗️ Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   User Space    │    │    AI Scoring    │    │   Dashboard     │
│ /sentinel/data  │◄──►│   & Decision     │◄──►│   & Alerts      │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                        │                        │
         ▼                        ▼                        ▼
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│ sentinel-fuse   │    │ sentinel-security│    │ sentinel-api    │
│ FUSE Driver     │◄──►│ Content Scanner  │◄──►│ REST API        │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                        │                        │
         ▼                        ▼                        ▼
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│ sentinel-net    │    │ sentinel-db      │    │ sentinel-common │
│ Network Aware   │◄──►│ Audit & Policy   │◄──►│ Shared Types    │
│ Replication     │    │ Store            │    │ & Utilities     │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

## 📦 Modules

### Core Modules

| Module | Responsibility | Key Features |
|--------|---------------|--------------|
| **sentinel-fuse** | FUSE file system interface | Mount point, VFS operations, I/O hooks |
| **sentinel-security** | Content scanning & policy enforcement | YARA rules, entropy analysis, AES-256 encryption |
| **sentinel-ai** | Behavioral anomaly detection | Feature extraction, ML scoring, alert generation |
| **sentinel-net** | Network-aware replication | Node discovery, latency measurement, smart placement |
| **sentinel-db** | Audit logging & policy management | PostgreSQL backend, RLS, event tracking |
| **sentinel-api** | Admin interface & orchestration | REST API, dashboard backend, override controls |
| **sentinel-common** | Shared utilities | Common types, error handling, configuration |

### Infrastructure
- **infra/**: Docker Compose setup, Kubernetes manifests, CI/CD
- **scripts/**: Development tools, attack simulators, benchmarks
- **docs/**: Technical documentation, API references

## 🚀 Quick Start

### Prerequisites
- Rust 1.70+
- Docker & Docker Compose
- PostgreSQL 15+
- Python 3.9+ (for AI components)

### Development Setup(The project has not been started yet :( . You can skip this part)

```bash
# Clone repository
git clone <repo-url>
cd network-ai-manager

# Build all modules
cargo build --workspace

# Start development environment
docker-compose -f infra/docker-compose.dev.yml up -d

# Run database migrations
cargo run --bin sentinel-db -- migrate

# Mount FUSE filesystem (requires sudo)
sudo ./target/debug/sentinel-fused --mount-point /sentinel

# Run tests
cargo test --workspace
```

### Demo Environment

```bash
# Start 3-node demo cluster
docker-compose -f infra/docker-compose.demo.yml up -d

# Access dashboard
open http://localhost:3000

# Run attack simulations
./scripts/simulate-ransomware.sh
./scripts/simulate-data-exfiltration.sh
```

## 🔧 Configuration

Each module has its own configuration file:

- `sentinel-fuse/config.toml` - FUSE mount options, cache settings
- `sentinel-security/config.toml` - YARA rules, content scanning policies  
- `sentinel-ai/config.toml` - Model endpoints, scoring thresholds
- `sentinel-net/config.toml` - Network topology, replication policies
- `sentinel-db/config.toml` - Database connection, audit settings

## 📊 Metrics & Monitoring

SentinelFS exports Prometheus metrics for:
- **Performance**: I/O latency (p99), throughput (ops/s)
- **Security**: Threat detection rate, false positives  
- **AI**: Model accuracy, anomaly scores
- **Network**: Replication cost, node availability

Access Grafana dashboard at `http://localhost:3001`

## 🧪 Testing & Validation

### Attack Scenarios
1. **Ransomware Detection**: Rapid file encryption patterns
2. **Unauthorized Access**: Credential stuffing, privilege escalation
3. **Data Exfiltration**: Large sequential read patterns

### Performance Benchmarks
- **Baseline**: Traditional NFS vs SentinelFS
- **Target**: <35% latency overhead, 60%+ threat detection accuracy
- **Metrics**: MTTR (Mean Time To Response) for isolation

## 🛡️ Security Considerations

- **Encryption**: AES-256-GCM for data at rest and in transit
- **Authentication**: JWT + MFA for admin operations
- **Authorization**: Role-based access control (RBAC) + Row-level security (RLS)
- **Audit**: Immutable append-only logs with integrity verification
- **Fail-Safe**: Configurable fail-open/fail-closed behavior

## 📈 Performance Targets

| Metric | Target | Measurement |
|--------|---------|-------------|
| I/O Latency (p99) | <25% overhead vs baseline | fio benchmarks |
| Threat Detection | >90% accuracy | Attack simulation suite |
| False Positive Rate | <5% | Normal workload testing |
| Network Efficiency | 25% faster avg access time | Multi-node deployment |
| MTTR | <30 seconds | Incident response testing |

## 🔄 Development Workflow

### Branch Strategy
- `main` - Stable releases
- `develop` - Integration branch
- `feature/*` - Feature development
- `hotfix/*` - Critical fixes

### Testing Pipeline
1. **Unit Tests**: `cargo test --workspace`
2. **Integration Tests**: Docker-based multi-service tests
3. **Security Tests**: YARA rule validation, RLS bypass attempts
4. **Performance Tests**: k6 load testing + fio I/O benchmarks
5. **Chaos Tests**: Network partitions, node failures

### Code Quality
- **Formatting**: `cargo fmt`
- **Linting**: `cargo clippy`  
- **Security**: `cargo audit`
- **Coverage**: `cargo tarpaulin`

## 📋 Deliverables (YMH345 Compliance)

- ✅ Complete source code for all modules
- ✅ Docker Compose demo environment (3-node cluster)
- ✅ Attack scenario test reports (ransomware, unauthorized access, data exfiltration)
- ✅ AI model performance metrics (precision, recall, F1-score)
- ✅ Performance comparison report (NFS vs SentinelFS)
- ✅ Technical documentation and architecture guide
- ✅ 10-15 minute demo video
- ✅ Presentation slides (PDF)

## 🤝 Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines.

## 📄 License

This project is licensed under the MIT License - see [LICENSE](LICENSE) file for details.

---

**Status**: 🚧 Active Development  
**Team**: Network Security & AI Integration  
**Contact**: [mehmetardahakbilen2005@gmail.com]