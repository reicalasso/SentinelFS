#include "ThreadPool.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <atomic>
#include <chrono>

using namespace SentinelFS;

void test_basic_execution() {
    std::cout << "Running test_basic_execution..." << std::endl;
    ThreadPool pool(2);
    
    std::atomic<bool> executed{false};
    auto future = pool.enqueue([&executed]() {
        executed = true;
    });
    
    future.wait();
    assert(executed);
    std::cout << "test_basic_execution passed." << std::endl;
}

void test_multiple_tasks() {
    std::cout << "Running test_multiple_tasks..." << std::endl;
    ThreadPool pool(4);
    
    const int taskCount = 100;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < taskCount; ++i) {
        futures.push_back(pool.enqueue([&counter]() {
            counter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }));
    }
    
    for (auto& f : futures) {
        f.wait();
    }
    
    assert(counter == taskCount);
    std::cout << "test_multiple_tasks passed." << std::endl;
}

int main() {
    try {
        test_basic_execution();
        test_multiple_tasks();
        std::cout << "All ThreadPool tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
