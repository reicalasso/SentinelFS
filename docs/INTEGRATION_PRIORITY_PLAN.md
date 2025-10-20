# 🎯 SentinelFS-Neo Integration Priority Plan

**Option B Implementation - Critical 100 Functions in 12 Weeks**

---

## 📅 Week-by-Week Breakdown

### 🔥 **PHASE 1: SECURITY (Week 1-2) - CRITICAL**

#### Week 1: TLS/SSL Infrastructure
**Goal:** Enable encrypted communication between peers

**Functions to Integrate:**
```cpp
// src/net/secure_transfer.hpp
✅ SecureTransfer::SecureTransfer()
✅ SecureTransfer::initTLS()
✅ SecureTransfer::encrypt(const std::vector<uint8_t>& data)
✅ SecureTransfer::decrypt(const std::vector<uint8_t>& data)
✅ SecureTransfer::sendEncrypted(int socket, const std::vector<uint8_t>& data)
✅ SecureTransfer::receiveEncrypted(int socket)
✅ SecureTransfer::handshake(int socket)
✅ SecureTransfer::cleanup()
```

**Integration Points:**
```cpp
// In src/net/transfer.cpp
#include "secure_transfer.hpp"

class Transfer {
    SecureTransfer secureLayer;  // Add member

public:
    void sendFile(const std::string& path, const PeerInfo& peer) {
        // Existing code...
        secureLayer.initTLS();
        secureLayer.handshake(socket);
        
        // Replace raw send with:
        secureLayer.sendEncrypted(socket, fileData);
    }
};
```

**Testing:**
```bash
# Verify encryption working
./sentinelfs-neo --session TEST --verbose
# Check logs for "TLS handshake successful"
```

**Acceptance Criteria:**
- ✅ All peer connections use TLS 1.3
- ✅ No plaintext data transmission
- ✅ Handshake completes within 500ms
- ✅ Encrypted transfer overhead < 10%

**Effort:** 16 hours  
**Lines Changed:** ~200

---

#### Week 2: Certificate Management & File Encryption
**Goal:** PKI infrastructure and at-rest encryption

**Functions to Integrate:**
```cpp
// src/security/security_manager.hpp
✅ SecurityManager::createCertificate(const std::string& commonName)
✅ SecurityManager::validateCertificate(const std::string& cert)
✅ SecurityManager::addPeerCertificate(const std::string& peerId, const std::string& cert)
✅ SecurityManager::getPeerCertificate(const std::string& peerId)
✅ SecurityManager::revokeCertificate(const std::string& cert)
✅ SecurityManager::encryptFile(const std::string& path, const std::string& password)
✅ SecurityManager::decryptFile(const std::string& path, const std::string& password)
✅ SecurityManager::signData(const std::vector<uint8_t>& data)
✅ SecurityManager::verifySignature(const std::vector<uint8_t>& data, const std::vector<uint8_t>& signature)
```

**Integration Points:**
```cpp
// In src/app/main.cpp (startup)
SecurityManager& secMgr = SecurityManager::getInstance();

// Generate certificate if doesn't exist
if (!filesystem::exists(config.certPath)) {
    logger.info("Generating self-signed certificate...");
    auto cert = secMgr.createCertificate(config.deviceId);
    // Save to config.certPath
}

// In src/net/discovery.cpp (peer validation)
void Discovery::handlePeerDiscovery(const std::string& peerId, const std::string& cert) {
    if (secMgr.validateCertificate(cert)) {
        secMgr.addPeerCertificate(peerId, cert);
        peers[peerId] = { /* peer info */ };
        logger.info("Peer " + peerId + " authenticated");
    }
}

// In src/sync/sync_manager.cpp (file encryption)
void SyncManager::syncFile(const std::string& path) {
    if (config.encryptFiles) {
        secMgr.encryptFile(path, config.encryptionKey);
    }
    // ... transfer encrypted file
}
```

**Testing:**
```bash
# Test certificate generation
./sentinelfs-neo --generate-cert --device-id TEST-001

# Test file encryption
./sentinelfs-neo --encrypt-file /tmp/test.txt --password secret123

# Verify encrypted file unreadable
cat /tmp/test.txt.enc  # Should be binary gibberish
```

