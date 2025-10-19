#include "db.hpp"
#include <iostream>

MetadataDB::MetadataDB(const std::string& dbPath) : dbPath(dbPath), db(nullptr) {}

MetadataDB::~MetadataDB() {
    if (db) {
        sqlite3_close(db);
    }
}

bool MetadataDB::initialize() {
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    return prepareTables();
}

bool MetadataDB::prepareTables() {
    const char* createFilesTable = R"(
        CREATE TABLE IF NOT EXISTS files (
            path TEXT PRIMARY KEY,
            hash TEXT NOT NULL,
            last_modified TEXT NOT NULL,
            size INTEGER NOT NULL,
            device_id TEXT NOT NULL
        );
    )";
    
    const char* createPeersTable = R"(
        CREATE TABLE IF NOT EXISTS peers (
            id TEXT PRIMARY KEY,
            address TEXT NOT NULL,
            port INTEGER NOT NULL,
            latency REAL,
            active INTEGER NOT NULL,
            last_seen TEXT NOT NULL
        );
    )";
    
    const char* createAnomaliesTable = R"(
        CREATE TABLE IF NOT EXISTS anomalies (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            features TEXT
        );
    )";
    
    char* errMsg = 0;
    
    if (sqlite3_exec(db, createFilesTable, 0, 0, &errMsg) != SQLITE_OK) {
        std::cerr << "Error creating files table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    if (sqlite3_exec(db, createPeersTable, 0, 0, &errMsg) != SQLITE_OK) {
        std::cerr << "Error creating peers table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    if (sqlite3_exec(db, createAnomaliesTable, 0, 0, &errMsg) != SQLITE_OK) {
        std::cerr << "Error creating anomalies table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool MetadataDB::addFile(const FileInfo& fileInfo) {
    const char* sql = R"(
        INSERT OR REPLACE INTO files (path, hash, last_modified, size, device_id)
        VALUES (?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, fileInfo.path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, fileInfo.hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, fileInfo.lastModified.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, fileInfo.size);
    sqlite3_bind_text(stmt, 5, fileInfo.deviceId.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool MetadataDB::updateFile(const FileInfo& fileInfo) {
    return addFile(fileInfo);  // Using same logic as add for now
}

bool MetadataDB::deleteFile(const std::string& filePath) {
    const char* sql = "DELETE FROM files WHERE path = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

FileInfo MetadataDB::getFile(const std::string& filePath) {
    const char* sql = "SELECT path, hash, last_modified, size, device_id FROM files WHERE path = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return FileInfo();
    }
    
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
    
    FileInfo fileInfo;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        fileInfo.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        fileInfo.hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        fileInfo.lastModified = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        fileInfo.size = sqlite3_column_int64(stmt, 3);
        fileInfo.deviceId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    }
    
    sqlite3_finalize(stmt);
    return fileInfo;
}

std::vector<FileInfo> MetadataDB::getAllFiles() {
    const char* sql = "SELECT path, hash, last_modified, size, device_id FROM files";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    std::vector<FileInfo> files;
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            FileInfo fileInfo;
            fileInfo.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            fileInfo.hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            fileInfo.lastModified = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            fileInfo.size = sqlite3_column_int64(stmt, 3);
            fileInfo.deviceId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            files.push_back(fileInfo);
        }
    }
    
    sqlite3_finalize(stmt);
    return files;
}

bool MetadataDB::addPeer(const PeerInfo& peer) {
    const char* sql = R"(
        INSERT OR REPLACE INTO peers (id, address, port, latency, active, last_seen)
        VALUES (?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, peer.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, peer.address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, peer.port);
    sqlite3_bind_double(stmt, 4, peer.latency);
    sqlite3_bind_int(stmt, 5, peer.active ? 1 : 0);
    
    // Set current timestamp
    time_t now = time(0);
    std::string timestamp = std::string(ctime(&now));
    timestamp.pop_back(); // Remove newline
    
    sqlite3_bind_text(stmt, 6, timestamp.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool MetadataDB::updatePeer(const PeerInfo& peer) {
    return addPeer(peer);  // Using same logic as add for now
}

bool MetadataDB::removePeer(const std::string& peerId) {
    const char* sql = "DELETE FROM peers WHERE id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, peerId.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

PeerInfo MetadataDB::getPeer(const std::string& peerId) {
    const char* sql = "SELECT id, address, port, latency, active, last_seen FROM peers WHERE id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return PeerInfo();
    }
    
    sqlite3_bind_text(stmt, 1, peerId.c_str(), -1, SQLITE_STATIC);
    
    PeerInfo peerInfo;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        peerInfo.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        peerInfo.address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        peerInfo.port = sqlite3_column_int(stmt, 2);
        peerInfo.latency = sqlite3_column_double(stmt, 3);
        peerInfo.active = sqlite3_column_int(stmt, 4) != 0;
        peerInfo.lastSeen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    }
    
    sqlite3_finalize(stmt);
    return peerInfo;
}

std::vector<PeerInfo> MetadataDB::getAllPeers() {
    const char* sql = "SELECT id, address, port, latency, active, last_seen FROM peers";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    std::vector<PeerInfo> peers;
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            PeerInfo peerInfo;
            peerInfo.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            peerInfo.address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            peerInfo.port = sqlite3_column_int(stmt, 2);
            peerInfo.latency = sqlite3_column_double(stmt, 3);
            peerInfo.active = sqlite3_column_int(stmt, 4) != 0;
            peerInfo.lastSeen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            peers.push_back(peerInfo);
        }
    }
    
    sqlite3_finalize(stmt);
    return peers;
}

bool MetadataDB::logAnomaly(const std::string& filePath, const std::vector<float>& features) {
    // Convert features vector to string for storage
    std::string featuresStr = "[";
    for (size_t i = 0; i < features.size(); ++i) {
        if (i > 0) featuresStr += ",";
        featuresStr += std::to_string(features[i]);
    }
    featuresStr += "]";
    
    const char* sql = R"(
        INSERT INTO anomalies (file_path, timestamp, features)
        VALUES (?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) {
        return false;
    }
    
    // Set current timestamp
    time_t now = time(0);
    std::string timestamp = std::string(ctime(&now));
    timestamp.pop_back(); // Remove newline
    
    sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, timestamp.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, featuresStr.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

std::vector<std::map<std::string, std::string>> MetadataDB::getAnomalies() {
    const char* sql = "SELECT id, file_path, timestamp, features FROM anomalies ORDER BY timestamp DESC";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    std::vector<std::map<std::string, std::string>> anomalies;
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> anomaly;
            anomaly["id"] = std::to_string(sqlite3_column_int64(stmt, 0));
            anomaly["file_path"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            anomaly["timestamp"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            anomaly["features"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            anomalies.push_back(anomaly);
        }
    }
    
    sqlite3_finalize(stmt);
    return anomalies;
}

bool MetadataDB::executeQuery(const std::string& query) {
    char* errMsg = 0;
    int rc = sqlite3_exec(db, query.c_str(), 0, 0, &errMsg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

std::string MetadataDB::escapeString(const std::string& str) {
    // Simple escaping - in a real application, use parameterized queries instead
    std::string escaped = str;
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.insert(pos, "'");
        pos += 2;
    }
    return escaped;
}