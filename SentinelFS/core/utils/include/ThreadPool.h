#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <cstddef>

namespace SentinelFS {

    /**
     * @brief Fixed-size worker thread pool for CPU-bound tasks.
     *
     * Lightweight utility used via composition (e.g. by DeltaEngine)
     * to parallelize independent jobs without introducing global state.
     * 
     * For shared usage across components, use the global() singleton.
     */
    class ThreadPool {
    public:
        explicit ThreadPool(std::size_t threadCount);
        ~ThreadPool();
        
        /**
         * @brief Get the global shared thread pool instance.
         * 
         * Uses hardware concurrency by default. Thread-safe lazy initialization.
         * Prefer this over creating new ThreadPool instances for short-lived operations.
         */
        static ThreadPool& global() {
            static ThreadPool instance(std::thread::hardware_concurrency());
            return instance;
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        /**
         * @brief Enqueue a task for execution.
         * @return std::future that becomes ready when the task finishes.
         */
        template<typename F>
        std::future<void> enqueue(F&& func) {
            using TaskType = std::packaged_task<void()>;

            auto task = std::make_shared<TaskType>(std::forward<F>(func));
            std::future<void> fut = task->get_future();

            post([task]() {
                (*task)();
            });

            return fut;
        }

        /**
         * @brief Gracefully stop all workers and drain the queue.
         */
        void shutdown();

    private:
        void workerLoop();
        void post(std::function<void()> task);

        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex mutex_;
        std::condition_variable cv_;
        bool stopping_{false};
    };

} // namespace SentinelFS
