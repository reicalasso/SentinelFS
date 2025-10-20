#pragma once

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

struct SyncItem {
    std::string filePath;
    std::string operation; // "add", "update", "delete"
    std::string hash;
    
    SyncItem(const std::string& path, const std::string& op, const std::string& h = "")
        : filePath(path), operation(op), hash(h) {}
};

class FileQueue {
public:
    FileQueue();
    ~FileQueue();

    void enqueue(const SyncItem& item);
    bool dequeue(SyncItem& item);
    bool isEmpty() const;
    size_t size() const;
    
    void waitForItem();
    
    // Batch operations - dequeue multiple items at once for efficient processing
    std::vector<SyncItem> dequeueBatch(size_t maxItems = 100);
    
private:
    std::queue<SyncItem> queue;
    mutable std::mutex queueMutex;
    std::condition_variable condition;
};