**Acceptance Criteria:**
- ✅ Self-signed certificates generated on first run
- ✅ Peer certificates validated before connection
- ✅ Invalid certificates rejected
- ✅ Files encrypted with AES-256-GCM
- ✅ Digital signatures verified on all transfers

**Effort:** 20 hours  
**Lines Changed:** ~300

---

### 🔧 **PHASE 2: CORE FUNCTIONALITY (Week 3-4)**

#### Week 3: Logging & CLI
**Goal:** Production-grade observability and user interface

**Functions to Integrate:**
```cpp
// src/app/logger.hpp
✅ Logger::Logger()
✅ Logger::~Logger()
✅ Logger::setLevel(LogLevel level)
✅ Logger::setLogFile(const std::string& path)
✅ Logger::debug(const std::string& message)
✅ Logger::info(const std::string& message)
✅ Logger::warning(const std::string& message)
✅ Logger::error(const std::string& message)
✅ Logger::logInternal(LogLevel level, const std::string& msg)

// src/app/cli.hpp
✅ CLI::CLI(int argc, char** argv)
✅ CLI::parseArguments()
✅ CLI::printUsage()
✅ CLI::printVersion()
✅ CLI::getConfig()
```

**Integration Points:**
```cpp
// REPLACE ALL EXISTING CODE IN src/app/main.cpp

#include "logger.hpp"
#include "cli.hpp"

int main(int argc, char* argv[]) {
    // CLI parsing
    CLI cli(argc, argv);
    if (!cli.parseArguments()) {
        cli.printUsage();
        return 1;
    }
    
    // Logger initialization
    Logger& logger = Logger::getInstance();
    logger.setLevel(cli.getConfig().verbose ? Logger::DEBUG : Logger::INFO);
    logger.setLogFile("/var/log/sentinelfs/sentinelfs-neo.log");
    
    logger.info("SentinelFS-Neo v1.0.0 starting...");
    logger.debug("Session: " + cli.getConfig().sessionId);
    
    // REPLACE ALL printf() with logger.info()
    // REPLACE ALL std::cerr with logger.error()
    // REPLACE ALL std::cout with logger.debug()
    
    // ... rest of main
}
```

**Example Replacements:**
```cpp
// BEFORE:
printf("Connected to peer: %s\n", peerId.c_str());

// AFTER:
logger.info("Connected to peer: " + peerId);
```

**Testing:**
```bash
# Test CLI
./sentinelfs-neo --help
./sentinelfs-neo --version
./sentinelfs-neo --session TEST --path /tmp/sync --verbose

# Check log file
tail -f /var/log/sentinelfs/sentinelfs-neo.log
```

**Acceptance Criteria:**
- ✅ All printf/cout replaced with Logger
- ✅ Log levels: DEBUG, INFO, WARNING, ERROR
- ✅ Log rotation (max 10MB per file, 5 files)
- ✅ CLI provides --help, --version, all config options
- ✅ Structured log format: [TIMESTAMP] [LEVEL] message

**Effort:** 12 hours  
**Lines Changed:** ~150

---

#### Week 4: FileWatcher & Compressor
**Goal:** Real-time monitoring and bandwidth optimization

**Functions to Integrate:**
```cpp
// src/fs/watcher.hpp
✅ FileWatcher::FileWatcher(const std::string& path)
✅ FileWatcher::start()
✅ FileWatcher::stop()
✅ FileWatcher::watchLoop()
✅ FileWatcher::onFileChanged(const std::string& path)
✅ FileWatcher::scanDirectory()

// src/fs/compressor.hpp
✅ Compressor::Compressor()
✅ Compressor::compress(const std::vector<uint8_t>& data)
✅ Compressor::decompress(const std::vector<uint8_t>& data)
✅ Compressor::compressFile(const std::string& path)
✅ Compressor::decompressFile(const std::string& path)
✅ Compressor::compressGzip(const std::vector<uint8_t>& data)
✅ Compressor::compressZstd(const std::vector<uint8_t>& data)
```

