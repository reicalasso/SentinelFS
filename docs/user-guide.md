# 📖 SentinelFS User Guide

Welcome to the SentinelFS User Guide! This document will help you install, configure, and use SentinelFS in your environment.

## 🚀 Installation

### Prerequisites

Before installing SentinelFS, ensure you have:

- Linux system (Ubuntu 20.04+, CentOS 8+, or equivalent)
- Root/sudo access for FUSE mounting
- Docker and Docker Compose (for infrastructure services)
- Rust toolchain (for building from source)
- PostgreSQL client

### Quick Install

```bash
# Clone the repository
git clone https://github.com/reicalasso/SentinelFS.git
cd SentinelFS

# Install dependencies
sudo apt update && sudo apt install -y \
    curl build-essential pkg-config libssl-dev \
    libfuse-dev postgresql-client docker-compose

# Install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source ~/.cargo/env

# Build the project
cargo build --workspace --release
```

### Production Installation

For production environments, it's recommended to:

1. Use systemd for service management
2. Configure TLS for secure communication
3. Set up a reverse proxy for API access

#### Systemd Service Configuration

Create `/etc/systemd/system/sentinelfs.service`:

```ini
[Unit]
Description=SentinelFS Distributed File System
After=network.target
Wants=network.target

[Service]
Type=forking
User=root
Group=root
ExecStart=/opt/sentinelfs/bin/sentinel-fused --config /etc/sentinelfs/config.toml
PIDFile=/var/run/sentinelfs.pid
Restart=always
RestartSec=5
LimitNOFILE=65536

# Security settings
NoNewPrivileges=true
ReadWritePaths=/sentinel /tmp
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
PrivateDevices=true

[Install]
WantedBy=multi-user.target
```

Enable the service:

```bash
sudo systemctl daemon-reload
sudo systemctl enable sentinelfs
sudo systemctl start sentinelfs
```

#### TLS Configuration

To secure communications between nodes, configure TLS in your `config.toml`:

```toml
[security]
tls_enabled = true
cert_path = "/etc/sentinelfs/tls/cert.pem"
key_path = "/etc/sentinelfs/tls/key.pem"
ca_path = "/etc/sentinelfs/tls/ca.pem"
```

#### Reverse Proxy Setup

For secure API access, set up a reverse proxy using nginx:

```nginx
server {
    listen 443 ssl;
    server_name sentinelfs.domain.com;

    ssl_certificate /path/to/certificate.crt;
    ssl_certificate_key /path/to/private.key;

    location /api {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

## 🔧 Configuration

### Mount Configuration

The primary configuration file is located at `sentinel-fuse/config.toml`. Here's a production-ready template:

```toml
[mount]
point = "/sentinel"
cache_size = "4GB"
io_timeout = "60s"
max_open_files = 131072
read_only = false

[security]
scan_on_write = true
quarantine_path = "/sentinel/.quarantine"
max_scan_size = "1GB"
threat_score_threshold = 85.0

[performance]
read_ahead_size = "8MB"
write_buffer_size = "32MB"
sync_interval = "2s"
concurrent_ops = 256

[network]
connection_pool_size = 10
connection_timeout = "10s"
retry_attempts = 3
health_check_interval = "30s"

[logging]
level = "info"
format = "json"
file = "/var/log/sentinelfs.log"
max_size = "100MB"
max_files = 5
```

### Security Configuration

Configure your security settings in `sentinel-security/config.toml`:

```toml
[yara]
rules_path = "/etc/sentinelfs/yara-rules"
max_scan_size = "1GB"
timeout = "60s"
scan_concurrency = 8

[entropy]
threshold = 7.5
window_size = 2048
enabled = true

[quarantine]
enabled = true
max_retention = "90d"
notify_admin = true
auto_delete = false

[ai]
model_path = "/etc/sentinelfs/models/anomaly-detection.pt"
confidence_threshold = 0.85
learning_enabled = true
```

## 📁 Basic Usage

### Mounting the File System

```bash
# Create mount point
sudo mkdir -p /sentinel

