/**
 * @file Cache.h
 * @brief LRU Cache implementation for FalconStore
 */

#pragma once

#include "FalconStore.h"
#include <list>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace SentinelFS {
namespace Falcon {

/**
 * @brief Thread-safe LRU Cache with TTL support
 */
class LRUCache : public ICache {
public:
    explicit LRUCache(size_t maxSize = 10000, 
                      size_t maxMemory = 64 * 1024 * 1024,
                      std::chrono::seconds defaultTTL = std::chrono::seconds{300})
        : maxSize_(maxSize)
        , maxMemory_(maxMemory)
        , defaultTTL_(defaultTTL) {}
    
    void put(const std::string& key, const std::string& value, 
             std::chrono::seconds ttl = std::chrono::seconds{0}) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto effectiveTTL = ttl.count() > 0 ? ttl : defaultTTL_;
        auto expiry = std::chrono::steady_clock::now() + effectiveTTL;
        
        // Check if key exists
        auto it = cacheMap_.find(key);
        if (it != cacheMap_.end()) {
            // Update existing entry
            memoryUsed_ -= it->second->value.size();
            cacheList_.erase(it->second);
            cacheMap_.erase(it);
        }
        
        // Check capacity
        while (cacheList_.size() >= maxSize_ || memoryUsed_ + value.size() > maxMemory_) {
            if (cacheList_.empty()) break;
            evictOldest();
        }
        
        // Insert new entry
        cacheList_.push_front({key, value, expiry});
        cacheMap_[key] = cacheList_.begin();
        memoryUsed_ += value.size();
    }
    
    std::optional<std::string> get(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cacheMap_.find(key);
        if (it == cacheMap_.end()) {
            stats_.misses++;
            return std::nullopt;
        }
        
        // Check expiry
        if (std::chrono::steady_clock::now() > it->second->expiry) {
            memoryUsed_ -= it->second->value.size();
            cacheList_.erase(it->second);
            cacheMap_.erase(it);
            stats_.misses++;
            return std::nullopt;
        }
        
        // Move to front (most recently used)
        cacheList_.splice(cacheList_.begin(), cacheList_, it->second);
        stats_.hits++;
        
        return it->second->value;
    }
    
    bool exists(const std::string& key) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cacheMap_.find(key);
        if (it == cacheMap_.end()) return false;
        
        // Check expiry
        return std::chrono::steady_clock::now() <= it->second->expiry;
    }
    
    void invalidate(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cacheMap_.find(key);
        if (it != cacheMap_.end()) {
            memoryUsed_ -= it->second->value.size();
            cacheList_.erase(it->second);
            cacheMap_.erase(it);
        }
    }
    
    void invalidatePrefix(const std::string& prefix) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cacheList_.begin();
        while (it != cacheList_.end()) {
            if (it->key.find(prefix) == 0) {
                memoryUsed_ -= it->value.size();
                cacheMap_.erase(it->key);
                it = cacheList_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        cacheList_.clear();
        cacheMap_.clear();
        memoryUsed_ = 0;
    }
    
    CacheStats getStats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        CacheStats s = stats_;
        s.entries = cacheList_.size();
        s.memoryUsed = memoryUsed_;
        return s;
    }
    
    /**
     * @brief Remove expired entries
     */
    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto it = cacheList_.begin();
        
        while (it != cacheList_.end()) {
            if (now > it->expiry) {
                memoryUsed_ -= it->value.size();
                cacheMap_.erase(it->key);
                it = cacheList_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    struct CacheEntry {
        std::string key;
        std::string value;
        std::chrono::steady_clock::time_point expiry;
    };
    
    size_t maxSize_;
    size_t maxMemory_;
    std::chrono::seconds defaultTTL_;
    
    std::list<CacheEntry> cacheList_;
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> cacheMap_;
    
    mutable std::mutex mutex_;
    size_t memoryUsed_{0};
    CacheStats stats_;
    
    void evictOldest() {
        if (cacheList_.empty()) return;
        
        auto& oldest = cacheList_.back();
        memoryUsed_ -= oldest.value.size();
        cacheMap_.erase(oldest.key);
        cacheList_.pop_back();
    }
};

} // namespace Falcon
} // namespace SentinelFS