**Integration Points:**
```cpp
// In src/app/main.cpp
#include "fs/watcher.hpp"

FileWatcher watcher(config.syncPath);
watcher.start();  // Starts background thread

// Watcher callback integration
void onFileChangedCallback(const std::string& path) {
    logger.info("File changed: " + path);
    fileQueue.enqueue({path, SyncAction::UPLOAD});
}

// In src/net/transfer.cpp
#include "fs/compressor.hpp"

void Transfer::sendFile(const std::string& path, const PeerInfo& peer) {
    auto data = readFile(path);
    
    // Compress before sending
    Compressor comp;
    auto compressed = comp.compressZstd(data);  // Use Zstandard for speed
    
    logger.debug("Compressed " + path + ": " + 
                 std::to_string(data.size()) + " -> " + 
                 std::to_string(compressed.size()) + 
                 " (" + std::to_string(100 - compressed.size() * 100 / data.size()) + "% saved)");
    
    secureLayer.sendEncrypted(socket, compressed);
}

void Transfer::receiveFile(int socket, const std::string& path) {
    auto compressed = secureLayer.receiveEncrypted(socket);
    
    Compressor comp;
    auto data = comp.decompress(compressed);
    
    writeFile(path, data);
}
```

**Testing:**
```bash
# Test file watcher
echo "test" > /tmp/sync/test.txt
# Check logs for "File changed: /tmp/sync/test.txt"

# Test compression
dd if=/dev/urandom of=/tmp/test.bin bs=1M count=10
./sentinelfs-neo --compress-file /tmp/test.bin
# Check compression ratio in logs
```

**Acceptance Criteria:**
- ✅ FileWatcher detects changes within 1 second
- ✅ Compression ratio > 60% for text files
- ✅ Zstandard compression < 50ms per MB
- ✅ Automatic decompression on receive
- ✅ Bandwidth savings > 70% for source code

**Effort:** 16 hours  
**Lines Changed:** ~250

---

### 🌐 **PHASE 3: NETWORK OPTIMIZATION (Week 5-6)**

#### Week 5: NAT Traversal
**Goal:** Universal peer connectivity behind firewalls

**Functions to Integrate:**
```cpp
// src/net/nat_traversal.hpp
✅ NATTraversal::NATTraversal()
✅ NATTraversal::detectNATType()
✅ NATTraversal::performHolePunch(const std::string& peerIp, int peerPort)
✅ NATTraversal::setupUPnP()
✅ NATTraversal::performSTUN()
✅ NATTraversal::getExternalIP()
✅ NATTraversal::getExternalPort()
```

**Integration Points:**
```cpp
// In src/net/discovery.cpp
#include "nat_traversal.hpp"

class Discovery {
    NATTraversal natTraversal;

public:
    void start() {
        // Detect NAT situation
        auto natType = natTraversal.detectNATType();
        logger.info("NAT type: " + natType);
        
        // Try UPnP first (easy)
        if (natTraversal.setupUPnP()) {
            logger.info("UPnP port mapping successful");
        } else {
            // Fall back to STUN
            logger.warning("UPnP failed, using STUN...");
            natTraversal.performSTUN();
        }
        
        auto externalIP = natTraversal.getExternalIP();
        auto externalPort = natTraversal.getExternalPort();
        
        logger.info("External address: " + externalIP + ":" + std::to_string(externalPort));
        
        // Broadcast this in discovery
        broadcastPresence(externalIP, externalPort);
    }
    
    void connectToPeer(const PeerInfo& peer) {
        if (peer.behindNAT) {
            logger.info("Performing hole punch to " + peer.id);
            if (natTraversal.performHolePunch(peer.ip, peer.port)) {
                logger.info("Hole punch successful");
            } else {
                logger.error("Hole punch failed, trying relay...");
                // Fall back to relay server
            }
        }
        // ... normal connection
    }
};
```

**Testing:**
```bash
# Test behind NAT
./sentinelfs-neo --session NAT-TEST --detect-nat

# Verify external IP detection
./sentinelfs-neo --show-external-ip

# Test UPnP
./sentinelfs-neo --test-upnp
```