# Mount the file system
sudo ./target/release/sentinel-fused \
    --mount-point /sentinel \
    --config sentinel-fuse/config.toml
```

### File Operations

SentinelFS behaves like a standard file system:

```bash
# Write a file
echo "Hello, SentinelFS!" > /sentinel/hello.txt

# Read a file
cat /sentinel/hello.txt

# Directory operations
mkdir /sentinel/mydata
cp /tmp/data.tar.gz /sentinel/mydata/
ls -la /sentinel/

# File permissions work normally
chmod 600 /sentinel/private.txt
```

### Permission Management

Use the SentinelFS API to manage permissions:

```bash
# Get current permissions for a file
curl -X GET http://localhost:8080/api/v1/files/sentinels/myfile.txt/permissions

# Set permissions
curl -X PUT http://localhost:8080/api/v1/files/sentinels/myfile.txt/permissions \
    -H "Content-Type: application/json" \
    -d '{
        "user": "alice",
        "permissions": ["read", "write"],
        "expiration": "2024-12-31T23:59:59Z"
    }'
```

## 🛠️ Troubleshooting

### Common Issues

#### Mount Point Busy Error
**Issue**: `mount: /sentinel: mount point busy`
**Solution**:
```bash
# Check what's using the mount point
lsof /sentinel
# Unmount if needed
sudo umount /sentinel
# Force unmount if necessary
sudo umount -f /sentinel
```

#### Permission Denied
**Issue**: Unable to mount without root privileges
**Solution**: Ensure your user is in the `fuse` group:
```bash
sudo usermod -a -G fuse $USER
# Log out and back in, or run:
newgrp fuse
```

#### High Memory Usage
**Issue**: SentinelFS using more memory than expected
**Solution**: Adjust cache settings in `config.toml`:
```toml
[mount]
cache_size = "2GB"  # Reduce from default if needed
```

#### Slow Performance
**Issue**: Operations taking longer than expected
**Solution**:
1. Check network connectivity between nodes
2. Review performance settings:
```toml
[performance]
read_ahead_size = "16MB"      # Increase for sequential workloads
concurrent_ops = 512          # Increase for high-concurrency
sync_interval = "5s"          # Increase for better performance
```

### Log Analysis

Check logs for common issues:

```bash
# View recent logs
tail -f /var/log/sentinelfs.log

# Search for errors
grep -i error /var/log/sentinelfs.log

# Monitor security events
grep -i threat /var/log/sentinelfs.log
```

### Health Checks

Use the health check endpoint to monitor system status:

```bash
# Node health
curl http://localhost:8080/api/v1/health

# Storage health
curl http://localhost:8080/api/v1/health/storage

# Security status
curl http://localhost:8080/api/v1/health/security
```

## 🚨 Security Best Practices

### Regular Updates

Keep your YARA rules updated:
```bash
# Update YARA rules
sentinelfs-cli security update-rules
```

### Audit Trail Monitoring

Regularly review audit logs:
```bash
# Get recent audit events
curl "http://localhost:8080/api/v1/audit?limit=100&time_range=24h"
```

### Backup Strategy

Implement a backup strategy for your data:
```bash
# Backup command (example)
sentinelfs-cli backup create --destination /backup/location --encryption
```

## 📊 Monitoring and Metrics

SentinelFS exports Prometheus metrics at `/metrics`:

```bash
# Get metrics
curl http://localhost:8080/metrics
```

Key metrics to monitor:

- `sentinel_fs_operations_total`: Total file operations by type
- `sentinel_fs_threats_detected_total`: Detected threats by type
- `sentinel_fs_latency_seconds`: Operation latency percentiles
- `sentinel_fs_cache_hit_ratio`: Cache efficiency
- `sentinel_fs_network_latency_seconds`: Network performance between nodes

_last updated 29.09.2025_