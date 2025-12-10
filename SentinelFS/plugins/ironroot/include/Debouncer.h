#pragma once

/**
 * @file Debouncer.h
 * @brief Event debouncing for filesystem events
 * 
 * Handles:
 * - Rapid successive modifications (editor saves)
 * - Atomic write detection (temp file → rename)
 * - Batch operations (copy/move many files)
 */

#include "IWatcher.h"
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <filesystem>

namespace SentinelFS {
namespace IronRoot {

/**
 * @brief Debounce configuration
 */
struct DebounceConfig {
    std::chrono::milliseconds window{100};
    std::chrono::milliseconds maxDelay{500};
    bool coalesceModifies{true};
    bool detectAtomicWrites{true};
};

/**
 * @brief Pending event for debouncing
 */
struct PendingEvent {
    WatchEvent event;
    std::chrono::steady_clock::time_point firstSeen;
    std::chrono::steady_clock::time_point lastSeen;
    int count{1};
    bool isAtomicWrite{false};
    std::string tempPath;  // For atomic writes
};

/**
 * @brief Event debouncer
 */
class Debouncer {
public:
    using OutputCallback = std::function<void(const WatchEvent&, bool isAtomicWrite)>;
    using BatchCallback = std::function<void(const std::vector<WatchEvent>&)>;
    
    Debouncer();
    ~Debouncer();
    
    /**
     * @brief Start the debouncer
     */
    void start(const DebounceConfig& config, OutputCallback callback);
    
    /**
     * @brief Stop the debouncer
     */
    void stop();
    
    /**
     * @brief Add an event to debounce
     */
    void addEvent(const WatchEvent& event);
    
    /**
     * @brief Flush all pending events immediately
     */
    void flush();
    
    /**
     * @brief Set batch callback
     */
    void setBatchCallback(BatchCallback callback);
    
    /**
     * @brief Get statistics
     */
    struct Stats {
        uint64_t eventsReceived{0};
        uint64_t eventsEmitted{0};
        uint64_t eventsCoalesced{0};
        uint64_t atomicWritesDetected{0};
        uint64_t batchesDetected{0};
    };
    Stats getStats() const;

private:
    void processLoop();
    void emitEvent(const PendingEvent& pending);
    bool detectAtomicWrite(const WatchEvent& event);
    bool isTempFile(const std::string& path) const;
    
    DebounceConfig config_;
    OutputCallback outputCallback_;
    BatchCallback batchCallback_;
    
    std::atomic<bool> running_{false};
    std::thread processThread_;
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::map<std::string, PendingEvent> pendingEvents_;
    
    // Atomic write tracking
    std::map<std::string, std::string> tempToTarget_;  // temp path -> expected target
    
    // Statistics
    mutable std::mutex statsMutex_;
    Stats stats_;
};

} // namespace IronRoot
} // namespace SentinelFS
