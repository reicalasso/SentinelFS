# SentinelFS Database ER Diagram

## Overview
The SentinelFS database is designed to manage distributed file synchronization across multiple devices with version control, conflict resolution, and peer-to-peer networking capabilities.

## High-Level ER Diagram

```mermaid
erDiagram
    %% Core Entities
    files {
        int id PK
        text path UK
        text hash
        int size
        int modified_time
        int created_at
        int updated_at
        text file_hash
        text checksum
    }
    
    device {
        int id PK
        text device_id UK
        int last_seen
        int created_at
    }
    
    operations {
        int id PK
        int file_id FK
        int device_id FK
        int op_type
        int status
        int timestamp
        text error_message
        int retry_count
        int created_at
    }
    
    %% Network & Sync
    peers {
        int id PK
        text peer_id UK
        text endpoint
        int last_seen
        int latency
        text status
        int trust_level
        int created_at
        int updated_at
    }
    
    sync_state {
        int id PK
        int file_id FK
        int device_id FK
        text last_sync_hash
        int last_sync_time
        text sync_status
        text conflict_resolution
        int priority
        text batch_id
        int created_at
        int updated_at
    }
    
    %% Version Control
    file_versions {
        int id PK
        int file_id FK
        int device_id FK
        int version
        text hash
        int size
        int modified_time
        blob content
        boolean is_deleted
        int created_at
    }
    
    conflicts {
        int id PK
        int file_id FK
        int device_id FK
        int conflict_type
        int local_version
        int remote_version
        text local_hash
        text remote_hash
        text status
        text resolution_strategy
        int resolved_at
        int created_at
        int updated_at
    }
    
    merge_results {
        int id PK
        int conflict_id FK
        int base_version
        text merged_hash
        blob merge_data
        int conflicts_remaining
        int created_at
    }
    
    %% System Tables
    schema_version {
        int version PK
        text description
    }
    
    schema_migrations {
        int version PK
        text name
        int applied_at
    }
    
    %% Relationships
    files ||--o{ operations : "has"
    device ||--o{ operations : "performs"
    files ||--o{ sync_state : "synced"
    device ||--o{ sync_state : "syncs"
    files ||--o{ file_versions : "versioned"
    device ||--o{ file_versions : "versions"
    files ||--o{ conflicts : "conflicts"
    device ||--o{ conflicts : "conflicts"
    conflicts ||--|| merge_results : "resolved_by"
    
    %% Unique Constraints
    files {
        UNIQUE(path)
    }
    device {
        UNIQUE(device_id)
    }
    peers {
        UNIQUE(peer_id)
    }
    sync_state {
        UNIQUE(file_id, device_id)
    }
    file_versions {
        UNIQUE(file_id, device_id, version)
    }
```

## Detailed Table Descriptions

### Core File System Tables

#### `files`
Stores metadata for all files in the distributed filesystem.

| Column | Type | Description | Constraints |
|--------|------|-------------|-------------|
| id | INTEGER | Primary key | AUTO_INCREMENT |
| path | TEXT | Full file path | UNIQUE, NOT NULL |
| hash | TEXT | File content hash | |
| size | INTEGER | File size in bytes | DEFAULT 0 |
| modified_time | INTEGER | Last modification timestamp | DEFAULT 0 |
| created_at | INTEGER | Creation timestamp | DEFAULT (strftime('%s', 'now')) |
| updated_at | INTEGER | Last update timestamp | DEFAULT (strftime('%s', 'now')) |
| file_hash | TEXT | Additional file hash (v4) | |
| checksum | TEXT | File checksum (v4) | |

#### `device`
Represents devices participating in the distributed filesystem.

| Column | Type | Description | Constraints |
|--------|------|-------------|-------------|
| id | INTEGER | Primary key | AUTO_INCREMENT |
| device_id | TEXT | Unique device identifier | UNIQUE, NOT NULL |
| last_seen | INTEGER | Last activity timestamp | DEFAULT (strftime('%s', 'now')) |
| created_at | INTEGER | Registration timestamp | DEFAULT (strftime('%s', 'now')) |

#### `operations`
Tracks file operations across devices for synchronization.

