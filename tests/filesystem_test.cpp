#include "PluginLoader.h"
#include "IFileAPI.h"
#include "EventBus.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace fs = std::filesystem;

int main() {
    SentinelFS::EventBus eventBus;
    SentinelFS::PluginLoader loader;
    
    std::string pluginPath = "../plugins/filesystem/libfilesystem_plugin.so"; 
    auto plugin = loader.loadPlugin(pluginPath, &eventBus);

    if (!plugin) {
        std::cerr << "Failed to load plugin" << std::endl;
        return 1;
    }

    auto filePlugin = std::dynamic_pointer_cast<SentinelFS::IFileAPI>(plugin);
    if (!filePlugin) {
        std::cerr << "Plugin does not implement IFileAPI" << std::endl;
        return 1;
    }

    // Create a temp directory for testing
    std::string testDir = "/tmp/sentinel_fs_test";
    if (fs::exists(testDir)) {
        fs::remove_all(testDir);
    }
    fs::create_directory(testDir);

    std::cout << "Created test directory: " << testDir << std::endl;

    // Synchronization for test
    std::mutex mtx;
    std::condition_variable cv;
    bool eventReceived = false;
    std::string receivedPath;
    std::string receivedEvent;

    // Subscribe to events
    auto callback = [&](const std::any& data) {
        try {
            std::string path = std::any_cast<std::string>(data);
            std::cout << "Event received for: " << path << std::endl;
            {
                std::lock_guard<std::mutex> lock(mtx);
                receivedPath = path;
                eventReceived = true;
            }
            cv.notify_one();
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Bad any cast: " << e.what() << std::endl;
        }
    };

    eventBus.subscribe("FILE_CREATED", callback);
    eventBus.subscribe("FILE_MODIFIED", callback);

    // Start watching
    filePlugin->watchDirectory(testDir);

    // Give some time for the watcher to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create a file
    std::string testFile = testDir + "/test.txt";
    std::cout << "Creating file: " << testFile << std::endl;
    std::ofstream outfile(testFile);
    outfile << "Hello World";
    outfile.close();

    // Wait for event
    std::unique_lock<std::mutex> lock(mtx);
    if (cv.wait_for(lock, std::chrono::seconds(2), [&]{ return eventReceived; })) {
        std::cout << "Success! Event received." << std::endl;
        if (receivedPath.find("test.txt") != std::string::npos) {
             std::cout << "Path matches." << std::endl;
        } else {
             std::cerr << "Path mismatch: " << receivedPath << std::endl;
             return 1;
        }
    } else {
        std::cerr << "Timeout waiting for event." << std::endl;
        return 1;
    }

    // Cleanup
    plugin->shutdown();
    fs::remove_all(testDir);

    return 0;
}
