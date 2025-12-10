#include "OfflineQueue.h"
#include "Logger.h"

namespace sfs::sync {

OfflineQueue::OfflineQueue() = default;

OfflineQueue::~OfflineQueue() {
    stop();
}

void OfflineQueue::setProcessor(OperationProcessor processor) {
    std::lock_guard<std::mutex> lock(mutex_);
    processor_ = std::move(processor);
}

void OfflineQueue::start() {
    if (running_) return;
    
    running_ = true;
    processorThread_ = std::thread(&OfflineQueue::processLoop, this);
    
    SentinelFS::Logger::instance().info("Offline queue started", "OfflineQueue");
}

void OfflineQueue::stop() {
    running_ = false;
    if (processorThread_.joinable()) {
        processorThread_.join();
    }
    
    SentinelFS::Logger::instance().info("Offline queue stopped", "OfflineQueue");
}

void OfflineQueue::enqueue(OperationType type, const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.emplace(type, filePath);
    
    SentinelFS::Logger::instance().debug(
        "Queued operation: " + filePath, "OfflineQueue");
}

void OfflineQueue::enqueueRename(const std::string& oldPath, const std::string& newPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.emplace(OperationType::Rename, oldPath, newPath);
    
    SentinelFS::Logger::instance().debug(
        "Queued rename: " + oldPath + " -> " + newPath, "OfflineQueue");
}

std::size_t OfflineQueue::pendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool OfflineQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

void OfflineQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<QueuedOperation> empty;
    std::swap(queue_, empty);
    
    SentinelFS::Logger::instance().info("Offline queue cleared", "OfflineQueue");
}

void OfflineQueue::setOnline(bool online) {
    bool wasOffline = !online_.exchange(online);
    
    if (wasOffline && online) {
        SentinelFS::Logger::instance().info(
            "Back online, processing queued operations", "OfflineQueue");
    } else if (!online) {
        SentinelFS::Logger::instance().info(
            "Going offline, operations will be queued", "OfflineQueue");
    }
}

bool OfflineQueue::isOnline() const {
    return online_;
}

std::vector<QueuedOperation> OfflineQueue::getPendingOperations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<QueuedOperation> ops;
    std::queue<QueuedOperation> temp = queue_;
    
    while (!temp.empty()) {
        ops.push_back(temp.front());
        temp.pop();
    }
    
    return ops;
}

void OfflineQueue::loadOperations(const std::vector<QueuedOperation>& ops) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& op : ops) {
        queue_.push(op);
    }
    
    SentinelFS::Logger::instance().info(
        "Loaded " + std::to_string(ops.size()) + " queued operations", "OfflineQueue");
}

void OfflineQueue::processLoop() {
    auto& logger = SentinelFS::Logger::instance();
    
    while (running_) {
        // Sleep in small intervals for responsive shutdown
        for (int i = 0; i < PROCESS_INTERVAL_MS / 100 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!running_ || !online_) continue;
        
        QueuedOperation op(OperationType::Update, "");
        bool hasOp = false;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!queue_.empty()) {
                op = queue_.front();
                queue_.pop();
                hasOp = true;
            }
        }
        
        if (hasOp) {
            if (tryProcess(op)) {
                logger.debug("Processed queued operation: " + op.filePath, "OfflineQueue");
            } else {
                // Re-queue if not max retries
                op.retryCount++;
                if (op.retryCount < MAX_RETRIES) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    queue_.push(op);
                    logger.warn("Retry " + std::to_string(op.retryCount) + 
                               " for: " + op.filePath, "OfflineQueue");
                } else {
                    logger.error("Max retries exceeded for: " + op.filePath, "OfflineQueue");
                }
                
                // Delay before next attempt
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            }
        }
    }
}

bool OfflineQueue::tryProcess(QueuedOperation& op) {
    OperationProcessor proc;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        proc = processor_;
    }
    
    if (!proc) {
        return false;
    }
    
    try {
        return proc(op);
    } catch (const std::exception& e) {
        SentinelFS::Logger::instance().error(
            "Exception processing operation: " + std::string(e.what()), "OfflineQueue");
        return false;
    }
}

} // namespace sfs::sync