**Acceptance Criteria:**
- ✅ Detects NAT type (Full Cone, Restricted, Port Restricted, Symmetric)
- ✅ UPnP works on 80% of home routers
- ✅ STUN fallback works on remaining 20%
- ✅ Hole punching succeeds for non-symmetric NAT
- ✅ Connection timeout < 5 seconds

**Effort:** 20 hours  
**Lines Changed:** ~300

---

#### Week 6: Mesh Optimization & Bandwidth Throttling
**Goal:** Efficient topology and network fairness

**Functions to Integrate:**
```cpp
// src/net/remesh.hpp
✅ Remesh::Remesh()
✅ Remesh::start()
✅ Remesh::stop()
✅ Remesh::evaluateAndOptimize()
✅ Remesh::updatePeerLatency(const std::string& peerId, int latency)
✅ Remesh::updatePeerBandwidth(const std::string& peerId, double bandwidth)
✅ Remesh::getOptimalTopology()
✅ Remesh::needsRemesh()

// src/sync/bandwidth_throttling.hpp
✅ BandwidthThrottler::BandwidthThrottler()
✅ BandwidthThrottler::setMaxBandwidth(size_t bytesPerSecond)
✅ BandwidthThrottler::throttle(size_t dataSize)
✅ BandwidthThrottler::getCurrentUsage()
✅ BandwidthThrottler::adjustRate()
```

**Integration Points:**
```cpp
// In src/app/main.cpp
#include "net/remesh.hpp"
#include "sync/bandwidth_throttling.hpp"

Remesh meshOptimizer;
BandwidthThrottler throttler;

meshOptimizer.start();  // Background optimization
throttler.setMaxBandwidth(10 * 1024 * 1024);  // 10 MB/s

// In src/net/transfer.cpp
void Transfer::sendData(int socket, const std::vector<uint8_t>& data) {
    // Measure latency
    auto start = steady_clock::now();
    send(socket, data.data(), data.size(), 0);
    auto latency = duration_cast<milliseconds>(steady_clock::now() - start).count();
    
    // Update mesh optimizer
    meshOptimizer.updatePeerLatency(peerId, latency);
    meshOptimizer.updatePeerBandwidth(peerId, data.size() / (latency / 1000.0));
    
    // Throttle if needed
    throttler.throttle(data.size());
}

// Periodic optimization (every 30 seconds)
if (meshOptimizer.needsRemesh()) {
    auto newTopology = meshOptimizer.getOptimalTopology();
    logger.info("Remeshing to optimal topology...");
    applyTopology(newTopology);
}
```

**Testing:**
```bash
# Test bandwidth throttling
./sentinelfs-neo --max-bandwidth 1M  # Limit to 1 MB/s
# Transfer large file, verify speed limit respected

# Test mesh optimization
./sentinelfs-neo --session MESH --peers 10
# Check logs for "Remeshing to optimal topology"
```

**Acceptance Criteria:**
- ✅ Bandwidth throttling accurate within 5%
- ✅ Mesh re-optimization every 30 seconds
- ✅ Prefer low-latency peers (< 50ms)
- ✅ Balance load across peers
- ✅ Topology converges to optimal within 2 minutes

**Effort:** 18 hours  
**Lines Changed:** ~280

---

### 💾 **PHASE 4: DATABASE INTEGRATION (Week 7-8)**

#### Week 7: Transactions & Batch Operations
**Goal:** ACID guarantees and performance

**Functions to Integrate:**
```cpp
// src/db/db.hpp
✅ MetadataDB::beginTransaction()
✅ MetadataDB::commit()
✅ MetadataDB::rollback()
✅ MetadataDB::addFilesBatch(const std::vector<FileInfo>& files)
✅ MetadataDB::updateFilesBatch(const std::vector<FileInfo>& files)
```

