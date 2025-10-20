#pragma once

#include <string>
#include <vector>
#include <map>

// Shared data structures used across modules
struct FileInfo {
    std::string path;
    std::string hash;
    std::string lastModified;
    size_t size;
    std::string deviceId;
    int version;                    // Version number for conflict resolution
    std::string conflictStatus;     // "none", "conflicted", "resolved"
    
    FileInfo() : size(0), version(1), conflictStatus("none") {}
    FileInfo(const std::string& p, const std::string& h, size_t s, const std::string& id) 
        : path(p), hash(h), size(s), deviceId(id), version(1), conflictStatus("none") {
        // Set current timestamp
        time_t now = time(0);
        lastModified = std::string(ctime(&now));
        // Remove newline from ctime
        lastModified.pop_back();
    }
};

struct PeerInfo {
    std::string id;
    std::string address;
    int port;
    double latency; // in milliseconds
    bool active;
    std::string lastSeen;
    
    PeerInfo() : port(0), latency(0.0), active(true) {}
    PeerInfo(const std::string& i, const std::string& addr, int p) 
        : id(i), address(addr), port(p), latency(0.0), active(true) {}
};