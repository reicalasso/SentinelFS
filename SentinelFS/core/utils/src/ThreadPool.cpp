#include "ThreadPool.h"

#include <algorithm>

namespace SentinelFS {

    ThreadPool::ThreadPool(std::size_t threadCount) {
        if (threadCount == 0) {
            auto hw = std::thread::hardware_concurrency();
            threadCount = hw == 0 ? 1 : hw;
        }

        workers_.reserve(threadCount);
        for (std::size_t i = 0; i < threadCount; ++i) {
            workers_.emplace_back(&ThreadPool::workerLoop, this);
        }
    }

    ThreadPool::~ThreadPool() {
        shutdown();
    }

    void ThreadPool::shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
        }

        cv_.notify_all();

        for (auto& t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }

        workers_.clear();
    }

    void ThreadPool::post(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                return;
            }
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    void ThreadPool::workerLoop() {
        for (;;) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() {
                    return stopping_ || !tasks_.empty();
                });

                if (stopping_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            if (task) {
                task();
            }
        }
    }

} // namespace SentinelFS
