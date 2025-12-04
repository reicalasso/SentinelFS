# SentinelFS Key Management Design

## Overview

SentinelFS implements a two-tier key hierarchy that provides strong authentication while enabling convenient device pairing:

1. **Identity Keys** (long-lived) - For device authentication and signing
2. **Session Keys** (short-lived) - For connection encryption with forward secrecy

## Key Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                    IDENTITY LAYER                           │
│  ┌─────────────────────┐    ┌─────────────────────┐         │
│  │   Device A          │    │   Device B          │         │
│  │   Ed25519 Key       │    │   Ed25519 Key       │         │
│  │   (permanent)       │    │   (permanent)       │         │
│  │                     │    │                     │         │
│  │   Signs/Verifies    │◄──►│   Signs/Verifies    │         │
│  └─────────────────────┘    └─────────────────────┘         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    SESSION LAYER                            │
│                                                             │
│    ┌──────────────┐ ECDH Exchange ┌──────────────┐          │
│    │ Ephemeral    │◄─────────────►│ Ephemeral    │          │
│    │ X25519 Key   │               │ X25519 Key   │          │
│    └──────┬───────┘               └──────┬───────┘          │
│           │                              │                  │
│           └──────────┐  ┌────────────────┘                  │
│                      ▼  ▼                                   │
│              ┌──────────────────┐                           │
│              │  Session Key     │                           │
│              │  AES-256-GCM     │                           │
│              │  (per-connection)│                           │
│              └──────────────────┘                           │
└─────────────────────────────────────────────────────────────┘
```

## Identity Keys

### Purpose
- **Authentication**: Prove device identity to peers
- **Signing**: Sign messages and data transfers
- **Trust anchor**: Root of trust for the device

### Characteristics
| Property | Value |
|----------|-------|
| Algorithm | Ed25519 |
| Key Size | 256 bits (32 bytes public, 64 bytes private) |
| Lifespan | Years (until device decommissioned) |
| Storage | Encrypted at rest (AES-256-GCM) |
| Backup | Exportable with password protection |

### Key Generation
```cpp
// Generate on first run or device reset
keyManager.generateIdentityKey("MyLaptop");

// Generates:
// - Ed25519 keypair
// - Key ID (SHA256 hash of public key)
// - Human-readable fingerprint
```

### Fingerprint Verification
Identity keys have a human-readable fingerprint for out-of-band verification:

```
SHA256:Ab3C:D4eF:5678:9012:3456:7890:ABCD:EF12
```

Users should verify fingerprints through a separate channel (phone call, in person, etc.) before trusting a peer.

## Session Keys

### Purpose
- **Encryption**: Encrypt data in transit
- **Forward secrecy**: Compromise of identity key doesn't expose past communications
- **Performance**: Symmetric encryption is faster than asymmetric

### Characteristics
| Property | Value |
|----------|-------|
| Algorithm | AES-256-GCM |
| Key Size | 256 bits (32 bytes) |
| Lifespan | 24 hours default (configurable) |
| Rotation | Automatic based on time, data volume, or message count |
| Storage | Memory only (not persisted) |

### Key Derivation
Session keys are derived using ECDH + HKDF:

```cpp
// Each peer generates ephemeral X25519 keypair
auto [ephemeralPub, ephemeralPriv] = generateEphemeralKeyPair();

// Exchange public keys
send(ephemeralPub);
peerPub = receive();

// ECDH to get shared secret
sharedSecret = X25519(ephemeralPriv, peerPub);

