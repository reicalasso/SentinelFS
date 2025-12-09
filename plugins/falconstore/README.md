# FalconStore 🦅

**High-performance storage plugin for SentinelFS**

## Features

- **Schema Migration System** - Version-controlled database schema with up/down migrations
- **LRU Cache Layer** - In-memory caching with TTL support for frequently accessed data
- **Type-safe Query Builder** - Fluent API for building SQL queries without raw strings
- **Connection Pooling** - Thread-safe database access with shared/exclusive locks
- **Transaction Support** - ACID-compliant transactions with automatic rollback
- **WAL Mode** - Write-Ahead Logging for better concurrent performance
- **Backup/Restore** - Hot backup and restore capabilities

## Architecture

```
FalconStore
├── FalconStore.h/cpp      # Main plugin class, IStorageAPI implementation
├── QueryBuilder.h         # Type-safe SQL query builder
├── MigrationManager.h/cpp # Schema migration system
└── Cache.h                # LRU cache with TTL
```

## Usage

### Basic Operations

```cpp
auto storage = std::make_shared<FalconStore>();
storage->initialize(&eventBus);

// Save file metadata
FileMetadata file;
file.path = "/home/user/document.txt";
file.hash = "abc123...";
file.size = 1024;
file.modified = time(nullptr);
storage->saveFile(file);

// Query with builder
auto results = storage->query()
    ->select({"path", "size", "modified"})
    ->from("files")
    ->where("size", ">", 1000)
    ->orderBy("modified", OrderDirection::DESC)
    ->limit(10)
    ->execute();

while (results->next()) {
    auto& row = results->current();
    std::cout << row.getString("path") << ": " << row.getInt64("size") << " bytes\n";
}
```

### Migrations

```cpp
auto migrationManager = storage->getMigrationManager();

// Check current version
int version = migrationManager->getCurrentVersion();

// Run pending migrations
migrationManager->migrateUp();

// Rollback to specific version
migrationManager->migrateDown(3);
```

### Cache

```cpp
auto cache = storage->getCache();

// Cache a value
cache->put("user:123", userJson, std::chrono::seconds{300});

// Retrieve
auto cached = cache->get("user:123");
if (cached) {
    // Use cached value
}

// Invalidate
cache->invalidate("user:123");
cache->invalidatePrefix("user:");  // Invalidate all user entries
```

### Statistics

```cpp
auto stats = storage->getStats();
std::cout << "Total queries: " << stats.totalQueries << "\n";
std::cout << "Avg query time: " << stats.avgQueryTimeMs << "ms\n";
std::cout << "Cache hit rate: " << (stats.cache.hitRate() * 100) << "%\n";
std::cout << "DB size: " << (stats.dbSizeBytes / 1024 / 1024) << "MB\n";
```

## Configuration

```cpp
Falcon::FalconConfig config;
config.dbPath = "/path/to/database.db";
config.poolSize = 4;
config.enableCache = true;
config.cacheMaxSize = 10000;
config.cacheMaxMemory = 64 * 1024 * 1024;  // 64MB
config.cacheTTL = std::chrono::seconds{300};
config.enableWAL = true;
config.synchronous = false;  // NORMAL mode for better performance

storage->configure(config);
```

## Schema Versions

| Version | Description |
|---------|-------------|
| 1 | Initial schema - files, peers, conflicts, watched_folders |
| 2 | Threat detection - detected_threats table, Zer0 compatible |
| 3 | File versioning - file_versions table |
| 4 | Sync queue - sync_queue table with priorities |
| 5 | Activity log - activity_log table |
| 6 | Ignore patterns - ignore_patterns table |

## Performance

- **WAL Mode**: Enables concurrent reads during writes
- **Shared Mutex**: Read operations don't block each other
- **Prepared Statements**: Reusable compiled SQL
- **LRU Cache**: Reduces database hits for hot data
- **Batch Operations**: Support for bulk inserts/updates

## Thread Safety

FalconStore uses `std::shared_mutex` for thread-safe access:
- **Read operations**: Shared lock (multiple concurrent reads)
- **Write operations**: Exclusive lock (single writer)

## Comparison with Legacy Storage

| Feature | Legacy Storage | FalconStore |
|---------|---------------|-------------|
| Migration system | ❌ | ✅ |
| Query builder | ❌ | ✅ |
| Cache layer | ❌ | ✅ |
| Connection pool | ❌ | ✅ |
| Statistics | ❌ | ✅ |
| Backup/Restore | ❌ | ✅ |
| WAL mode | ❌ | ✅ |
| Thread safety | Basic | Advanced |

## License

Part of SentinelFS - MIT License
