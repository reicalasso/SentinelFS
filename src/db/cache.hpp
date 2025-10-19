#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <list>
#include <algorithm>  // for std::find
#include <memory>
#include <mutex>
#include "../models.hpp"  // for FileInfo and PeerInfo

template<typename T>
class Cache {
public:
    Cache(size_t maxSize = 1024) : maxSize(maxSize) {}
    
    void put(const std::string& key, const T& value) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cacheMap[key] = value;
        cacheOrder.push_front(key);
        
        if (cacheMap.size() > maxSize) {
            // Remove least recently used item
            std::string lruKey = cacheOrder.back();
            cacheOrder.pop_back();
            cacheMap.erase(lruKey);
        }
    }
    
    bool get(const std::string& key, T& value) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cacheMap.find(key);
        if (it != cacheMap.end()) {
            value = it->second;
            // Move to front (most recently used)
            cacheOrder.erase(std::find(cacheOrder.begin(), cacheOrder.end(), key));
            cacheOrder.push_front(key);
            return true;
        }
        return false;
    }
    
    bool exists(const std::string& key) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        return cacheMap.find(key) != cacheMap.end();
    }
    
    void remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cacheMap.find(key);
        if (it != cacheMap.end()) {
            cacheMap.erase(it);
            cacheOrder.erase(std::find(cacheOrder.begin(), cacheOrder.end(), key));
        }
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cacheMap.clear();
        cacheOrder.clear();
    }
    
private:
    size_t maxSize;
    std::unordered_map<std::string, T> cacheMap;
    std::list<std::string> cacheOrder;  // For LRU
    mutable std::mutex cacheMutex;
};

class DeviceCache {
public:
    DeviceCache(size_t maxSize = 512);
    
    // Peer cache operations
    void cachePeer(const PeerInfo& peer);
    bool getCachedPeer(const std::string& peerId, PeerInfo& peer);
    bool isPeerCached(const std::string& peerId);
    void removeCachedPeer(const std::string& peerId);
    
    // File cache operations
    void cacheFileMetadata(const FileInfo& fileInfo);
    bool getCachedFileMetadata(const std::string& filePath, FileInfo& fileInfo);
    bool isFileCached(const std::string& filePath);
    void removeCachedFileMetadata(const std::string& filePath);
    
    // Clear all caches
    void clearCaches();
    
private:
    Cache<PeerInfo> peerCache;
    Cache<FileInfo> fileCache;
    mutable std::mutex cacheMutex;
};