**Integration Points:**
```cpp
// In src/sync/sync_manager.cpp
void SyncManager::syncDirectory(const std::string& path) {
    std::vector<FileInfo> files;
    
    // Collect all files
    for (auto& entry : filesystem::recursive_directory_iterator(path)) {
        files.push_back({/* file info */});
    }
    
    // Batch insert with transaction
    db.beginTransaction();
    try {
        db.addFilesBatch(files);  // Insert all at once
        db.commit();
        logger.info("Added " + std::to_string(files.size()) + " files to DB");
    } catch (const std::exception& e) {
        db.rollback();
        logger.error("Transaction failed: " + std::string(e.what()));
    }
}

void SyncManager::updateMultipleFiles(const std::vector<FileInfo>& files) {
    db.beginTransaction();
    try {
        db.updateFilesBatch(files);
        db.commit();
    } catch (...) {
        db.rollback();
        throw;
    }
}
```

**Testing:**
```bash
# Test batch insert
mkdir -p /tmp/sync/big_test
for i in {1..1000}; do touch /tmp/sync/big_test/file$i.txt; done
./sentinelfs-neo --session BATCH --path /tmp/sync/big_test
# Check logs for "Added 1000 files to DB"

# Test transaction rollback
# Simulate crash during insert
kill -9 $(pidof sentinelfs-neo)
# Restart and verify DB consistent
```

**Acceptance Criteria:**
- ✅ Batch insert 1000 files in < 100ms
- ✅ Transaction rollback on error
- ✅ No partial commits
- ✅ Database consistent after crash
- ✅ 10x performance improvement over individual inserts

**Effort:** 12 hours  
**Lines Changed:** ~150

---

#### Week 8: CRUD Operations & Query Helpers
**Goal:** Full database functionality

**Functions to Integrate:**
```cpp
// src/db/db.hpp
✅ MetadataDB::addFile(const FileInfo& file)
✅ MetadataDB::updateFile(const FileInfo& file)
✅ MetadataDB::deleteFile(const std::string& path)
✅ MetadataDB::getFile(const std::string& path)
✅ MetadataDB::getAllFiles()
✅ MetadataDB::getFilesModifiedAfter(const std::string& timestamp)
✅ MetadataDB::getFilesByDevice(const std::string& deviceId)
✅ MetadataDB::getFilesByHashPrefix(const std::string& prefix)
✅ MetadataDB::addPeer(const PeerInfo& peer)
✅ MetadataDB::updatePeer(const PeerInfo& peer)
✅ MetadataDB::removePeer(const std::string& peerId)
```

**Integration Points:**
```cpp
// In src/fs/watcher.cpp (file change detection)
void FileWatcher::onFileChanged(const std::string& path) {
    FileInfo file = getFileInfo(path);
    
    if (db.getFile(path).path.empty()) {
        db.addFile(file);
        logger.debug("New file tracked: " + path);
    } else {
        db.updateFile(file);
        logger.debug("Updated file: " + path);
    }
}

void FileWatcher::onFileDeleted(const std::string& path) {
    db.deleteFile(path);
    logger.debug("Removed file from tracking: " + path);
}

// In src/sync/sync_manager.cpp (incremental sync)
void SyncManager::incrementalSync() {
    auto lastSyncTime = getLastSyncTimestamp();
    
    // Get only files modified since last sync
    auto changedFiles = db.getFilesModifiedAfter(lastSyncTime);
    
    logger.info("Syncing " + std::to_string(changedFiles.size()) + " changed files");
    
    for (const auto& file : changedFiles) {
        transfer.sendFile(file.path, peer);
    }
}

// In src/net/discovery.cpp (peer tracking)
void Discovery::onPeerDiscovered(const PeerInfo& peer) {
    if (db.getPeer(peer.id).id.empty()) {
        db.addPeer(peer);
        logger.info("New peer: " + peer.id);
    } else {
        db.updatePeer(peer);
    }
}

void Discovery::onPeerDisconnected(const std::string& peerId) {
    db.removePeer(peerId);
    logger.warning("Peer disconnected: " + peerId);
}
```

**Testing:**
```bash
# Test CRUD
./sentinelfs-neo --session CRUD --path /tmp/sync

# Create file
echo "test" > /tmp/sync/test.txt
# Check: SELECT * FROM files WHERE path = 'test.txt'

# Update file
echo "updated" > /tmp/sync/test.txt
# Check: version incremented

# Delete file
rm /tmp/sync/test.txt
# Check: file removed from DB
```

