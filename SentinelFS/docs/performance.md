# ⚡ SentinelFS Performance Tuning Guide

This guide provides optimization strategies and configuration recommendations to maximize the performance of SentinelFS for different use cases and workloads.

## 📊 Performance Overview

### Baseline Benchmarks

SentinelFS performance compared to NFS baseline:

| Metric | NFS Baseline | SentinelFS (Default) | Potential Optimized | Improvement |
|--------|-------------|-------------------|-------------------|-------------|
| Sequential Read | 850 MB/s | 920 MB/s | 1100 MB/s | +8.2% to +29.4% |
| Sequential Write | 720 MB/s | 780 MB/s | 950 MB/s | +8.3% to +31.9% |
| Random Read IOPS | 15,000 | 18,500 | 25,000 | +23.3% to +66.7% |
| Random Write IOPS | 12,000 | 14,800 | 20,000 | +23.3% to +66.7% |
| P99 Latency | 45ms | 38ms | 25ms | -15.6% to -44.4% |

### Performance Factors

SentinelFS performance is influenced by:
- **Hardware**: CPU, RAM, storage, and network capabilities
- **Network**: Latency, bandwidth, and topology between nodes
- **Workload**: I/O patterns, file sizes, and operation types
- **Configuration**: Cache settings, buffer sizes, and security scanning
- **Security**: YARA scanning, encryption, and access validation

## 🛠️ Hardware Optimization

### CPU Requirements

For optimal performance, consider these CPU requirements:

| Workload Type | Cores Needed | CPU Recommendation |
|---------------|--------------|-------------------|
| Light (up to 100 ops/s) | 2-4 cores | 2.0+ GHz, 4+ cores |
| Medium (100-1000 ops/s) | 4-8 cores | 2.5+ GHz, 8+ cores |
| Heavy (1000+ ops/s) | 8+ cores | 3.0+ GHz, 16+ cores |

### Memory Requirements

Memory allocation for different use cases:

| Component | Minimum | Recommended | Heavy Workload |
|-----------|---------|-------------|----------------|
| Cache (FUSE) | 1GB | 4GB | 16GB+ |
| Security Engine | 512MB | 1GB | 4GB |
| AI Engine | 1GB | 2GB | 8GB |
| Network Buffer | 256MB | 512MB | 1GB |
| **Total** | **2.75GB** | **7.5GB** | **29GB+** |

### Storage Considerations

Optimize storage based on your performance requirements:

- **SSD Storage**: Essential for metadata operations and cache
- **High IOPS**: Minimum 10,000 IOPS for active metadata nodes
- **Low Latency**: <1ms latency for optimal performance
- **Capacity**: Plan for growth with at least 80% free space

### Network Optimization

For distributed SentinelFS clusters:

- **Bandwidth**: 10 Gbps minimum, 25+ Gbps for heavy workloads
- **Latency**: <1ms between nodes for optimal performance
- **Topology**: Minimize network hops between nodes
- **Redundancy**: Multiple network paths for high availability

## ⚙️ Configuration Optimization

### Cache Configuration

Adjust cache settings in `sentinel-fuse/config.toml`:

```toml
[mount]
cache_size = "4GB"                    # Increase for read-heavy workloads
read_ahead_size = "8MB"               # Optimize for sequential access
write_buffer_size = "32MB"           # Optimize for write-heavy workloads
max_open_files = 131072              # Increase for many concurrent files
io_timeout = "60s"                   # Adjust based on network latency
```

#### Cache Sizing Guidelines

| Workload | Cache Size Recommendation | Rationale |
|----------|---------------------------|-----------|
| Read-heavy | 50-80% of available RAM | Keep frequently accessed data in memory |
| Write-heavy | 20-40% of available RAM | Balance cache with write buffers |
| Random access | 30-50% of available RAM | Improve hit ratio |
| Sequential access | 10-30% of available RAM | Less benefit from caching |

### Performance-Specific Settings

Optimize for different performance priorities:

#### High IOPS Configuration

