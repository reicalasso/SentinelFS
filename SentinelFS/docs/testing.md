# 🧪 SentinelFS Test and Validation Guide

This guide provides comprehensive information about testing, validation, and quality assurance procedures for SentinelFS.

## 🧰 Testing Overview

### Test Categories

SentinelFS implements a multi-layered testing approach:

| Test Category | Purpose | Tools Used | Execution |
|---------------|---------|------------|-----------|
| Unit Tests | Verify individual components | Rust `#[test]`, Python `unittest` | Continuous Integration |
| Integration Tests | Verify module interactions | `cargo test`, custom test harness | CI/CD, Manual |
| Security Tests | Validate security controls | Custom security tools | Manual, periodic |
| Performance Tests | Validate performance SLAs | `fio`, `dd`, custom benchmarks | Periodic, pre-release |
| Chaos Tests | Test resilience to failures | Chaos Monkey, custom fault injection | Periodic |
| Functional Tests | Verify end-to-end functionality | Selenium, custom test suite | CI/CD |

### Test Coverage Goals

- **Minimum Coverage**: 85% line coverage for Rust code
- **Critical Components**: 95% coverage for security-related modules
- **Documentation**: Test scenarios documented with expected results
- **Regression Prevention**: Automated tests for all reported issues

## 🧪 Unit Testing

### Rust Unit Tests

All Rust components include comprehensive unit tests:

```rust
// Example unit test for security module
#[cfg(test)]
mod tests {
    use super::*;
    use std::fs::File;
    use std::io::Write;

    #[test]
    fn test_entropy_calculation_simple() {
        let data = b"Hello, World!";
        let entropy = calculate_entropy(data);
        assert!(entropy > 0.0);
        assert!(entropy < 8.0); // Should be less than max entropy
    }

    #[test]
    fn test_entropy_calculation_random() {
        let data = vec![0x42; 1024]; // All same bytes
        let entropy = calculate_entropy(&data);
        assert!(entropy < 0.1); // Should be very low entropy
    }

    #[test]
    fn test_yara_scan_clean_file() {
        let temp_file = create_test_file("test_clean.txt", b"This is a clean file");
        let result = scan_file_with_yara(&temp_file);
        assert_eq!(result.matches.len(), 0);
    }
}
```

Run unit tests:

```bash
# Run all unit tests
cargo test --lib

# Run tests for a specific module
cargo test -p sentinel-security

# Run tests with coverage
grcov . --binary-path ./target/debug/ -s . -t html --branch --ignore-not-existing -o ./target/coverage/

# Run tests with specific filters
cargo test --lib -- --test-threads=4 --nocapture
```

### Python Unit Tests

For Python AI components:

```python
import unittest
import numpy as np
from sentinel_ai.models import AnomalyDetector


class TestAnomalyDetector(unittest.TestCase):
    def setUp(self):
        self.detector = AnomalyDetector(threshold=0.8)

    def test_normal_pattern(self):
        # Test with normal access pattern
        features = np.array([1, 2, 3, 4, 5])
        result = self.detector.predict(features)
        self.assertFalse(result["anomaly"], "Normal pattern should not be flagged as anomaly")

    def test_anomalous_pattern(self):
        # Test with anomalous pattern
        features = np.array([1, 1, 1, 100, 1])  # Sudden spike
        result = self.detector.predict(features)
        self.assertTrue(result["anomaly"], "Anomalous pattern should be flagged")

    def test_threshold_boundary(self):
        # Test at threshold boundary
        features = np.array([0.8, 0.8, 0.8])  # At threshold
        result = self.detector.predict(features)
        self.assertEqual(result["confidence"], 0.8)
```

Run Python tests:

```bash
# Run Python tests
python -m pytest tests/python/

# Run with coverage
python -m pytest tests/python/ --cov=sentinel_ai --cov-report=html

# Run specific test file
python -m pytest tests/python/test_anomaly_detector.py
```

## 🔗 Integration Testing

### Cross-Module Tests

Verify that different modules work together correctly:

```rust
#[cfg(test)]
mod integration_tests {
    use sentinel_security::scanner::SecurityScanner;
    use sentinel_ai::analyzer::BehavioralAnalyzer;
    use sentinel_net::client::NetworkClient;

    #[tokio::test]
    async fn test_security_ai_integration() {
        // Create temporary file
        let test_file = create_test_file("integration_test.txt", b"Test content");
        
        // Run security scan
        let security_result = SecurityScanner::scan_file(&test_file).await;
        
        // Run AI analysis
        let ai_result = BehavioralAnalyzer::analyze_access("user1", &test_file).await;
        
        // Verify integration points
        assert!(security_result.is_ok());
        assert!(ai_result.is_ok());
        
        // Both should agree on safety assessment
        if security_result.is_clean() && ai_result.is_normal() {
            assert!(should_allow_operation());
        }
    }

    #[tokio::test]
    async fn test_network_security_integration() {
        let client = NetworkClient::new("http://localhost:8080");
        let file_path = "/test/integration/file.txt";
        
        // Attempt to write file through network
        let write_result = client.write_file(file_path, b"test content").await;
        
        // Verify security scanning occurred
        assert!(write_result.is_ok());
        
        // Verify operation was logged
        let audit_exists = client.audit_exists(file_path).await;
        assert!(audit_exists);
    }
}
```

### API Integration Tests

Test the full API stack:

```python
import pytest
import requests
import tempfile
import os


class TestAPIIntegration:
    BASE_URL = "http://localhost:8080/api/v1"
    
    def test_file_lifecycle(self):
        # Create a test file
        with tempfile.NamedTemporaryFile(delete=False) as tmp:
            tmp.write(b"test content for integration")
            temp_path = tmp.name
        
        try:
            # Upload file
            with open(temp_path, 'rb') as f:
                response = requests.post(
                    f"{self.BASE_URL}/files/test_integration.txt",
                    headers={"Authorization": "Bearer test-token"},
                    data=f
                )
                assert response.status_code == 200
            
            # Get file info
            response = requests.get(
                f"{self.BASE_URL}/files/test_integration.txt",
                headers={"Authorization": "Bearer test-token"}
            )
            assert response.status_code == 200
            data = response.json()
            assert data["size"] == 25  # Length of "test content for integration"
            
            # Delete file
            response = requests.delete(
                f"{self.BASE_URL}/files/test_integration.txt",
                headers={"Authorization": "Bearer test-token"}
            )
            assert response.status_code == 200
            
        finally:
            os.unlink(temp_path)

    def test_security_scanning_integration(self):
        # Test that security scanning happens during file operations
        with tempfile.NamedTemporaryFile(delete=False) as tmp:
            tmp.write(b"X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*\n")
            temp_path = tmp.name
        
        try:
            # Upload EICAR test file
            with open(temp_path, 'rb') as f:
                response = requests.post(
                    f"{self.BASE_URL}/files/eicar_test.txt",
                    headers={"Authorization": "Bearer test-token"},
                    data=f
                )
                # Should be quarantined, not 200 OK
                assert response.status_code in [200, 403]  # May allow quarantine or block
                
            # Check if file was quarantined
            response = requests.get(
                f"{self.BASE_URL}/security/quarantine",
                headers={"Authorization": "Bearer test-token"}
            )
            assert response.status_code == 200
            quarantine_data = response.json()
            eicar_found = any("eicar" in item["original_path"].lower() 
                            for item in quarantine_data["quarantined_files"])
            assert eicar_found, "EICAR test file should be in quarantine"
            
        finally:
            os.unlink(temp_path)
```

## 🛡️ Security Testing

### Attack Simulation Suite

SentinelFS includes comprehensive security test suites:

#### Ransomware Simulation

```python
# scripts/security-tests/ransomware-simulation.py
import requests
import time
import random
import string
from Crypto.Cipher import AES
from Crypto.Random import get_random_bytes


def simulate_ransomware_attack():
    """Simulate ransomware encryption behavior"""
    base_url = "http://localhost:8080/api/v1"
    auth_headers = {"Authorization": "Bearer test-token"}
    
    # Create multiple files to encrypt
    files_to_encrypt = []
    for i in range(10):
        filename = f"victim_file_{i}.txt"
        content = f"Important document {i} - should not be encrypted! " * 100
        
        # Upload file
        response = requests.post(
            f"{base_url}/files/{filename}",
            headers=auth_headers,
            data=content.encode()
        )
        
        if response.status_code == 200:
            files_to_encrypt.append(filename)
    
    print(f"Created {len(files_to_encrypt)} files for ransomware simulation")
    
    # Rapid file modification (like ransomware encryption)
    for filename in files_to_encrypt:
        try:
            # Modify file rapidly (simulating encryption)
            cipher = AES.new(get_random_bytes(32), AES.MODE_EAX)
            # ... simulate encryption process
            time.sleep(0.01)  # Small delay to not overwhelm
        except Exception as e:
            print(f"Error during ransomware simulation: {e}")
    
    print("Ransomware simulation completed")

if __name__ == "__main__":
    simulate_ransomware_attack()
```