**Acceptance Criteria:**
- ✅ All file operations reflected in DB
- ✅ Incremental sync queries < 10ms
- ✅ Peer tracking accurate
- ✅ Query by hash works (deduplication)
- ✅ Query by device works (multi-device sync)

**Effort:** 16 hours  
**Lines Changed:** ~220

---

### 🚀 **PHASE 5: ADVANCED FEATURES (Week 9-10)**

#### Week 9: Delta Sync Engine
**Goal:** Efficient large file sync with rsync-like algorithm

**Functions to Integrate:**
```cpp
// src/fs/delta_engine.hpp
✅ DeltaEngine::DeltaEngine()
✅ DeltaEngine::generateDelta(const std::string& oldPath, const std::string& newPath)
✅ DeltaEngine::applyDelta(const std::string& basePath, const Delta& delta)
✅ DeltaEngine::computeSignature(const std::string& path)
✅ DeltaEngine::computeChecksum(const std::vector<uint8_t>& data)
```

**Integration Points:**
```cpp
// In src/sync/sync_manager.cpp
#include "fs/delta_engine.hpp"

void SyncManager::syncFile(const std::string& path) {
    DeltaEngine deltaEngine;
    
    // Check if peer has old version
    auto peerVersion = peer.getFileVersion(path);
    auto localVersion = db.getFile(path);
    
    if (!peerVersion.hash.empty() && peerVersion.hash != localVersion.hash) {
        // Peer has old version, send delta
        logger.info("Generating delta for " + path);
        
        auto delta = deltaEngine.generateDelta(peerVersion.path, localVersion.path);
        
        logger.info("Delta size: " + std::to_string(delta.size()) + 
                    " (full file: " + std::to_string(localVersion.size) + ")");
        
        transfer.sendDelta(delta, peer);
    } else {
        // Full file transfer
        transfer.sendFile(path, peer);
    }
}

void SyncManager::receiveDelta(const Delta& delta, const std::string& path) {
    DeltaEngine deltaEngine;
    
    // Apply delta to existing file
    deltaEngine.applyDelta(path, delta);
    
    logger.info("Applied delta to " + path);
}
```

**Testing:**
```bash
# Test delta generation
cp /tmp/large.txt /tmp/large_old.txt
echo "new line" >> /tmp/large.txt

./sentinelfs-neo --generate-delta /tmp/large_old.txt /tmp/large.txt
# Check delta size << full file size

# Test large file sync efficiency
dd if=/dev/urandom of=/tmp/10mb.bin bs=1M count=10
# Modify 1% of file
dd if=/dev/urandom of=/tmp/10mb.bin bs=1K count=100 seek=5000 conv=notrunc
# Sync should transfer ~100KB instead of 10MB
```

**Acceptance Criteria:**
- ✅ Delta size < 5% of full file for small changes
- ✅ Delta generation < 100ms per MB
- ✅ Delta application < 50ms per MB
- ✅ Checksums verified (no corruption)
- ✅ 20x bandwidth savings for large modified files

**Effort:** 18 hours  
**Lines Changed:** ~280

---

#### Week 10: Conflict Resolution & File Locking
**Goal:** Data integrity and concurrent access safety

**Functions to Integrate:**
```cpp
// src/fs/conflict_resolver.hpp
✅ ConflictResolver::ConflictResolver()
✅ ConflictResolver::detectConflict(const FileInfo& local, const FileInfo& remote)
✅ ConflictResolver::resolveConflict(const Conflict& conflict)
✅ ConflictResolver::autoResolve(const Conflict& conflict)
✅ ConflictResolver::createConflictCopy(const std::string& path)

// src/fs/file_locker.hpp
✅ FileLock::FileLock(const std::string& path)
✅ FileLock::acquire()
✅ FileLock::release()
✅ FileLock::isLocked()
✅ FileLock::getLockOwner()
```

