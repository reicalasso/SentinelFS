#include "DatabaseMigrations.h"
#include "Logger.h"

namespace SentinelFS {

std::vector<DatabaseManager::Migration> DatabaseMigrations::getAllMigrations() {
    return {
        migration_v1(),
        migration_v2(),
        migration_v3(),
        migration_v4(),
        migration_v5(),
        migration_v6()
    };
}

std::string DatabaseMigrations::getInitialSchema() {
    return R"(
-- Create files table
CREATE TABLE IF NOT EXISTS files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT UNIQUE NOT NULL,
    hash TEXT,
    size INTEGER DEFAULT 0,
    modified_time INTEGER DEFAULT 0,
    created_at INTEGER DEFAULT (strftime('%s', 'now')),
    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
);

-- Create device table
CREATE TABLE IF NOT EXISTS device (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT UNIQUE NOT NULL,
    last_seen INTEGER DEFAULT (strftime('%s', 'now')),
    created_at INTEGER DEFAULT (strftime('%s', 'now'))
);

-- Create operations table
CREATE TABLE IF NOT EXISTS operations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    device_id INTEGER NOT NULL,
    op_type INTEGER NOT NULL,  -- 1=CREATE, 2=UPDATE, 3=DELETE, 4=READ, 5=WRITE, 6=RENAME, 7=MOVE
    status INTEGER NOT NULL,   -- 1=ACTIVE, 2=PENDING, 3=SYNCING, 4=COMPLETED, 5=FAILED, 6=OFFLINE, 7=PAUSED
    timestamp INTEGER DEFAULT (strftime('%s', 'now')),
    error_message TEXT,
    retry_count INTEGER DEFAULT 0,
    created_at INTEGER DEFAULT (strftime('%s', 'now')),
    FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
    FOREIGN KEY (device_id) REFERENCES device(id) ON DELETE CASCADE
);

-- Create peers table for network information
CREATE TABLE IF NOT EXISTS peers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    peer_id TEXT UNIQUE NOT NULL,
    endpoint TEXT,
    last_seen INTEGER DEFAULT (strftime('%s', 'now')),
    latency INTEGER DEFAULT 0,
    status TEXT DEFAULT 'offline',
    trust_level INTEGER DEFAULT 0,
    created_at INTEGER DEFAULT (strftime('%s', 'now')),
    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
);

-- Create sync_state table for tracking sync progress
CREATE TABLE IF NOT EXISTS sync_state (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    device_id INTEGER NOT NULL,
    last_sync_hash TEXT,
    last_sync_time INTEGER DEFAULT 0,
    sync_status TEXT DEFAULT 'pending',
    conflict_resolution TEXT,
    created_at INTEGER DEFAULT (strftime('%s', 'now')),
    updated_at INTEGER DEFAULT (strftime('%s', 'now')),
    FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
    FOREIGN KEY (device_id) REFERENCES device(id) ON DELETE CASCADE,
    UNIQUE(file_id, device_id)
);

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_files_path ON files(path);
CREATE INDEX IF NOT EXISTS idx_files_hash ON files(hash);
CREATE INDEX IF NOT EXISTS idx_files_modified ON files(modified_time);
CREATE INDEX IF NOT EXISTS idx_operations_file_id ON operations(file_id);
CREATE INDEX IF NOT EXISTS idx_operations_device_id ON operations(device_id);
CREATE INDEX IF NOT EXISTS idx_operations_status ON operations(status);
CREATE INDEX IF NOT EXISTS idx_operations_timestamp ON operations(timestamp);
CREATE INDEX IF NOT EXISTS idx_peers_peer_id ON peers(peer_id);
CREATE INDEX IF NOT EXISTS idx_peers_status ON peers(status);
CREATE INDEX IF NOT EXISTS idx_sync_state_file_device ON sync_state(file_id, device_id);

-- Create triggers for automatic timestamp updates
CREATE TRIGGER IF NOT EXISTS update_files_timestamp 
    AFTER UPDATE ON files
    BEGIN
        UPDATE files SET updated_at = strftime('%s', 'now') WHERE id = NEW.id;
    END;

CREATE TRIGGER IF NOT EXISTS update_peers_timestamp 
    AFTER UPDATE ON peers
    BEGIN
        UPDATE peers SET updated_at = strftime('%s', 'now') WHERE id = NEW.id;
    END;

CREATE TRIGGER IF NOT EXISTS update_sync_state_timestamp 
    AFTER UPDATE ON sync_state
    BEGIN
        UPDATE sync_state SET updated_at = strftime('%s', 'now') WHERE id = NEW.id;
    END;
)";
}

DatabaseManager::Migration DatabaseMigrations::migration_v1() {
    return {
        1,
        "Initial schema creation",
        getInitialSchema(),
        "DROP TABLE IF EXISTS sync_state; DROP TABLE IF EXISTS peers; DROP TABLE IF EXISTS operations; DROP TABLE IF EXISTS device; DROP TABLE IF EXISTS files;"
    };
}

DatabaseManager::Migration DatabaseMigrations::migration_v2() {
    return {
        2,
        "Add performance indexes",
        R"(
CREATE INDEX IF NOT EXISTS idx_operations_file_status ON operations(file_id, status);
CREATE INDEX IF NOT EXISTS idx_operations_device_timestamp ON operations(device_id, timestamp);
CREATE INDEX IF NOT EXISTS idx_sync_state_status ON sync_state(sync_status);
)",
        "DROP INDEX IF EXISTS idx_operations_file_status; DROP INDEX IF EXISTS idx_operations_device_timestamp; DROP INDEX IF EXISTS idx_sync_state_status;"
    };
}

