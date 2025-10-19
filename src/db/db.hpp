#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <ctime>
#include <sqlite3.h>
#include "../models.hpp"

class MetadataDB {
public:
    MetadataDB(const std::string& dbPath = "metadata.db");
    ~MetadataDB();

    bool initialize();
    
    // File operations
    bool addFile(const FileInfo& fileInfo);
    bool updateFile(const FileInfo& fileInfo);
    bool deleteFile(const std::string& filePath);
    FileInfo getFile(const std::string& filePath);
    std::vector<FileInfo> getAllFiles();
    
    // Peer operations
    bool addPeer(const PeerInfo& peer);
    bool updatePeer(const PeerInfo& peer);
    bool removePeer(const std::string& peerId);
    PeerInfo getPeer(const std::string& peerId);
    std::vector<PeerInfo> getAllPeers();
    
    // Anomaly logging
    bool logAnomaly(const std::string& filePath, const std::vector<float>& features);
    std::vector<std::map<std::string, std::string>> getAnomalies();
    
private:
    std::string dbPath;
    sqlite3* db;
    mutable std::mutex dbMutex;
    
    bool executeQuery(const std::string& query);
    bool prepareTables();
    
    std::string escapeString(const std::string& str);
};