**Integration Points:**
```cpp
// In src/sync/sync_manager.cpp
#include "fs/conflict_resolver.hpp"
#include "fs/file_locker.hpp"

void SyncManager::syncFile(const std::string& path) {
    // Lock file before sync
    FileLock lock(path);
    if (!lock.acquire()) {
        logger.warning("File locked by " + lock.getLockOwner() + ", skipping: " + path);
        return;
    }
    
    auto localFile = db.getFile(path);
    auto remoteFile = peer.getFileInfo(path);
    
    // Check for conflicts
    ConflictResolver resolver;
    if (resolver.detectConflict(localFile, remoteFile)) {
        logger.warning("Conflict detected for " + path);
        
        auto conflict = Conflict{localFile, remoteFile};
        
        if (config.autoResolve) {
            resolver.autoResolve(conflict);
            logger.info("Auto-resolved conflict: " + path);
        } else {
            // Create conflict copy: "file.txt.conflict-DEVICE-TIMESTAMP"
            resolver.createConflictCopy(path);
            logger.warning("Manual resolution required: " + path);
        }
    }
    
    // Proceed with sync
    transfer.sendFile(path, peer);
    
    lock.release();
}
```

**Testing:**
```bash
# Test conflict detection
# On Device A:
echo "version A" > /tmp/sync/test.txt

# On Device B (simultaneously):
echo "version B" > /tmp/sync/test.txt

# Sync both
./sentinelfs-neo --session A --path /tmp/sync
./sentinelfs-neo --session B --path /tmp/sync --peer-ip 192.168.1.100

# Check logs for "Conflict detected"
# Verify conflict copy created: test.txt.conflict-B-2025-01-15-120000

# Test file locking
# Terminal 1:
./sentinelfs-neo --lock-file /tmp/sync/large.txt --transfer

# Terminal 2 (simultaneously):
./sentinelfs-neo --lock-file /tmp/sync/large.txt --transfer
# Should see: "File locked by DEVICE-A, skipping"
```

**Acceptance Criteria:**
- ✅ Conflicts detected for simultaneous edits
- ✅ Auto-resolve: last-write-wins strategy
- ✅ Manual resolve: creates .conflict-DEVICE-TIMESTAMP copy
- ✅ File locks prevent concurrent writes
- ✅ Lock timeout after 30 seconds of inactivity
- ✅ Zero data loss in conflict scenarios

**Effort:** 20 hours  
**Lines Changed:** ~320

---

### 🧪 **PHASE 6: TESTING & DEPLOYMENT (Week 11-12)**

#### Week 11: Integration Testing
**Goal:** Verify all components work together

**Test Scenarios:**
```bash
# Test 1: End-to-End Encrypted Sync
./test_e2e_encrypted.sh
# - Device A creates files
# - Device B receives encrypted transfer
# - Verify decryption successful
# - Check TLS handshake logs

# Test 2: Real-Time Sync
./test_realtime_sync.sh
# - Start FileWatcher on both devices
# - Create/modify/delete files on A
# - Verify changes appear on B within 2 seconds

# Test 3: Large File Delta Sync
./test_delta_sync.sh
# - Create 1GB file
# - Modify 1% of file
# - Sync and verify only delta transferred
# - Check bandwidth savings

# Test 4: Conflict Resolution
./test_conflicts.sh
# - Simulate simultaneous edits
# - Verify conflict detection
# - Test auto-resolve and manual resolve
# - Check no data loss

# Test 5: NAT Traversal
./test_nat.sh
# - Run behind simulated NAT
# - Verify UPnP/STUN successful
# - Test hole punching
# - Connect peers behind different NATs

# Test 6: Performance
./test_performance.sh
# - Sync 10,000 small files
# - Sync 10 x 100MB files
# - Measure throughput, latency, CPU usage
# - Verify compression/encryption overhead < 15%

# Test 7: Database Integrity
./test_db_integrity.sh
# - Simulate crashes during transactions
# - Verify rollback worked
# - Check foreign key constraints
# - Test batch operations

# Test 8: Multi-Peer Mesh
./test_mesh.sh
# - Connect 10 peers
# - Verify optimal topology formation
# - Test bandwidth throttling
# - Check mesh re-optimization
```

**Acceptance Criteria:**
- ✅ All 8 test scenarios pass
- ✅ Zero crashes or hangs
- ✅ No memory leaks (valgrind clean)
- ✅ CPU usage < 10% during idle
- ✅ Memory usage < 100MB for 10k files

**Effort:** 24 hours  
**Lines Changed:** N/A (test scripts)