DatabaseManager::Migration DatabaseMigrations::migration_v3() {
    return {
        3,
        "Add optimization triggers",
        R"(
CREATE TRIGGER IF NOT EXISTS update_files_timestamp 
    AFTER UPDATE ON files
    BEGIN
        UPDATE files SET updated_at = strftime('%s', 'now') WHERE id = NEW.id;
    END;

CREATE TRIGGER IF NOT EXISTS update_peers_timestamp 
    AFTER UPDATE ON peers
    BEGIN
        UPDATE peers SET updated_at = strftime('%s', 'now') WHERE id = NEW.id;
    END;

CREATE TRIGGER IF NOT EXISTS update_sync_state_timestamp 
    AFTER UPDATE ON sync_state
    BEGIN
        UPDATE sync_state SET updated_at = strftime('%s', 'now') WHERE id = NEW.id;
    END;
)",
        R"(
DROP TRIGGER IF EXISTS update_files_timestamp;
DROP TRIGGER IF EXISTS update_peers_timestamp;
DROP TRIGGER IF EXISTS update_sync_state_timestamp;
)"
    };
}

DatabaseManager::Migration DatabaseMigrations::migration_v4() {
    return {
        4,
        "Add file metadata tracking",
        "ALTER TABLE files ADD COLUMN file_hash TEXT; ALTER TABLE files ADD COLUMN checksum TEXT;",
        "ALTER TABLE files DROP COLUMN file_hash; ALTER TABLE files DROP COLUMN checksum;"
    };
}

DatabaseManager::Migration DatabaseMigrations::migration_v5() {
    return {
        5,
        "Add sync optimization columns",
        "ALTER TABLE sync_state ADD COLUMN priority INTEGER DEFAULT 0; ALTER TABLE sync_state ADD COLUMN batch_id TEXT;",
        "ALTER TABLE sync_state DROP COLUMN priority; ALTER TABLE sync_state DROP COLUMN batch_id;"
    };
}

DatabaseManager::Migration DatabaseMigrations::migration_v6() {
    return {
        6,
        "Add versioning and conflict tracking",
        R"(
-- Create file_versions table for tracking file history
CREATE TABLE IF NOT EXISTS file_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    device_id INTEGER NOT NULL,
    version INTEGER NOT NULL,
    hash TEXT NOT NULL,
    size INTEGER DEFAULT 0,
    modified_time INTEGER DEFAULT 0,
    content BLOB,
    is_deleted BOOLEAN DEFAULT 0,
    created_at INTEGER DEFAULT (strftime('%s', 'now')),
    FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
    FOREIGN KEY (device_id) REFERENCES device(id) ON DELETE CASCADE,
    UNIQUE(file_id, device_id, version)
);

-- Create conflicts table for tracking file conflicts
CREATE TABLE IF NOT EXISTS conflicts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    device_id INTEGER NOT NULL,
    conflict_type INTEGER NOT NULL,  -- 1=CONTENT, 2=METADATA, 3=DELETION, 4=RENAME
    local_version INTEGER,
    remote_version INTEGER,
    local_hash TEXT,
    remote_hash TEXT,
    status TEXT DEFAULT 'pending',  -- pending, resolved, ignored
    resolution_strategy TEXT,      -- local_wins, remote_wins, merge, manual
    resolved_at INTEGER,
    created_at INTEGER DEFAULT (strftime('%s', 'now')),
    updated_at INTEGER DEFAULT (strftime('%s', 'now')),
    FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
    FOREIGN KEY (device_id) REFERENCES device(id) ON DELETE CASCADE
);

-- Create merge_results table for storing 3-way merge results
CREATE TABLE IF NOT EXISTS merge_results (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    conflict_id INTEGER NOT NULL,
    base_version INTEGER NOT NULL,
    merged_hash TEXT NOT NULL,
    merge_data BLOB,
    conflicts_remaining INTEGER DEFAULT 0,
    created_at INTEGER DEFAULT (strftime('%s', 'now')),
    FOREIGN KEY (conflict_id) REFERENCES conflicts(id) ON DELETE CASCADE
);

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_file_versions_file_id ON file_versions(file_id);
CREATE INDEX IF NOT EXISTS idx_file_versions_device_id ON file_versions(device_id);
CREATE INDEX IF NOT EXISTS idx_file_versions_hash ON file_versions(hash);
CREATE INDEX IF NOT EXISTS idx_conflicts_file_id ON conflicts(file_id);
CREATE INDEX IF NOT EXISTS idx_conflicts_status ON conflicts(status);
CREATE INDEX IF NOT EXISTS idx_conflicts_created_at ON conflicts(created_at);
CREATE INDEX IF NOT EXISTS idx_merge_results_conflict_id ON merge_results(conflict_id);

-- Create triggers for automatic version management
CREATE TRIGGER IF NOT EXISTS increment_file_version
    AFTER UPDATE ON files
    WHEN NEW.hash != OLD.hash
    BEGIN
        INSERT INTO file_versions (file_id, device_id, version, hash, size, modified_time)
        VALUES (NEW.id, 
                (SELECT device_id FROM sync_state WHERE file_id = NEW.id LIMIT 1),
                COALESCE((SELECT MAX(version) FROM file_versions WHERE file_id = NEW.id), 0) + 1,
                NEW.hash, NEW.size, NEW.modified_time);
    END;
)",
        R"(
DROP TRIGGER IF EXISTS increment_file_version;
DROP TABLE IF EXISTS merge_results;
DROP TABLE IF EXISTS conflicts;
DROP TABLE IF EXISTS file_versions;
)"
    };
}

} // namespace SentinelFS
