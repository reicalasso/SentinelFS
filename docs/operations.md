---
title: "SentinelFS Deployment & Operations Guide"
description: "Comprehensive information for deploying, managing, and operating SentinelFS in production environments"
date: "29.09.2025"
---

# 🚀 SentinelFS Deployment & Operations Guide

## 📋 Table of Contents
- [Production Deployment](#production-deployment)
- [Monitoring Integration](#monitoring-integration)
- [Operations Management](#operations-management)
- [Troubleshooting](#troubleshooting)
- [Security Operations](#security-operations)
- [Capacity Planning](#capacity-planning)

This guide provides comprehensive information for deploying, managing, and operating SentinelFS in production environments.

## Production Deployment

### Infrastructure Requirements

For a production SentinelFS deployment, ensure you have:

#### Minimum Requirements (Small Deployment)
- **Nodes**: 3 (for high availability)
- **CPU**: 8 cores per node (16+ recommended)
- **RAM**: 16GB per node (32+ recommended)
- **Storage**: 1TB SSD per node (NVMe recommended)
- **Network**: 10 Gbps interconnect (25 Gbps recommended)
- **OS**: Ubuntu 20.04+/CentOS 8+ or equivalent

#### Recommended Requirements (Medium Deployment)
- **Nodes**: 5 (for better fault tolerance)
- **CPU**: 16 cores per node
- **RAM**: 32GB per node
- **Storage**: 5TB NVMe per node
- **Network**: 25 Gbps interconnect
- **Network**: Dedicated storage network (VLAN/isolated)

#### Enterprise Requirements (Large Deployment)
- **Nodes**: 7+ (for maximum fault tolerance)
- **CPU**: 32+ cores per node
- **RAM**: 64GB+ per node
- **Storage**: 10TB+ NVMe per node
- **Network**: 40+ Gbps interconnect
- **Security**: HSM for key management

### Deployment Strategies

#### Single Datacenter Deployment

```
┌─────────────────────────────────────────────────────────┐
│                Datacenter Network                       │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐   │
│  │ Node 1  │  │ Node 2  │  │ Node 3  │  │ Load    │   │
│  │ (Meta)  │  │ (Data)  │  │ (Data)  │  │ Balancer│   │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘   │
└─────────────────────────────────────────────────────────┘
```

- All nodes in same location
- Lowest latency
- Single point of failure for site

#### Multi-Datacenter Deployment

```
Datacenter A:         Datacenter B:         Datacenter C:
┌─────────┐           ┌─────────┐           ┌─────────┐
│ Node 1  │◄─────────►│ Node 4  │◄─────────►│ Node 7  │
│ (Meta)  │◄─────────►│ (Meta)  │◄─────────►│ (Meta)  │
└─────────┘           └─────────┘           └─────────┘
┌─────────┐           ┌─────────┐           ┌─────────┐
│ Node 2  │◄─────────►│ Node 5  │◄─────────►│ Node 8  │
│ (Data)  │◄─────────►│ (Data)  │◄─────────►│ (Data)  │
└─────────┘           └─────────┘           └─────────┘
```

- Maximum availability
- Geographic distribution
- Higher complexity

### Kubernetes Deployment

#### Helm Chart Installation

SentinelFS provides a Helm chart for easy Kubernetes deployment:

```bash
# Add the SentinelFS Helm repository
helm repo add sentinelfs https://charts.sentinelfs.io
helm repo update

# Install with default values
helm install sentinelfs sentinelfs/sentinelfs \
    --namespace sentinelfs \
    --create-namespace \
    --set global.storageClass=fast-ssd \
    --set global.storageSize=100Gi
```

#### Custom Values File

Create a `values.yaml` for custom configuration:

```yaml
# SentinelFS Helm Values
global:
  storageClass: "fast-ssd"
  storageSize: "500Gi"

sentinelfs:
  replicaCount: 3
  image:
    repository: sentinelfs/sentinelfs
    tag: "1.0.0"
    pullPolicy: IfNotPresent
  
  resources:
    requests:
      cpu: "2"
      memory: "4Gi"
    limits:
      cpu: "4"
      memory: "8Gi"
  
  service:
    type: ClusterIP
    port: 8080
  
  persistence:
    enabled: true
    size: 500Gi
    # IMPORTANT: Use fast, reliable storage for performance
  
  nodeSelector:
    node-type: storage
  
  tolerations:
    - key: "dedicated"
      operator: "Equal"
      value: "storage"
      effect: "NoSchedule"

# Security-specific settings
security:
  yaraRulesVolume:
    enabled: true
    existingClaim: "yara-rules-pvc"
  
  tls:
    enabled: true
    secretName: "sentinelfs-tls"

# Monitoring integration
monitoring:
  enabled: true
  prometheus:
    serviceMonitor:
      enabled: true
    prometheusRule:
      enabled: true
  grafana:
    dashboards:
      enabled: true

# Backup configuration
backup:
  enabled: true
  schedule: "0 2 * * *"  # Daily at 2 AM
  retention: 30          # Keep 30 days
  destination:
    s3Bucket: "sentinelfs-backups"
    region: "us-west-2"
```

#### Deployment Commands

```bash
# Install with custom values
helm install sentinelfs sentinelfs/sentinelfs \
    --namespace sentinelfs \
    --create-namespace \
    -f values.yaml

# Upgrade deployment
helm upgrade sentinelfs sentinelfs/sentinelfs \
    --namespace sentinelfs \
    -f values.yaml

# Check deployment status
helm status sentinelfs --namespace sentinelfs

# View pods
kubectl get pods -n sentinelfs

# Check logs
kubectl logs -n sentinelfs deployment/sentinelfs
```

### Docker Compose Deployment

For testing or small deployments, use Docker Compose:

```yaml
# docker-compose.yml
version: '3.8'

services:
  sentinelfs-node-1:
    image: sentinelfs/sentinelfs:latest
    container_name: sentinelfs-node-1
    ports:
      - "8080:8080"
    volumes:
      - ./data/node-1:/data
      - ./config:/etc/sentinelfs
      - /var/run/docker.sock:/var/run/docker.sock
    environment:
      - SENTINELFS_ROLE=metadata
      - SENTINELFS_CLUSTER_NODES=sentinelfs-node-1,sentinelfs-node-2,sentinelfs-node-3
      - SENTINELFS_NODE_ID=node-1
    networks:
      - sentinelfs-net
    deploy:
      resources:
        limits:
          cpus: '4.0'
          memory: 8G
    restart: unless-stopped

  sentinelfs-node-2:
    image: sentinelfs/sentinelfs:latest
    container_name: sentinelfs-node-2
    ports:
      - "8081:8080"
    volumes:
      - ./data/node-2:/data
      - ./config:/etc/sentinelfs
    environment:
      - SENTINELFS_ROLE=data
      - SENTINELFS_CLUSTER_NODES=sentinelfs-node-1,sentinelfs-node-2,sentinelfs-node-3
      - SENTINELFS_NODE_ID=node-2
    networks:
      - sentinelfs-net
    deploy:
      resources:
        limits:
          cpus: '4.0'
          memory: 8G
    restart: unless-stopped

  sentinelfs-node-3:
    image: sentinelfs/sentinelfs:latest
    container_name: sentinelfs-node-3
    ports:
      - "8082:8080"
    volumes:
      - ./data/node-3:/data
      - ./config:/etc/sentinelfs
    environment:
      - SENTINELFS_ROLE=data
      - SENTINELFS_CLUSTER_NODES=sentinelfs-node-1,sentinelfs-node-2,sentinelfs-node-3
      - SENTINELFS_NODE_ID=node-3
    networks:
      - sentinelfs-net
    deploy:
      resources:
        limits:
          cpus: '4.0'
          memory: 8G
    restart: unless-stopped

  prometheus:
    image: prom/prometheus:latest
    container_name: sentinelfs-prometheus
    ports:
      - "9090:9090"
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml
      - prometheus_data:/prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--web.console.libraries=/etc/prometheus/console_libraries'
      - '--web.console.templates=/etc/prometheus/consoles'
    networks:
      - sentinelfs-net
    restart: unless-stopped

  grafana:
    image: grafana/grafana:latest
    container_name: sentinelfs-grafana
    ports:
      - "3000:3000"
    volumes:
      - grafana_data:/var/lib/grafana
      - ./monitoring/dashboards:/etc/grafana/provisioning/dashboards
      - ./monitoring/datasources:/etc/grafana/provisioning/datasources
    networks:
      - sentinelfs-net
    restart: unless-stopped
    depends_on:
      - prometheus

networks:
  sentinelfs-net:
    driver: bridge

volumes:
  prometheus_data:
  grafana_data:
```

Deploy with:

```bash
# Start the cluster
docker-compose up -d

# Check status
docker-compose ps

# View logs
docker-compose logs -f
```

## Monitoring Integration

### Prometheus Configuration

Configure Prometheus to scrape metrics from SentinelFS:

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'sentinelfs'
    static_configs:
      - targets: ['sentinelfs-node-1:8080', 'sentinelfs-node-2:8080', 'sentinelfs-node-3:8080']
    metrics_path: /api/v1/metrics
    scrape_interval: 15s
    scrape_timeout: 10s
    honor_labels: true
    relabel_configs:
      - source_labels: [__address__]
        target_label: node
        regex: '([^:]+):.*'
```

### Grafana Dashboards

Import these pre-built dashboards for comprehensive monitoring:

#### System Overview Dashboard
- Node health and status
- Storage utilization
- Network topology and latency
- CPU, memory, and disk I/O

#### Security Dashboard
- Threat detection rates
- Quarantined files count
- Security scan performance
- Anomaly detection alerts

#### Performance Metrics Dashboard
- I/O operations per second
- Read/write latency percentiles
- Cache hit ratios
- Throughput measurements

#### AI Analytics Dashboard
- Model accuracy rates
- Anomaly detection scores
- False positive trends
- Learning progress indicators

### Alert Rules

Configure Prometheus alert rules:

```yaml
# prometheus-alerts.yml
groups:
  - name: sentinelfs.rules
    rules:
    - alert: SentinelFSNodeDown
      expr: up{job="sentinelfs"} == 0
      for: 1m
      labels:
        severity: critical
      annotations:
        summary: "SentinelFS node is down"
        description: "{{ $labels.instance }} has been down for more than 1 minute."

    - alert: HighThreatDetectionRate
      expr: increase(sentinelfs_fs_threats_detected_total[5m]) > 10
      for: 2m
      labels:
        severity: warning
      annotations:
        summary: "High threat detection rate"
        description: "{{ $labels.instance }} detected {{ $value }} threats in the last 5 minutes."

    - alert: CacheHitRatioLow
      expr: sentinelfs_fs_cache_hit_ratio < 0.7
      for: 5m
      labels:
        severity: warning
      annotations:
        summary: "Low cache hit ratio"
        description: "Cache hit ratio on {{ $labels.instance }} is {{ $value }}."

    - alert: HighLatency
      expr: sentinelfs_fs_latency_seconds{quantile="0.99"} > 0.1
      for: 2m
      labels:
        severity: warning
      annotations:
        summary: "High operation latency"
        description: "P99 latency on {{ $labels.instance }} is {{ $value }} seconds."
```

### ELK Stack Integration

Configure Filebeat to send logs to ELK stack:

```yaml
# filebeat.yml
filebeat.inputs:
- type: log
  enabled: true
  paths:
    - /var/log/sentinelfs.log
  json.keys_under_root: true
  json.add_error_key: true
  json.message_key: message

output.elasticsearch:
  hosts: ["elasticsearch:9200"]
  index: "sentinelfs-logs-%{+yyyy.MM.dd}"

processors:
- add_host_metadata:
    when.not.contains.tags: forwarded
- add_docker_metadata: ~

setup.kibana:
  host: "kibana:5601"
```

## Operations Management

### Daily Operations

#### Health Checks

Perform regular health checks:

```bash
# Check system health
sentinelfs-cli health check

curl http://localhost:8080/api/v1/health

# Check storage health
sentinelfs-cli storage health

# Check security status
sentinelfs-cli security check

# Check cluster status
sentinelfs-cli cluster status
```

#### Performance Monitoring

Monitor performance metrics:

```bash
# Get current metrics
curl http://localhost:8080/api/v1/metrics | grep -E "operations_total|latency_seconds|cache_hit_ratio"

# Check operations per second
sentinelfs-cli monitor ops

# Check I/O performance
sentinelfs-cli monitor io
```

### Backup & Restore

#### Automated Backups

Configure automated backups with the backup tool:

```bash
#!/bin/bash
# backup-script.sh

BACKUP_DIR="/backup/sentinelfs"
DATE=$(date +%Y%m%d_%H%M%S)

# Create backup directory
mkdir -p "$BACKUP_DIR/$DATE"

# Backup configuration
sentinelfs-cli backup config --destination "$BACKUP_DIR/$DATE/config"

# Backup audit logs (retention based)
sentinelfs-cli backup audit --destination "$BACKUP_DIR/$DATE/audit" --retention 30d

# Backup encryption keys (secure location)
sentinelfs-cli backup keys --destination "$BACKUP_DIR/$DATE/keys" --encrypt

# Verify backup integrity
sentinelfs-cli backup verify "$BACKUP_DIR/$DATE"

# Compress and encrypt
if [ $? -eq 0 ]; then
  tar -czf "$BACKUP_DIR/sentinelfs_$DATE.tar.gz" -C "$BACKUP_DIR" "$DATE"
  rm -rf "$BACKUP_DIR/$DATE"
  echo "Backup completed: sentinelfs_$DATE.tar.gz"
else
  echo "Backup failed, check logs"
  exit 1
fi
```

#### Backup Schedule

Add to crontab:

```bash
# Daily backup at 2 AM
0 2 * * * /opt/sentinelfs/backup-script.sh

# Weekly full backup on Sundays at 1 AM
0 1 * * 0 /opt/sentinelfs/backup-full-script.sh
```

#### Restoration Process

```bash
# Stop services before restoration
sudo systemctl stop sentinelfs

# Restore configuration
sentinelfs-cli restore config --source /backup/config

# Restore audit logs
sentinelfs-cli restore audit --source /backup/audit

# Restore encryption keys
sentinelfs-cli restore keys --source /backup/keys

# Restart services
sudo systemctl start sentinelfs

# Verify restoration
sentinelfs-cli health check
```

### Scaling Operations

#### Adding Nodes

```bash
# Prepare new node
sentinelfs-cli node prepare --id node-4 --role data

# Join cluster
sentinelfs-cli cluster join --node node-4 --cluster-address 192.168.1.10

# Verify node status
sentinelfs-cli node status --id node-4

# Balance data across nodes
sentinelfs-cli cluster rebalance
```

#### Removing Nodes

```bash
# Drain node (migrate data)
sentinelfs-cli node drain --id node-3

# Remove from cluster
sentinelfs-cli cluster remove --id node-3

# Verify removal
sentinelfs-cli cluster status
```

### Maintenance Operations

#### Security Updates

```bash
# Update YARA rules
sentinelfs-cli security update-rules

# Update threat intelligence
sentinelfs-cli security update-threats

# Scan all files
sentinelfs-cli security deep-scan --all
```

#### System Maintenance

```bash
# Clean temporary files
sentinelfs-cli maintenance cleanup-temp

# Rotate logs
sentinelfs-cli maintenance rotate-logs

# Update statistics
sentinelfs-cli maintenance update-stats

# Optimize storage
sentinelfs-cli maintenance optimize-storage
```

## Troubleshooting

### Common Issues

#### Cluster Communication Problems
**Symptoms**: Nodes showing as unhealthy, replication failing
**Solutions**:
1. Check network connectivity between nodes
2. Verify firewall rules allow required ports
3. Check cluster configuration consistency
4. Restart network components if needed

```bash
# Test connectivity
ping -c 3 <node-ip>

# Check ports
nmap -p 8080,9090,9091 <node-ip>

# Restart network components
sentinelfs-cli network restart
```

#### Performance Degradation
**Symptoms**: Slow operations, high latency, low throughput
**Solutions**:
1. Check system resources (CPU, memory, disk, network)
2. Review cache settings
3. Verify storage performance
4. Check for security scanning impacting performance

#### Security Engine Issues
**Symptoms**: High threat detection, quarantined files, slow scans
**Solutions**:
1. Update YARA rules
2. Check entropy threshold settings
3. Verify AI model status
4. Review security policies

### Debugging Commands

```bash
# Enable debug logging
curl -X POST http://localhost:8080/api/v1/debug/enable

# Get detailed system status
sentinelfs-cli debug status

# Get configuration dump
curl http://localhost:8080/api/v1/debug/config

# Get performance profile
sentinelfs-cli debug profile --duration 30s

# Get memory dump (for advanced debugging)
sentinelfs-cli debug memory-dump
```

### Log Analysis

#### Log Locations
- **Main logs**: `/var/log/sentinelfs.log`
- **Security logs**: `/var/log/sentinelfs-security.log`
- **Audit logs**: `/var/log/sentinelfs-audit.log`
- **Debug logs**: `/var/log/sentinelfs-debug.log`

#### Log Analysis Commands

```bash
# Find errors in logs
grep -i error /var/log/sentinelfs.log

grep -i "threat|quarantine|security" /var/log/sentinelfs-security.log

# Monitor logs in real-time
tail -f /var/log/sentinelfs.log

# Analyze performance from logs
awk '/operation completed/ {print $4}' /var/log/sentinelfs.log | sort -n

# Find specific user activity
grep "username=alice" /var/log/sentinelfs-audit.log
```

## Security Operations

### Certificate Management

```bash
# Generate new certificates
sentinelfs-cli security generate-cert --type server --name sentinelfs-node-1

# Rotate certificates
sentinelfs-cli security rotate-cert --force

# Check certificate expiration
sentinelfs-cli security check-cert
```

### Access Control

```bash
# Add new user
sentinelfs-cli user add --username alice --role data_manager

# Update user permissions
curl -X PUT http://localhost:8080/api/v1/users/alice/permissions \
    -H "Content-Type: application/json" \
    -d '{"permissions": ["read", "write"]}'

# Revoke access
curl -X DELETE http://localhost:8080/api/v1/users/alice/access
```

## Capacity Planning

### Storage Growth Planning

Monitor storage growth patterns:

```bash
# Get storage utilization
curl http://localhost:8080/api/v1/storage/utilization

# Get historical data
sentinelfs-cli storage history --period 30d

# Predict growth
sentinelfs-cli storage predict --period 90d
```

### Performance Baseline

Establish and maintain performance baselines:

- **IOPS Baseline**: Document normal operations per second
- **Latency Baseline**: Document normal read/write latencies
- **Throughput Baseline**: Document normal data transfer rates
- **Resource Baseline**: Document normal CPU/memory usage

This information enables proactive operations and capacity planning for SentinelFS deployments.

_last updated 01.10.2025_