---

#### Week 12: Production Deployment
**Goal:** Production-ready system

**Deployment Checklist:**
```bash
# 1. Code Review
- Security audit of TLS implementation
- Review all error handling paths
- Check for race conditions
- Verify input validation

# 2. Documentation
- Update README.md with new features
- Create user manual
- Document API for developers
- Write troubleshooting guide

# 3. Packaging
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
strip sentinelfs-neo  # Reduce binary size
tar -czf sentinelfs-neo-v1.0.tar.gz sentinelfs-neo

# 4. Systemd Service
sudo cp sentinelfs-neo.service /etc/systemd/system/
sudo systemctl enable sentinelfs-neo
sudo systemctl start sentinelfs-neo

# 5. Logging Setup
sudo mkdir -p /var/log/sentinelfs
sudo chown sentinelfs:sentinelfs /var/log/sentinelfs

# 6. Monitoring
- Set up Prometheus metrics
- Configure Grafana dashboard
- Set up alerting for errors

# 7. Backup
- Enable automatic DB backups
- Set up version history
- Configure retention policy

# 8. Launch
sudo systemctl status sentinelfs-neo
tail -f /var/log/sentinelfs/sentinelfs-neo.log
```

**Production Metrics to Monitor:**
- Uptime percentage
- Average sync latency
- Bandwidth utilization
- Active peer count
- Conflict rate
- Error rate
- CPU/memory usage

**Effort:** 16 hours  
**Lines Changed:** N/A (ops work)

---

## 📊 Summary

### Total Effort Estimate
| Phase | Weeks | Hours | Lines Changed |
|-------|-------|-------|---------------|
| Security | 2 | 36 | ~500 |
| Core Functionality | 2 | 28 | ~400 |
| Network Optimization | 2 | 38 | ~580 |
| Database Integration | 2 | 28 | ~370 |
| Advanced Features | 2 | 38 | ~600 |
| Testing & Deployment | 2 | 40 | - |
| **TOTAL** | **12** | **208** | **~2,450** |

### Functions Integrated
- **Week 1-2:** 17 functions (Security)
- **Week 3-4:** 20 functions (Core)
- **Week 5-6:** 21 functions (Network)
- **Week 7-8:** 18 functions (Database)
- **Week 9-10:** 14 functions (Advanced)
- **TOTAL:** **90 critical functions** activated

### Before/After Comparison
| Metric | Before (86.7% dead) | After Option B |
|--------|---------------------|----------------|
| Active Functions | 121 (13.3%) | 211 (23.2%) |
| Dead Functions | 790 (86.7%) | 700 (76.8%) |
| Production Ready | ❌ NO | ✅ YES |
| Security | ❌ None | ✅ TLS + PKI |
| Performance | 🐌 Poor | 🚀 Excellent |
| Features | 🔧 Basic | ✨ Full |

### Critical Features Activated
✅ End-to-end encryption (TLS 1.3)  
✅ Certificate-based authentication  
✅ Production-grade logging  
✅ Full CLI interface  
✅ Real-time file watching  
✅ Zstandard compression (70% bandwidth savings)  
✅ NAT traversal (UPnP + STUN + hole punching)  
✅ Mesh optimization  
✅ Bandwidth throttling  
✅ Transaction support  
✅ Batch operations (10x faster)  
✅ Delta sync (20x bandwidth savings)  
✅ Conflict resolution  
✅ File locking  

### Still Dead (Acceptable)
⏳ ML layer (195 functions) - Infrastructure for future  
⏳ Version history (47 functions) - Nice-to-have  
⏳ Selective sync (28 functions) - Nice-to-have  
⏳ Resume transfers (19 functions) - Nice-to-have  

---

## 🚀 Get Started

```bash
# Fork the plan
git checkout -b feature/critical-integration

# Week 1 starts now!
cd src/net
# Follow "Week 1: TLS/SSL Infrastructure" section above

# Track progress
echo "✅ Week 1 Day 1: SecureTransfer::initTLS() integrated" >> PROGRESS.md
```

---

**Next:** Choose to proceed with Option B, or select different option from DEAD_CODE_REPORT.md