// HKDF to derive session key
sessionKey = HKDF(sharedSecret, salt, "SentinelFS-Session");
```

### Rotation Policy
Session keys are automatically rotated when:

1. **Time-based**: After 24 hours (default)
2. **Volume-based**: After 1 GB encrypted
3. **Message-based**: After 1 million messages
4. **Manual**: Forced rotation on suspicion of compromise

## Session Codes (Bootstrap)

Session codes provide a simple mechanism for initial device pairing:

```
┌────────────────────────────────────────────────────────────┐
│                 INITIAL PAIRING FLOW                       │
│                                                            │
│  1. Device A generates session code: ABC-123               │
│                                                            │
│  2. User enters ABC-123 on Device B                        │
│                                                            │
│  3. Devices establish temporary connection using:          │
│     PBKDF2(session_code, salt, 100000) → temp_key          │
│                                                            │
│  4. Over encrypted channel, exchange identity keys         │
│                                                            │
│  5. User verifies fingerprints match                       │
│                                                            │
│  6. Devices switch to identity-based authentication        │
│                                                            │
│  7. Session code is invalidated                            │
└────────────────────────────────────────────────────────────┘
```

### Session Code vs Identity Key

| Aspect | Session Code | Identity Key |
|--------|--------------|--------------|
| Entropy | ~31 bits (6 chars) | 256 bits |
| Lifespan | Minutes | Years |
| Purpose | Initial pairing | Ongoing auth |
| Security | Low (brute-forceable) | High |
| UX | Easy to type | Fingerprint verify |

## Implementation Classes

### KeyManager
Main interface for all key operations:

```cpp
class KeyManager {
    // Identity management
    bool generateIdentityKey(const std::string& deviceName);
    bool loadIdentityKey();
    std::vector<uint8_t> getPublicKey() const;
    std::string getFingerprint() const;
    
    // Signing
    std::vector<uint8_t> sign(const std::vector<uint8_t>& data);
    bool verify(const std::vector<uint8_t>& data,
                const std::vector<uint8_t>& signature,
                const std::vector<uint8_t>& peerPublicKey);
    
    // Peer management
    bool addPeerKey(const std::string& peerId,
                    const std::vector<uint8_t>& publicKey,
                    bool verified = false);
    void markPeerKeyVerified(const std::string& peerId);
    
    // Session keys
    std::string deriveSessionKey(const std::string& peerId,
                                  const std::vector<uint8_t>& peerPublicKey,
                                  bool isInitiator);
    std::vector<uint8_t> getSessionKey(const std::string& peerId);
    bool sessionNeedsRotation(const std::string& peerId);
};
```

### FileKeyStore
Encrypted storage backend:

```cpp
class FileKeyStore : public IKeyStore {
    // Keys encrypted at rest with master password
    // Uses AES-256-GCM for encryption
    // PBKDF2 with 100,000 iterations for key derivation
    
    bool store(const std::string& keyId,
               const std::vector<uint8_t>& keyData,
               const KeyInfo& info);
    std::vector<uint8_t> load(const std::string& keyId);
};
```

## Security Considerations

### Key Storage
- Private keys encrypted with AES-256-GCM
- Master password derived using PBKDF2 (100,000 iterations)
- Key files have restrictive permissions (0600)
- Memory cleared with `OPENSSL_cleanse()` after use

### Forward Secrecy
- Ephemeral keys generated for each session
- Session keys not persisted to disk
- Compromise of identity key doesn't expose past communications

### Key Compromise Recovery
1. Generate new identity key
2. Notify trusted peers of key change
3. Re-verify fingerprints
4. Old sessions automatically invalidated

## Configuration

```properties
# sentinel.conf

# Key storage directory
key_store_path=/var/lib/sentinelfs/keys

# Session key lifetime (seconds)
session_key_lifetime=86400

# Enable key rotation notifications
key_rotation_notify=true

# Minimum session key rotation interval (seconds)
min_rotation_interval=3600
```

## Future Enhancements

1. **Hardware security modules**: Support for TPM/HSM storage
2. **Key escrow**: Optional backup to trusted party
3. **Group keys**: Efficient multi-party encryption
4. **Post-quantum**: Migration path to PQ-safe algorithms
5. **WebAuthn integration**: Hardware key support for GUI
