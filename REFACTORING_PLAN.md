# SentinelFS - Comprehensive Code Review & Refactoring Plan

**Date:** November 20, 2025  
**Status:** 🔄 IN PROGRESS

## Objective
Perform a systematic, end-to-end code review and refactoring of the entire SentinelFS codebase to improve:
- Code quality and standards
- Error handling
- Memory safety
- Thread safety
- Performance
- Maintainability
- Production readiness

---

## Phase 1: Critical Issues (Priority: HIGH)

### ✅ Discovered Issues

#### 1. Plugin Path Management
**Location:** `DaemonCore.cpp`  
**Issue:** Hardcoded plugin paths (`./plugins/`) fail when daemon started from different directory  
**Status:** 🔴 CRITICAL  
**Fix Required:**
- Make plugin path configurable
- Add environment variable support
- Search multiple paths (current dir, /usr/lib/sentinelfs, etc.)

#### 2. Signal Handler Safety
**Location:** `DaemonCore.cpp:12-15`  
**Issue:** Using `std::cout` in signal handler (undefined behavior)  
**Status:** 🔴 CRITICAL  
**Fix Required:**
- Use atomic flag only
- Move logging to main loop

#### 3. Error Handling Gaps
**Location:** Various  
**Issue:** Many functions lack proper error handling  
**Examples:**
- `loadPlugins()` - Returns false but doesn't clean up partial loads
- Network operations - No timeout handling
- File operations - Incomplete error checks

#### 4. Memory Management
**Location:** Various  
**Issue:** Potential shared_ptr cycles, lack of RAII in some places  
**Examples:**
- Event bus subscriptions not cleaned up
- Socket resources may leak on error paths

#### 5. Thread Safety
**Location:** Various  
**Issue:** Shared state without proper synchronization  
**Examples:**
- `syncEnabled_` accessed without atomic or mutex in some paths
- Event bus concurrent access needs review

---

## Phase 2: Code Quality (Priority: MEDIUM)

### Issues to Address

#### 6. Logging Inconsistency
**Status:** 🟡 MEDIUM  
- Mix of `std::cout` and `Logger::instance()`
- Need to complete logging migration
- Add structured logging everywhere

#### 7. Configuration Management
**Status:** 🟡 MEDIUM  
- Config passed around, no validation
- Need config file support (JSON/YAML)
- Environment variable support

#### 8. Metrics Integration
**Status:** 🟡 MEDIUM  
- MetricsCollector created but not used everywhere
- Need to add metrics calls throughout code

#### 9. Documentation
**Status:** 🟡 MEDIUM  
- Inconsistent doc comments
- Missing API documentation
- No architectural diagrams

---

## Phase 3: Architecture (Priority: LOW)

### Improvements Needed

#### 10. Error Types
**Status:** 🟢 LOW  
- Create Result<T, Error> type
- Replace bool returns with Result
- Better error messages

#### 11. Testing
**Status:** 🟢 LOW  
- Add unit tests for all components
- Integration test improvements
- Mock implementations for plugins

#### 12. Performance
**Status:** 🟢 LOW  
- Profile code
- Optimize hot paths
- Reduce allocations

---

## Review Checklist by Component

### Core Components

- [ ] **DaemonCore** (daemon orchestration)
  - [ ] Fix signal handler
  - [ ] Fix plugin path management
  - [ ] Add error handling
  - [ ] Complete logging migration
  
- [ ] **EventBus** (pub/sub system)
  - [ ] Thread safety review
  - [ ] Subscription lifecycle
  - [ ] Performance profiling
  
- [ ] **Logger** (logging system)
  - [ ] Verify thread safety
  - [ ] Test log rotation
  - [ ] Add async logging option
  
- [ ] **MetricsCollector** (metrics)
  - [ ] Integrate throughout codebase
  - [ ] Add HTTP export endpoint
  - [ ] Test accuracy

### Network Components

- [ ] **TCPHandler** (TCP connections)
  - [ ] Timeout handling
  - [ ] Connection pooling
  - [ ] Error recovery
  
- [ ] **UDPDiscovery** (peer discovery)
  - [ ] Rate limiting
  - [ ] Error handling
  
- [ ] **HandshakeProtocol** (authentication)
  - [ ] Security review
  - [ ] Error handling
  
- [ ] **BandwidthLimiter** (rate limiting)
  - [ ] Test accuracy
  - [ ] Thread safety

### Storage Components

- [ ] **SQLiteHandler** (database)
  - [ ] Transaction support
  - [ ] Connection pooling
  - [ ] Error handling
  - [ ] Migration system
  
- [ ] **PeerManager** (peer tracking)
  - [ ] Thread safety
  - [ ] Cleanup stale peers
  
- [ ] **FileMetadataManager** (file tracking)
  - [ ] Consistency checks
  - [ ] Performance optimization
  
- [ ] **ConflictManager** (conflict resolution)
  - [ ] Complete implementation
  - [ ] Test coverage

### Filesystem Components

- [ ] **InotifyWatcher** (file monitoring)
  - [ ] Event batching
  - [ ] Error handling
  - [ ] Resource cleanup
  
- [ ] **FileHasher** (SHA-256)
  - [ ] Performance optimization
  - [ ] Error handling

### Security Components

- [ ] **Crypto** (encryption)
  - [ ] Security audit
  - [ ] Key management
  - [ ] Error handling
  
- [ ] **SessionCode** (authentication)
  - [ ] Entropy verification
  - [ ] Rate limiting
  
- [ ] **AnomalyDetector** (threat detection)
  - [ ] Tuning
  - [ ] False positive reduction

### Daemon Components

- [ ] **EventHandlers** (event routing)
  - [ ] Error handling
  - [ ] Logging completion
  
- [ ] **FileSyncHandler** (sync logic)
  - [ ] Error handling
  - [ ] Metrics integration
  
- [ ] **DeltaSyncProtocolHandler** (delta sync)
  - [ ] Error handling
  - [ ] Performance tuning
  
- [ ] **IPCHandler** (CLI communication)
  - [ ] Error handling
  - [ ] Add more commands

---

## Systematic Review Process

### Step 1: Read & Analyze (Current Phase)
- Read each file completely
- Note issues, TODOs, improvements
- Check against best practices

### Step 2: Prioritize
- Categorize issues by severity
- Create fix order

### Step 3: Fix Critical Issues
- One file at a time
- Test after each change
- Commit frequently

### Step 4: Improve Code Quality
- Apply consistent style
- Add logging
- Add metrics
- Improve documentation

### Step 5: Test & Validate
- Run all tests
- Integration testing
- Performance testing

---

## Success Criteria

- [ ] No compiler warnings
- [ ] All tests passing
- [ ] No memory leaks (valgrind)
- [ ] No data races (thread sanitizer)
- [ ] Code coverage > 80%
- [ ] Documentation complete
- [ ] Performance benchmarks met
- [ ] Security audit passed

---

## Next Steps

1. ✅ Create this refactoring plan
2. 🔄 Fix critical issues in DaemonCore
3. ⏳ Review each component systematically
4. ⏳ Apply fixes
5. ⏳ Test & validate
6. ⏳ Update documentation

---

**Note:** This is a living document. Issues will be added as discovered.
