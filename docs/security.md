---
title: "SentinelFS Security Model"
description: "Security architecture and implementation in SentinelFS"
date: "29.09.2025"
---

# 🛡️ SentinelFS Security Model

## 📋 Table of Contents
- [Zero-Trust Architecture](#zero-trust-architecture)
- [Authentication & Authorization](#authentication--authorization)
- [Key Management](#key-management)
- [Role-Based Access Control (RBAC)](#role-based-access-control-rbac)
- [Threat Detection & Response](#threat-detection--response)
- [Immutable Audit Logging](#immutable-audit-logging)
- [Incident Response](#incident-response)
- [Security Configuration](#security-configuration)
- [Security Testing](#security-testing)

This document details the security architecture and implementation in SentinelFS, covering the zero-trust model, key management, and RBAC implementation.

## Zero-Trust Architecture

SentinelFS implements a comprehensive zero-trust security model with the following principles:

### Never Trust, Always Verify

- All requests are authenticated and authorized regardless of source
- Continuous validation of permissions during long-running operations
- Device and user identity verification
- Network segmentation and micro-perimeters

### Least Privilege Access

- Granular permissions based on user roles and file sensitivity
- Just-in-time access for sensitive operations
- Session-based access tokens with short expiration times
- Principle of least authority (POLA) for system components

### Assume Breach

- Immutable audit logs for all operations
- End-to-end encryption of data in transit and at rest
- Regular security scanning and threat detection
- Automated anomaly detection and response

## Authentication & Authorization

### JWT-Based Authentication

SentinelFS uses JSON Web Tokens (JWT) for authentication with the following characteristics:

- **Algorithm**: RS256 (RSA Signature with SHA-256)
- **Expiration**: 15 minutes (refreshable)
- **Refresh tokens**: 24 hours with automatic rotation
- **Claims**: User ID, roles, permissions, and session information

### Multi-Factor Authentication (MFA)

For administrative accounts, MFA is enforced with:

- TOTP-based second factor
- Hardware security keys support (YubiKey)
- Certificate-based authentication (optional)
- Biometric authentication (when available)

## Key Management

### Encryption Standards

SentinelFS employs industry-standard encryption:

- **Data at rest**: AES-256-GCM encryption
- **Data in transit**: TLS 1.3 with perfect forward secrecy
- **Key derivation**: PBKDF2 with 100,000 iterations
- **Salt generation**: Cryptographically secure random salts

### Key Rotation

Automatic key rotation policies:

- **File encryption keys**: Rotated every 30 days, or immediately if compromised
- **Authentication keys**: Rotated every 90 days
- **Transport keys**: Rotated per session or every 24 hours
- **Database keys**: Rotated during major updates

### Hardware Security Module (HSM) Integration

For enterprise deployments, SentinelFS supports HSM integration:

```toml
[security.keys]
enabled = true
hsm_provider = "thales" # Options: thales, utimaco, gemalto
hsm_endpoint = "tcp://hsm-server:9998"
certificate_path = "/etc/sentinelfs/hsm/cert.pem"
connection_timeout = "30s"
```

### Master Key Management

The master key hierarchy:

```
┌─────────────────────┐
│   Root Master Key   │ ← Protected by HSM / Shamir's Secret
├─────────────────────┤
│   Service Master    │ ← Per service (fuse, security, ai, etc.)
├─────────────────────┤
│    File Master      │ ← Per file group/tenant
├─────────────────────┤
│ Data Encryption Key │ ← Per file (derived from file master)
└─────────────────────┘
```

## Role-Based Access Control (RBAC)

### Role Hierarchy

SentinelFS implements a hierarchical RBAC system:

```
System Admin (highest)
├── Security Admin
│   ├── Network Admin
│   └── Audit Admin
├── Data Owner
│   ├── Data Manager
│   └── Data Viewer
└── Guest (lowest)
```

### Role Permissions

| Role | File Read | File Write | File Delete | Admin Ops | Security Ops | Audit Access |
|------|-----------|------------|-------------|-----------|--------------|--------------|
| System Admin | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Security Admin | ✓ | ✓ | ✓ | - | ✓ | ✓ |
| Network Admin | ✓ | ✓ | - | - | - | - |
| Audit Admin | ✓ | - | - | - | - | ✓ |
| Data Owner | ✓ | ✓ | ✓ | - | - | Partial |
| Data Manager | ✓ | ✓ | ✓* | - | - | - |
| Data Viewer | ✓ | - | - | - | - | - |
| Guest | ✓* | - | - | - | - | - |

*With restrictions based on file sensitivity and time-based policies

### Dynamic Policy Engine

SentinelFS includes a dynamic policy engine that evaluates access requests based on:

- User role and attributes
- Resource sensitivity and classification
- Time-based policies
- Network location and device posture
- Behavioral patterns and anomalies

#### Policy Structure

Policies are defined in a structured format:

```yaml
policies:
  - id: "data_explorer_access"
    name: "Data Explorer Access Policy"
    description: "Allow data exploration with read-only access"
    conditions:
      - type: "user_role"
        value: ["data_viewer"]
      - type: "resource_classification"
        value: ["public", "internal"]
      - type: "time_range"
        value: "09:00-17:00 MON-FRI"
    permissions:
      - "file:read"
    effect: "allow"
```

### Attribute-Based Access Control (ABAC)

In addition to RBAC, SentinelFS supports ABAC for fine-grained access control:

- **User attributes**: Department, clearance level, employment status
- **Resource attributes**: Classification, department, retention policy
- **Environment attributes**: Network zone, device compliance, time

## Threat Detection & Response

### Multi-Layer Security

SentinelFS employs defense-in-depth with multiple security layers:

```
┌─────────────────────────────────────────────────────────┐
│                    APPLICATION LAYER                    │
├─────────────────────────────────────────────────────────┤
│  🔐 JWT Authentication + MFA    │  🎭 RBAC Authorization│
├─────────────────────────────────────────────────────────┤
│          🤖 AI BEHAVIORAL ANALYSIS ENGINE               │
├─────────────────────────────────────────────────────────┤
│  🦠 YARA Malware Detection      │  📊 Entropy Analysis  │
├─────────────────────────────────────────────────────────┤
│  🔒 AES-256-GCM Encryption      │  🔑 Key Management    │
├─────────────────────────────────────────────────────────┤
│          📝 IMMUTABLE AUDIT LOGGING                     │
└─────────────────────────────────────────────────────────┘
```

### YARA-Based Content Scanning

YARA rules are applied to all file operations:

- **Real-time scanning**: All write operations are scanned
- **Deep scanning**: Periodic scanning of existing files
- **Quarantine**: Suspicious files are automatically isolated
- **Custom rules**: Organizations can define custom YARA rules

#### YARA Configuration

```toml
[yara]
rule_dir = "/etc/sentinelfs/yara/rules"
custom_rule_dir = "/etc/sentinelfs/yara/custom"
scan_timeout = "60s"
thread_count = 8
max_file_size = "1GB"
rules:
  - "malware.yar"
  - "suspicious_extensions.yar"
  - "entropy_patterns.yar"
```

### Entropy Analysis

Statistical analysis detects potentially malicious content:

- **Threshold**: Default entropy threshold is 7.5 bits per byte
- **Window size**: 1024-byte sliding windows
- **Algorithm**: Shannon entropy calculation
- **Action**: Flag files with high entropy for inspection

### AI Behavioral Analysis

Machine learning models detect anomalous access patterns:

- **LSTM models**: Detect unusual access timing and patterns
- **Anomaly scoring**: Risk-based scoring system
- **Learning**: Continuous model updates with new data
- **Response**: Automated alerts and access restrictions

## Immutable Audit Logging

### Blockchain-Based Log Integrity

Audit logs in SentinelFS maintain integrity through:

- **Cryptographic hashing**: SHA-256 for log entries
- **Chained integrity**: Each entry references the previous hash
- **Periodic publication**: Hashes published to blockchain
- **Verification**: Anyone can verify log integrity

### Audit Event Categories

| Category | Examples | Retention |
|----------|----------|-----------|
| Administrative | User management, policy changes | 7 years |
| Security | Threat detection, access violations | 7 years |
| Data Access | File read/write/delete | 3 years |
| System | Node health, performance | 1 year |

### Audit Log Format

Each audit entry contains:

```json
{
    "id": "audit-12345",
    "timestamp": "2023-10-01T12:00:00.000Z",
    "version": "1.0",
    "prev_hash": "sha256_hash_of_previous_entry",
    "user": {
        "id": "user-67890",
        "name": "alice",
        "role": "data_manager",
        "ip_address": "192.168.1.100",
        "device_id": "device-abc"
    },
    "action": {
        "type": "file_read",
        "target": "/sentinel/important_data.txt",
        "metadata": {
            "file_size": 1024,
            "classification": "confidential"
        }
    },
    "result": {
        "status": "success",
        "duration_ms": 120
    },
    "security": {
        "threat_score": 0.0,
        "anomaly_score": 0.2,
        "yara_matches": []
    },
    "signature": "digital_signature_of_entry"
}
```

## Incident Response

### Automated Response Actions

When security events are detected, SentinelFS automatically responds:

| Threat Level | Response Action | Notification |
|--------------|----------------|--------------|
| Low | Log event, continue monitoring | System admin |
| Medium | Quarantine file, alert security team | Security admin |
| High | Block access, isolate node, manual review | All admins |
| Critical | Emergency shutdown, full investigation | All admins, legal |

### Threat Scoring

A multi-factor threat scoring system:

```
Threat Score = Weighted Average of:
- YARA match severity (0-100)
- Entropy level (0-100) 
- AI anomaly score (0-100)
- Access pattern deviation (0-100)
- File reputation (0-100)
```

Default thresholds:
- **Green (0-25)**: Normal operations
- **Yellow (26-50)**: Monitor and alert
- **Orange (51-75)**: Quarantine and investigate
- **Red (76-100)**: Block access immediately

## Security Configuration

### Security Settings Configuration

Configure security parameters in `sentinel-security/config.toml`:

```toml
[security]
enabled = true
threat_detection = true
quarantine_enabled = true
scan_on_write = true
scan_on_read = false  # Performance consideration

[security.threats]
threat_score_threshold = 75.0
timeout_seconds = 60
max_concurrent_scans = 16

[security.encryption]
algorithm = "AES-256-GCM"
key_rotation_days = 30
key_storage = "hsm"  # "hsm", "file", "vault"

[security.audit]
retention_days = 730
integrity_check = true
log_encryption = true
```

### Compliance Standards

SentinelFS helps meet compliance requirements including:

- **SOX**: Immutable audit logs and access controls
- **PCI DSS**: Data encryption and access monitoring
- **GDPR**: Data access controls and audit trails
- **HIPAA**: Encryption and access logging

## 🔍 Security Testing

### Penetration Testing

Regular penetration testing includes:

- Network security assessment
- API security testing
- Authentication bypass attempts
- Privilege escalation testing
- Data exposure verification

### Security Validation

Automated security validation tests:

```bash
# Run security tests
./scripts/security-tests/run-all.sh

# Test RBAC enforcement
./scripts/security-tests/rbac-validation.sh

# Validate encryption
./scripts/security-tests/encryption-validation.sh
```

_last updated 01.10.2025_