| Column | Type | Description | Constraints |
|--------|------|-------------|-------------|
| id | INTEGER | Primary key | AUTO_INCREMENT |
| file_id | INTEGER | Reference to files | NOT NULL, FK → files(id) |
| device_id | INTEGER | Reference to device | NOT NULL, FK → device(id) |
| op_type | INTEGER | Operation type | NOT NULL (1=CREATE, 2=UPDATE, 3=DELETE, 4=READ, 5=WRITE, 6=RENAME, 7=MOVE) |
| status | INTEGER | Operation status | NOT NULL (1=ACTIVE, 2=PENDING, 3=SYNCING, 4=COMPLETED, 5=FAILED, 6=OFFLINE, 7=PAUSED) |
| timestamp | INTEGER | Operation timestamp | DEFAULT (strftime('%s', 'now')) |
| error_message | TEXT | Error details if failed | |
| retry_count | INTEGER | Number of retries | DEFAULT 0 |
| created_at | INTEGER | Record creation time | DEFAULT (strftime('%s', 'now')) |

### Network & Synchronization Tables

#### `peers`
Manages network peer information for P2P communication.

| Column | Type | Description | Constraints |
|--------|------|-------------|-------------|
| id | INTEGER | Primary key | AUTO_INCREMENT |
| peer_id | TEXT | Unique peer identifier | UNIQUE, NOT NULL |
| endpoint | TEXT | Network endpoint | |
| last_seen | INTEGER | Last contact timestamp | DEFAULT (strftime('%s', 'now')) |
| latency | INTEGER | Network latency in ms | DEFAULT 0 |
| status | TEXT | Peer status | DEFAULT 'offline' |
| trust_level | INTEGER | Trust score | DEFAULT 0 |
| created_at | INTEGER | First seen timestamp | DEFAULT (strftime('%s', 'now')) |
| updated_at | INTEGER | Last update timestamp | DEFAULT (strftime('%s', 'now')) |

#### `sync_state`
Tracks synchronization status for each file-device pair.

| Column | Type | Description | Constraints |
|--------|------|-------------|-------------|
| id | INTEGER | Primary key | AUTO_INCREMENT |
| file_id | INTEGER | Reference to files | NOT NULL, FK → files(id) |
| device_id | INTEGER | Reference to device | NOT NULL, FK → device(id) |
| last_sync_hash | TEXT | Hash of last synced version | |
| last_sync_time | INTEGER | Last successful sync | DEFAULT 0 |
| sync_status | TEXT | Current sync status | DEFAULT 'pending' |
| conflict_resolution | TEXT | Conflict resolution strategy | |
| priority | INTEGER | Sync priority | DEFAULT 0 (v5) |
| batch_id | TEXT | Batch identifier | (v5) |
| created_at | INTEGER | Record creation | DEFAULT (strftime('%s', 'now')) |
| updated_at | INTEGER | Last update | DEFAULT (strftime('%s', 'now')) |

### Version Control Tables (v6+)

#### `file_versions`
Stores version history for files.

| Column | Type | Description | Constraints |
|--------|------|-------------|-------------|
| id | INTEGER | Primary key | AUTO_INCREMENT |
| file_id | INTEGER | Reference to files | NOT NULL, FK → files(id) |
| device_id | INTEGER | Reference to device | NOT NULL, FK → device(id) |
| version | INTEGER | Version number | NOT NULL |
| hash | TEXT | Content hash | NOT NULL |
| size | INTEGER | File size | DEFAULT 0 |
| modified_time | INTEGER | Modification timestamp | DEFAULT 0 |
| content | BLOB | File content (optional) | |
| is_deleted | BOOLEAN | Deletion marker | DEFAULT 0 |
| created_at | INTEGER | Version creation | DEFAULT (strftime('%s', 'now')) |

#### `conflicts`
Tracks file conflicts between devices.

| Column | Type | Description | Constraints |
|--------|------|-------------|-------------|
| id | INTEGER | Primary key | AUTO_INCREMENT |
| file_id | INTEGER | Reference to files | NOT NULL, FK → files(id) |
| device_id | INTEGER | Reference to device | NOT NULL, FK → device(id) |
| conflict_type | INTEGER | Type of conflict | NOT NULL (1=CONTENT, 2=METADATA, 3=DELETION, 4=RENAME) |
| local_version | INTEGER | Local version number | |
| remote_version | INTEGER | Remote version number | |
| local_hash | TEXT | Local content hash | |
| remote_hash | TEXT | Remote content hash | |
| status | TEXT | Conflict status | DEFAULT 'pending' |
| resolution_strategy | TEXT | How to resolve | |
| resolved_at | INTEGER | Resolution timestamp | |
| created_at | INTEGER | Conflict detection | DEFAULT (strftime('%s', 'now')) |
| updated_at | INTEGER | Last update | DEFAULT (strftime('%s', 'now')) |