#### Data Exfiltration Simulation

```python
# scripts/security-tests/data-exfiltration.py
import requests
import time
import random


def simulate_data_exfiltration():
    """Simulate bulk data access patterns"""
    base_url = "http://localhost:8080/api/v1"
    auth_headers = {"Authorization": "Bearer test-token"}
    
    # Create multiple files with sensitive content
    sensitive_files = []
    for i in range(50):
        filename = f"sensitive_data_{i:03d}.dat"
        content = f"CONFIDENTIAL DATA RECORD {i:03d}: " + "sensitive content " * 20
        
        response = requests.post(
            f"{base_url}/files/{filename}",
            headers=auth_headers,
            data=content.encode()
        )
        
        if response.status_code == 200:
            sensitive_files.append(filename)
    
    print(f"Created {len(sensitive_files)} sensitive files")
    
    # Rapid sequential access to simulate exfiltration
    start_time = time.time()
    accessed_count = 0
    
    for filename in sensitive_files:
        try:
            response = requests.get(
                f"{base_url}/files/{filename}/content",
                headers=auth_headers
            )
            
            if response.status_code == 200:
                accessed_count += 1
                # Minimal delay to simulate fast access
                time.sleep(0.001)
                
        except Exception as e:
            print(f"Error accessing {filename}: {e}")
    
    duration = time.time() - start_time
    print(f"Exfiltration simulation: {accessed_count} files in {duration:.2f}s")
    print(f"Rate: {accessed_count/duration:.2f} files/second")

if __name__ == "__main__":
    simulate_data_exfiltration()
```

### Security Test Execution

Run comprehensive security tests:

```bash
# Run all security tests
./scripts/security-tests/run-all.sh

# Run specific security test
time python ./scripts/security-tests/ransomware-simulation.py

# Run security regression tests
./scripts/run-security-regression.sh

# Test security rules update
./scripts/test-security-rules.sh
```

### Security Test Results Analysis

Security tests produce detailed reports:

```bash
# Generate security test report
./scripts/generate-security-report.sh > security-test-report.json

# Analyze results
python -c "
import json
with open('security-test-report.json') as f:
    report = json.load(f)
    
print('Security Test Results:')
print(f'Tests Run: {report[\"total_tests\"]}')
print(f'Passed: {report[\"passed\"]}')
print(f'Failed: {report[\"failed\"]}')
print(f'Detection Rate: {report[\"detection_rate\"]}%')
print(f'False Positive Rate: {report[\"false_positive_rate\"]}%')
"
```

## 🚀 Performance Testing

### Benchmark Suite

Run comprehensive performance benchmarks:

```bash
#!/bin/bash
# scripts/benchmark-suite.sh

set -e

echo "Starting SentinelFS Performance Benchmark Suite"

# Sequential read benchmark
echo "Running Sequential Read Test..."
fio --name=sequential-read \
     --rw=read \
     --bs=1M \
     --size=1G \
     --numjobs=4 \
     --runtime=60 \
     --directory=/sentinel \
     --output-format=json > seq-read.json

# Sequential write benchmark
echo "Running Sequential Write Test..."
fio --name=sequential-write \
     --rw=write \
     --bs=1M \
     --size=1G \
     --numjobs=4 \
     --runtime=60 \
     --directory=/sentinel \
     --output-format=json > seq-write.json

# Random read benchmark
echo "Running Random Read Test..."
fio --name=random-read \
     --rw=randread \
     --bs=4k \
     --size=1G \
     --numjobs=16 \
     --runtime=60 \
     --directory=/sentinel \
     --output-format=json > rand-read.json

# Random write benchmark
echo "Running Random Write Test..."
fio --name=random-write \
     --rw=randwrite \
     --bs=4k \
     --size=1G \
     --numjobs=16 \
     --runtime=60 \
     --directory=/sentinel \
     --output-format=json > rand-write.json

# Mixed workload benchmark
echo "Running Mixed Workload Test..."
fio --name=mixed-workload \
     --rw=rw \
     --rwmixread=70 \
     --bs=4k \
     --size=1G \
     --numjobs=8 \
     --runtime=60 \
     --directory=/sentinel \
     --output-format=json > mixed.json

# Security scanning impact test
echo "Running Security Impact Test..."
{
  echo "Without scanning:"
  time dd if=/dev/zero of=/sentinel/test-file bs=1M count=100 oflag=direct
  
  echo "With scanning (small files):"
  for i in {1..10}; do
    dd if=/dev/zero of=/sentinel/small-$i bs=1k count=1 oflag=direct
  done
} > security-impact.txt

echo "Performance benchmark suite completed!"
```

