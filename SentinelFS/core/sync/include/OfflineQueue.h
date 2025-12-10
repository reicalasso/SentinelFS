#pragma once

/**
 * @file OfflineQueue.h
 * @brief Offline operation queue for SentinelFS
 * 
 * Queues file operations when peers are unavailable and
 * automatically syncs when connectivity is restored.
 */

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>

namespace sfs::sync {

/**
 * @brief Types of operations that can be queued
 */
enum class OperationType {
    Create,
    Update,
    Delete,
    Rename
};

/**
 * @brief A queued file operation
 */
struct QueuedOperation {
    OperationType type;
    std::string filePath;
    std::string targetPath;  // For rename operations
    std::chrono::steady_clock::time_point timestamp;
    int retryCount{0};
    
    QueuedOperation(OperationType t, std::string path)
        : type(t), filePath(std::move(path)), 
          timestamp(std::chrono::steady_clock::now()) {}
    
    QueuedOperation(OperationType t, std::string path, std::string target)
        : type(t), filePath(std::move(path)), targetPath(std::move(target)),
          timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Callback for processing queued operations
 * @return true if operation succeeded, false to retry later
 */
using OperationProcessor = std::function<bool(const QueuedOperation&)>;

/**
 * @brief Offline operation queue with automatic retry
 * 
 * Usage:
 * @code
 * OfflineQueue queue;
 * queue.setProcessor([](const QueuedOperation& op) {
 *     return syncEngine.processOperation(op);
 * });
 * queue.start();
 * 
 * // Queue operations when offline
 * queue.enqueue(OperationType::Update, "/path/to/file.txt");
 * 
 * // Operations are automatically processed when processor returns true
 * @endcode
 */
class OfflineQueue {
public:
    OfflineQueue();
    ~OfflineQueue();
    
    // Non-copyable
    OfflineQueue(const OfflineQueue&) = delete;
    OfflineQueue& operator=(const OfflineQueue&) = delete;
    
    /**
     * @brief Set the operation processor callback
     */
    void setProcessor(OperationProcessor processor);
    
    /**
     * @brief Start the background processing thread
     */
    void start();
    
    /**
     * @brief Stop the background processing thread
     */
    void stop();
    
    /**
     * @brief Enqueue a file operation
     */
    void enqueue(OperationType type, const std::string& filePath);
    
    /**
     * @brief Enqueue a rename operation
     */
    void enqueueRename(const std::string& oldPath, const std::string& newPath);
    
    /**
     * @brief Get the number of pending operations
     */
    std::size_t pendingCount() const;
    
    /**
     * @brief Check if queue is empty
     */
    bool empty() const;
    
    /**
     * @brief Clear all pending operations
     */
    void clear();
    
    /**
     * @brief Set online/offline status
     */
    void setOnline(bool online);
    
    /**
     * @brief Check if currently online
     */
    bool isOnline() const;
    
    /**
     * @brief Get all pending operations (for persistence)
     */
    std::vector<QueuedOperation> getPendingOperations() const;
    
    /**
     * @brief Load operations (from persistence)
     */
    void loadOperations(const std::vector<QueuedOperation>& ops);

private:
    void processLoop();
    bool tryProcess(QueuedOperation& op);
    
    mutable std::mutex mutex_;
    std::queue<QueuedOperation> queue_;
    OperationProcessor processor_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> online_{true};
    std::thread processorThread_;
    
    static constexpr int MAX_RETRIES = 5;
    static constexpr int RETRY_DELAY_MS = 5000;
    static constexpr int PROCESS_INTERVAL_MS = 1000;
};

} // namespace sfs::sync
