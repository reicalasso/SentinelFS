#include "cache.hpp"

DeviceCache::DeviceCache(size_t maxSize) : peerCache(maxSize/2), fileCache(maxSize/2) {}

void DeviceCache::cachePeer(const PeerInfo& peer) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    peerCache.put(peer.id, peer);
}

bool DeviceCache::getCachedPeer(const std::string& peerId, PeerInfo& peer) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    return peerCache.get(peerId, peer);
}

bool DeviceCache::isPeerCached(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    return peerCache.exists(peerId);
}

void DeviceCache::removeCachedPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    peerCache.remove(peerId);
}

void DeviceCache::cacheFileMetadata(const FileInfo& fileInfo) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    fileCache.put(fileInfo.path, fileInfo);
}

bool DeviceCache::getCachedFileMetadata(const std::string& filePath, FileInfo& fileInfo) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    return fileCache.get(filePath, fileInfo);
}

bool DeviceCache::isFileCached(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    return fileCache.exists(filePath);
}

void DeviceCache::removeCachedFileMetadata(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    fileCache.remove(filePath);
}

void DeviceCache::clearCaches() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    peerCache.clear();
    fileCache.clear();
}