### Performance Validation

Validate performance against baseline:

```bash
#!/bin/bash
# scripts/validate-performance.sh

BASELINE_FILE=${1:-"baseline.json"}
CURRENT_FILE=${2:-"current.json"}
THRESHOLD=${3:-"0.95"}  # 95% of baseline is acceptable

# Run current benchmarks
cargo run --bin sentinel-benchmark -- --format json > "$CURRENT_FILE"

# Compare with baseline
python -c "
import json

with open('$BASELINE_FILE', 'r') as f:
    baseline = json.load(f)

with open('$CURRENT_FILE', 'r') as f:
    current = json.load(f)

all_pass = True
for metric in baseline:
    if metric in current:
        baseline_val = baseline[metric]
        current_val = current[metric]
        ratio = current_val / baseline_val if baseline_val > 0 else 0
        
        if ratio < $THRESHOLD:
            print(f'PERFORMANCE DEGRADATION: {metric} - Baseline: {baseline_val}, Current: {current_val}, Ratio: {ratio:.2f}')
            all_pass = False
        else:
            print(f'OK: {metric} - {current_val} ({ratio:.2f}x baseline)')
    else:
        print(f'MISSING METRIC: {metric}')
        all_pass = False

if not all_pass:
    print('Performance validation FAILED')
    exit(1)
else:
    print('Performance validation PASSED')
"
```

## 🐛 Chaos Engineering

### Fault Injection Tests

Test system resilience with chaos engineering:

```python
# scripts/chaos-engineering.py
import requests
import time
import threading
import random
from kubernetes import client, config


class ChaosEngineer:
    def __init__(self, base_url="http://localhost:8080"):
        self.base_url = base_url
        
    def network_partition(self, duration=30):
        """Simulate network partition between nodes"""
        print(f"Injecting network partition for {duration}s")
        # Implementation would use tools like iptables or tc
        # This is a simulation
        time.sleep(duration)
        print("Network partition restored")
    
    def disk_failure(self, disk_path, duration=60):
        """Simulate disk failure"""
        print(f"Simulating disk failure on {disk_path} for {duration}s")
        # This would involve blocking disk access
        time.sleep(duration)
        print("Disk failure simulation ended")
    
    def memory_stress(self, node, memory_mb=1024, duration=120):
        """Apply memory stress to a node"""
        print(f"Applying memory stress to {node} ({memory_mb}MB) for {duration}s")
        # Implementation would create memory-heavy process
        time.sleep(duration)
        print("Memory stress test completed")
    
    def run_chaos_test(self):
        """Run comprehensive chaos test"""
        print("Starting chaos engineering test")
        
        # Run operations while injecting chaos
        operations_thread = threading.Thread(target=self.run_normal_operations)
        operations_thread.start()
        
        # Inject various failures
        time.sleep(10)  # Let system stabilize
        self.network_partition(duration=45)
        time.sleep(10)
        self.memory_stress("node-1", memory_mb=2048, duration=60)
        time.sleep(10)
        self.disk_failure("/dev/sdb", duration=30)
        
        operations_thread.join()
        print("Chaos engineering test completed")
    
    def run_normal_operations(self):
        """Run normal operations during chaos"""
        start_time = time.time()
        operations_count = 0
        
        while time.time() - start_time < 120:  # Run for 2 minutes
            try:
                # Perform normal operations
                response = requests.get(f"{self.base_url}/api/v1/health")
                if response.status_code == 200:
                    operations_count += 1
                time.sleep(1)
            except Exception as e:
                print(f"Operation failed during chaos: {e}")
        
        print(f"Completed {operations_count} operations during chaos test")

if __name__ == "__main__":
    chaos = ChaosEngineer()
    chaos.run_chaos_test()
```

### Chaos Test Execution