#### `merge_results`
Stores results of 3-way merge operations.

| Column | Type | Description | Constraints |
|--------|------|-------------|-------------|
| id | INTEGER | Primary key | AUTO_INCREMENT |
| conflict_id | INTEGER | Reference to conflicts | NOT NULL, FK → conflicts(id) |
| base_version | INTEGER | Base version for merge | NOT NULL |
| merged_hash | TEXT | Merged content hash | NOT NULL |
| merge_data | BLOB | Merge metadata | |
| conflicts_remaining | INTEGER | Unresolved conflicts | DEFAULT 0 |
| created_at | INTEGER | Merge timestamp | DEFAULT (strftime('%s', 'now')) |

### System Tables

#### `schema_version`
Tracks database schema version (core system).

| Column | Type | Description | Constraints |
|--------|------|-------------|-------------|
| version | INTEGER | Schema version | PRIMARY KEY |
| description | TEXT | Version description | |

#### `schema_migrations`
Tracks migration history (Falcon plugin).

| Column | Type | Description | Constraints |
|--------|------|-------------|-------------|
| version | INTEGER | Migration version | PRIMARY KEY |
| name | TEXT | Migration name | NOT NULL |
| applied_at | INTEGER | Application timestamp | DEFAULT (strftime('%s', 'now')) |

## Database Indexes

### Performance Indexes
- `idx_files_path` ON files(path)
- `idx_files_hash` ON files(hash)
- `idx_files_modified` ON files(modified_time)
- `idx_operations_file_id` ON operations(file_id)
- `idx_operations_device_id` ON operations(device_id)
- `idx_operations_status` ON operations(status)
- `idx_operations_timestamp` ON operations(timestamp)
- `idx_operations_file_status` ON operations(file_id, status) (v2+)
- `idx_operations_device_timestamp` ON operations(device_id, timestamp) (v2+)
- `idx_peers_peer_id` ON peers(peer_id)
- `idx_peers_status` ON peers(status)
- `idx_sync_state_file_device` ON sync_state(file_id, device_id)
- `idx_sync_state_status` ON sync_state(sync_status) (v2+)
- `idx_file_versions_file_id` ON file_versions(file_id) (v6+)
- `idx_file_versions_device_id` ON file_versions(device_id) (v6+)
- `idx_file_versions_hash` ON file_versions(hash) (v6+)
- `idx_conflicts_file_id` ON conflicts(file_id) (v6+)
- `idx_conflicts_status` ON conflicts(status) (v6+)
- `idx_conflicts_created_at` ON conflicts(created_at) (v6+)
- `idx_merge_results_conflict_id` ON merge_results(conflict_id) (v6+)

## Database Triggers

### Automatic Timestamp Updates
- `update_files_timestamp`: Updates `updated_at` on file modifications
- `update_peers_timestamp`: Updates `updated_at` on peer changes
- `update_sync_state_timestamp`: Updates `updated_at` on sync state changes
- `increment_file_version`: Creates new version when file hash changes (v6+)

## Migration History

| Version | Description | Changes |
|---------|-------------|---------|
| 1 | Initial schema creation | Created core tables (files, device, operations, peers, sync_state) |
| 2 | Performance indexes | Added composite indexes for query optimization |
| 3 | Optimization triggers | Added automatic timestamp update triggers |
| 4 | File metadata tracking | Added file_hash and checksum columns to files |
| 5 | Sync optimization | Added priority and batch_id to sync_state |
| 6 | Versioning and conflicts | Added file_versions, conflicts, and merge_results tables |

## Foreign Key Constraints

All foreign key relationships use `ON DELETE CASCADE` to maintain referential integrity:
- operations.file_id → files.id
- operations.device_id → device.id
- sync_state.file_id → files.id
- sync_state.device_id → device.id
- file_versions.file_id → files.id
- file_versions.device_id → device.id
- conflicts.file_id → files.id
- conflicts.device_id → device.id
- merge_results.conflict_id → conflicts.id
