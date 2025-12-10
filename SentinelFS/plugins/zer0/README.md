# Zer0 - Advanced Threat Detection Plugin

Zer0 is an intelligent threat detection plugin for SentinelFS that provides:

## Features

### 🔍 File Type Awareness
- **No false positives** on compressed files (PNG, JPEG, PDF, ZIP)
- Understands that images/videos/archives naturally have high entropy
- Only flags suspicious entropy in text/config files

### 🧬 Magic Byte Validation
- Detects extension spoofing (e.g., `malware.pdf.exe`)
- Identifies hidden executables disguised as documents
- Validates file content matches declared extension

### 🎯 Behavioral Analysis
- Detects ransomware patterns (mass rename, extension changes)
- Identifies suspicious process activity
- Monitors for mass file modifications
- Tracks file access patterns

### 🛡️ Threat Classification

| Level | Description | Action |
|-------|-------------|--------|
| NONE | No threat | - |
| INFO | Informational | Log |
| LOW | Low risk | Monitor |
| MEDIUM | Medium risk | Warn user |
| HIGH | High risk | Quarantine |
| CRITICAL | Critical threat | Immediate action |

### 🚨 Threat Types

- **EXTENSION_MISMATCH** - File content doesn't match extension
- **HIDDEN_EXECUTABLE** - Executable disguised as other file type
- **HIGH_ENTROPY_TEXT** - Encrypted/obfuscated text file
- **RANSOMWARE_PATTERN** - Ransomware behavior detected
- **MASS_MODIFICATION** - Too many files changed quickly
- **DOUBLE_EXTENSION** - `file.pdf.exe` pattern
- **SCRIPT_IN_DATA** - Script embedded in data file

## Configuration

```cpp
Zer0Config config;

// Entropy thresholds by file type
config.entropyThresholds[FileCategory::TEXT] = 6.0;
config.entropyThresholds[FileCategory::CONFIG] = 5.5;

// Behavioral thresholds
config.massModificationThreshold = 50;  // Files per minute
config.suspiciousRenameThreshold = 10;  // Renames per minute

// Whitelist
config.whitelistedPaths.insert("/home/user/trusted/");
config.whitelistedProcesses.insert("backup_tool");

// Actions
config.autoQuarantine = true;
config.quarantineThreshold = ThreatLevel::HIGH;
```

## API Usage

```cpp
// Analyze a file
auto result = zer0->analyzeFile("/path/to/file");

if (result.level >= ThreatLevel::MEDIUM) {
    std::cout << "Threat: " << result.description << std::endl;
    std::cout << "Confidence: " << result.confidence << std::endl;
}

// Check behavioral patterns
auto behavior = zer0->checkBehavior();
if (behavior.level >= ThreatLevel::HIGH) {
    std::cout << "Ransomware pattern detected!" << std::endl;
}

// Quarantine management
zer0->quarantineFile("/path/to/malware");
auto quarantined = zer0->getQuarantineList();
zer0->restoreFile(quarantinePath, originalPath);
```

## Comparison with Legacy ML Plugin

| Feature | Legacy ML | Zer0 |
|---------|-----------|------|
| False positives on images | ❌ High | ✅ None |
| Magic byte validation | ❌ No | ✅ Yes |
| Behavioral analysis | ⚠️ Basic | ✅ Advanced |
| Ransomware detection | ⚠️ Entropy only | ✅ Pattern-based |
| Process correlation | ❌ No | ✅ Yes |
| Whitelist support | ❌ No | ✅ Yes |
| Threat levels | 2 | 6 |

## Architecture

```
Zer0Plugin
├── MagicBytes (file type detection)
├── BehaviorAnalyzer (pattern detection)
├── EntropyAnalyzer (selective entropy check)
└── QuarantineManager (threat isolation)
```

## License

Part of SentinelFS project.