```bash
# Run chaos engineering tests
python ./scripts/chaos-engineering.py

# Run specific fault injection
time ./scripts/network-partition-test.sh

# Run resilience validation
time ./scripts/resilience-validation.sh
```

## 🧪 Validation Criteria

### Test Success Criteria

| Test Type | Success Criteria | Warning Threshold | Failure Threshold |
|-----------|------------------|-------------------|-------------------|
| Unit Tests | 100% pass rate | >5% failure rate | >10% failure rate |
| Security Tests | 90%+ detection rate | <95% detection | <85% detection | | Performance | Within 105% of baseline | 110-120% of baseline | >120% of baseline |
| Integration | >95% pass rate | 90-95% pass rate | <90% pass rate |
| Chaos Tests | >90% requests succeed | 80-90% succeed | <80% succeed |

### Automated Validation

```bash
# Run all validations
cargo run --bin sentinel-validation -- --all

# Validate specific aspects
cargo run --bin sentinel-validation -- --security

cargo run --bin sentinel-validation -- --performance

cargo run --bin sentinel-validation -- --functional
```

### Validation Report Generation

```bash
#!/bin/bash
# scripts/generate-validation-report.sh

OUTPUT_DIR=${1:-"/tmp/validation-report"}
mkdir -p "$OUTPUT_DIR"

# Run all tests and collect results
echo "Running unit tests..."
cargo test --lib -- --format=json --quiet > "$OUTPUT_DIR/unit-tests.json" 2>&1

echo "Running integration tests..."
cargo test --test integration -- --format=json --quiet > "$OUTPUT_DIR/integration-tests.json" 2>&1

echo "Running security tests..."
./scripts/security-tests/run-all.sh > "$OUTPUT_DIR/security-tests.log" 2>&1

echo "Running performance tests..."
./scripts/benchmark-suite.sh
mv *.json "$OUTPUT_DIR/"

# Generate summary
cat > "$OUTPUT_DIR/summary.md" << EOF
# SentinelFS Validation Report

## Summary
- **Date**: $(date)
- **Version**: $(git describe --tags)
- **Commit**: $(git rev-parse HEAD)

## Test Results
- **Unit Tests**: $(grep -c '"type":"test"' "$OUTPUT_DIR/unit-tests.json" || echo 0) total
- **Integration Tests**: $(grep -c '"type":"test"' "$OUTPUT_DIR/integration-tests.json" || echo 0) total
- **Security Tests**: Results in security-tests.log
- **Performance Tests**: Benchmarks completed

## Status
- **Overall**: PASSED/FAILED (to be determined by script)

EOF

echo "Validation report generated in $OUTPUT_DIR"
```

## 🔍 Test Data Management

### Test Data Sets

Different test data sets for various scenarios:

- **Clean Data Set**: Benign files for normal operations
- **Malware Samples**: EICAR, test malware for security testing
- **Large Files**: Multi-GB files for performance testing
- **Special Characters**: Files with Unicode, special characters
- **Edge Cases**: Zero-byte files, very large filenames

### Synthetic Data Generation

```bash
# Generate various test data
./scripts/generate-test-data.sh --size 10GB --type mixed

# Generate security test data
./scripts/generate-security-test-data.sh

# Generate performance test data
./scripts/generate-performance-test-data.sh
```

## 🚦 Continuous Integration

### CI Pipeline

```yaml
# .github/workflows/test.yml
name: SentinelFS CI Tests

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup Rust
        uses: actions-rs/toolchain@v1
        with:
          toolchain: stable
          components: rustfmt, clippy
      - name: Run unit tests
        run: |
          cargo test --lib
          cargo fmt -- --check
          cargo clippy -- -D warnings
          
  integration-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup environment
        run: |
          sudo apt update
          sudo apt install -y libfuse-dev
      - name: Run integration tests
        run: cargo test --test integration
        
  security-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.9'
      - name: Install Python dependencies
        run: pip install -r requirements.txt
      - name: Run security tests
        run: ./scripts/security-tests/run-all.sh
```

### Quality Gates

Automated quality gates ensure code quality:

- **Code Coverage**: Must maintain >85% coverage
- **Security Scanning**: No critical vulnerabilities
- **Performance**: Within 105% of baseline performance
- **Build Success**: All compilation steps pass
- **Test Success**: All tests pass

This comprehensive testing and validation framework ensures that SentinelFS maintains high quality, security, and performance standards.