```toml
[performance]
concurrent_ops = 512                 # Increase concurrency
read_ahead_size = "4MB"              # Optimize for random IOPS
write_buffer_size = "16MB"           # Balance with read buffers
sync_interval = "10s"                # Reduce sync frequency for better IOPS
```

#### Low Latency Configuration

```toml
[performance]
concurrent_ops = 128                 # Moderate concurrency
read_ahead_size = "2MB"              # Smaller read-ahead for latency
write_buffer_size = "8MB"            # Balanced write buffers
sync_interval = "2s"                 # More frequent sync for consistency
```

#### High Throughput Configuration

```toml
[performance]
concurrent_ops = 256                 # Balanced concurrency
read_ahead_size = "16MB"             # Large read-ahead for sequential access
write_buffer_size = "64MB"           # Large write buffers
sync_interval = "30s"                # Less frequent sync for better throughput
```

### Network Configuration

Optimize network settings for your infrastructure:

```toml
[network]
connection_pool_size = 20           # Increase for high-concurrency
connection_timeout = "15s"          # Adjust based on network reliability
retry_attempts = 5                  # Increase for unreliable networks
health_check_interval = "15s"       # Frequent checks for fast failover
replication_factor = 3              # Balance between availability and performance
```

### Security Performance

Balance security scanning with performance:

```toml
[security]
scan_on_write = true                # Always scan on write
scan_on_read = false                # Only scan on read if required
max_scan_size = "500MB"             # Limit scanning for large files
quarantine_async = true             # Perform quarantine asynchronously
```

## 🧪 Workload-Specific Tuning

### Database Workloads

For database applications using SentinelFS:

```toml
[mount]
cache_size = "8GB"                  # Large cache for database buffers
read_ahead_size = "2MB"             # Conservative read-ahead
write_buffer_size = "32MB"          # Sufficient for transaction logs

[performance]
sync_interval = "1s"                # Frequent sync for ACID compliance
concurrent_ops = 256                # High concurrency for database connections
```

### Media Streaming

For video/audio streaming workloads:

```toml
[mount]
cache_size = "16GB"                 # Large cache for streaming buffers
read_ahead_size = "32MB"            # Large read-ahead for sequential access

[performance]
concurrent_ops = 1024               # Support many concurrent streams
read_ahead_size = "32MB"            # Optimize for sequential reads
sync_interval = "60s"               # Less frequent sync
```

### Machine Learning Workloads

For ML training or inference data:

```toml
[mount]
cache_size = "32GB"                 # Large cache for ML datasets
read_ahead_size = "16MB"            # Balance between random and sequential

[security]
scan_on_read = false                # Disable read scanning for performance
max_scan_size = "1GB"               # Handle large ML models

[performance]
concurrent_ops = 512                # Support parallel training
sync_interval = "30s"               # Balance consistency and performance
```

## 🚀 Advanced Optimization Techniques

### System-Level Tuning

#### Kernel Parameters

Optimize system-level parameters:

```bash
# Increase file handle limits
ulimit -n 524288

# Network buffer tuning
net.core.rmem_max = 134217728
net.core.wmem_max = 134217728
net.ipv4.tcp_rmem = 4096 87380 134217728
net.ipv4.tcp_wmem = 4096 65536 134217728

# Increase network connection limits
net.core.somaxconn = 65535
net.core.netdev_max_backlog = 5000
```

#### File System Optimization

Optimize the underlying file system:

```bash
# Mount options for underlying storage
mount -o noatime,data=writeback,barrier=0 /dev/sdX /path/to/sentinelfs/data
```

### Memory-Mapped I/O

For certain workloads, enable memory-mapped I/O:

```toml
[performance]
use_mmap = true                     # Enable memory-mapped I/O for large files
mmap_threshold = "16MB"              # Minimum file size for mmap
max_mmap_size = "512MB"             # Maximum size to map
```

### Thread Pool Optimization

Adjust thread pools for optimal CPU utilization:

```toml
[threads]
security_pool_size = 16             # YARA scanning threads
ai_pool_size = 8                   # AI analysis threads
network_pool_size = 32             # Network I/O threads
io_pool_size = 16                  # File I/O threads
```

## 📈 Monitoring Performance

### Key Metrics to Monitor

Monitor these metrics for performance optimization:

- `sentinel_fs_operations_total`: Operation rates by type
- `sentinel_fs_latency_seconds`: Operation latency percentiles
- `sentinel_fs_cache_hit_ratio`: Cache efficiency
- `sentinel_fs_network_latency_seconds`: Node-to-node communication
- `sentinel_fs_cpu_usage_percent`: CPU utilization
- `sentinel_fs_memory_usage_bytes`: Memory consumption

### Performance Testing Commands

Use these commands to benchmark your configuration:

```bash
# I/O performance with fio
fio --name=sentinelfs-test \
     --directory=/sentinel \
     --size=10g \
     --bs=4k \
     --direct=1 \
     --numjobs=16 \
     --runtime=60 \
     --time_based \
     --rw=randrw \
     --refill_buffers \
     --verify=0 \
     --end_fsync=0 \
     --iodepth=64

# Latency testing
dd if=/dev/zero of=/sentinel/testfile bs=1M count=1024 oflag=direct

# Throughput testing
dd if=/dev/zero of=/sentinel/bigfile bs=1M count=4096 oflag=direct
```

### Performance Profiling

Enable profiling for performance analysis:

```toml
[debug]
profiling_enabled = true
profile_output_path = "/var/log/sentinelfs-profile.json"
log_level = "info"
```

Then use tools like `perf` or `flamegraph` to analyze bottlenecks:

```bash
# Generate flamegraph
perf record -g -p $(pgrep sentinel-fused)
perf script | stackcollapse-perf | flamegraph > profile.svg
```

## 🚨 Performance Troubleshooting

### Common Performance Issues

#### Slow Read Performance
**Symptoms**: Read operations taking longer than expected
**Solutions**:
1. Increase cache size: `cache_size = "8GB"`
2. Adjust read-ahead: `read_ahead_size = "16MB"`
3. Check network latency between nodes
4. Verify underlying storage performance

#### Slow Write Performance
**Symptoms**: Write operations slow or timing out
**Solutions**:
1. Increase write buffer: `write_buffer_size = "64MB"`
2. Reduce sync frequency: `sync_interval = "30s"`
3. Disable read scanning: `scan_on_read = false`
4. Check disk write performance

#### High Memory Usage
**Symptoms**: SentinelFS using more memory than expected
**Solutions**:
1. Reduce cache size: `cache_size = "2GB"`
2. Limit concurrent operations: `concurrent_ops = 128`
3. Enable memory cleanup: `memory_cleanup_interval = "30s"`

#### High CPU Usage
**Symptoms**: High CPU utilization
**Solutions**:
1. Reduce security scanning: `max_scan_size = "100MB"`
2. Limit YARA threads: `yara_thread_count = 4`
3. Disable AI scanning temporarily: `ai_scanning_enabled = false`

### Performance Checklist

Before going to production, verify:

- [ ] Cache size appropriate for workload
- [ ] Network latency between nodes < 1ms
- [ ] Underlying storage performance tested
- [ ] Security scanning settings balanced with performance
- [ ] Thread pools sized appropriately
- [ ] Monitoring and alerting configured
- [ ] Performance baselines established
- [ ] Load testing performed

## 🧪 Benchmarking

### Internal Benchmarking

Run internal benchmarks to validate your optimizations:

```bash
# Run internal benchmark suite
./scripts/benchmark-suite.sh --config production

# Compare against baseline
./scripts/benchmark-vs-nfs.sh --mount-point /sentinel
```

### Performance Validation

After tuning, validate performance improvements:

```bash
# Confirm improvements
./scripts/validate-performance.sh --baseline nfs --current sentinelfs

# Generate performance report
cargo run --bin sentinel-benchmark -- --report /tmp/performance-report.pdf
```
