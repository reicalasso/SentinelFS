#include "file_queue.hpp"

FileQueue::FileQueue() = default;

FileQueue::~FileQueue() = default;

void FileQueue::enqueue(const SyncItem& item) {
    std::lock_guard<std::mutex> lock(queueMutex);
    queue.push(item);
    condition.notify_one();
}

bool FileQueue::dequeue(SyncItem& item) {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (queue.empty()) {
        return false;
    }
    item = queue.front();
    queue.pop();
    return true;
}

bool FileQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return queue.empty();
}

size_t FileQueue::size() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return queue.size();
}

void FileQueue::waitForItem() {
    std::unique_lock<std::mutex> lock(queueMutex);
    condition.wait(lock, [this] { return !queue.empty(); });
}