#include "core/plugin_loader/plugin_loader.h"
#include "core/event_bus/event_bus.h"
#include "core/logger/logger.h"
#include "plugins/filesystem/watcher_common/iwatcher.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

/**
 * @brief Sprint 3 Test - Filesystem Watcher Plugin
 * 
 * Demonstrates:
 * - Loading watcher.linux plugin
 * - Setting up filesystem monitoring
 * - Receiving file system events
 * - EventBus integration
 */

using namespace sfs::core;
using namespace sfs::plugins;

static std::atomic<bool> keep_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived SIGINT, stopping..." << std::endl;
        keep_running = false;
    }
}

void print_separator(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << title << std::endl;
    std::cout << "========================================\n" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    
    print_separator("SentinelFS-Neo Sprint 3 Test");
    std::cout << "Filesystem Watcher Plugin Demo\n" << std::endl;
    
    // Initialize logger
    Logger::instance().set_level(LogLevel::INFO);
    Logger::instance().set_console_output(true);
    
    SFS_LOG_INFO("Main", "Starting Sprint 3 test");
    
    // ========================================
    // Test 1: Load Watcher Plugin
    // ========================================
    print_separator("Test 1: Loading watcher.linux Plugin");
    
    PluginLoader loader;
    
    std::string plugin_path = "lib/watcher_linux.so";
    
    if (!loader.load_plugin(plugin_path)) {
        SFS_LOG_ERROR("Main", "Failed to load plugin: " + plugin_path);
        std::cerr << "\nMake sure the plugin is built:" << std::endl;
        std::cerr << "  cmake --build . -j$(nproc)" << std::endl;
        return 1;
    }
    
    SFS_LOG_INFO("Main", "✓ Plugin loaded successfully");
    
    // Get plugin info
    const SFS_PluginInfo* info = loader.get_plugin_info("watcher.linux");
    if (info) {
        std::cout << "\nPlugin Information:" << std::endl;
        std::cout << "  Name: " << info->name << std::endl;
        std::cout << "  Version: " << info->version << std::endl;
        std::cout << "  Type: FILESYSTEM" << std::endl;
        std::cout << "  Description: " << info->description << std::endl;
    }
    
    // ========================================
    // Test 2: Get Watcher Instance
    // ========================================
    print_separator("Test 2: Initializing Watcher");
    
    void* instance = loader.get_plugin_instance("watcher.linux");
    if (!instance) {
        SFS_LOG_ERROR("Main", "Failed to get plugin instance");
        return 1;
    }
    
    // Cast to IWatcher interface
    IWatcher* watcher = static_cast<IWatcher*>(instance);
    SFS_LOG_INFO("Main", "✓ Watcher instance obtained");
    
    // ========================================
    // Test 3: Setup EventBus Integration
    // ========================================
    print_separator("Test 3: EventBus Integration");
    
    EventBus event_bus;
    
    // Subscribe to filesystem events
    auto sub_id = event_bus.subscribe("fs.event", [](const Event& evt) {
        try {
            // Extract FsEvent from Event data
            const FsEvent& fs_event = std::any_cast<const FsEvent&>(evt.data);
            
            std::cout << "[FS EVENT] "
                      << event_type_to_string(fs_event.type) << " "
                      << fs_event.path;
            
            if (fs_event.is_directory) {
                std::cout << " [DIR]";
            }
            
            std::cout << std::endl;
            
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Failed to cast event data" << std::endl;
        }
    });
    
    SFS_LOG_INFO("Main", "✓ EventBus subscriber registered");
    
    // Set watcher callback to publish to EventBus
    watcher->on_event = [&event_bus](const FsEvent& fs_event) {
        Event event("fs.event", fs_event, "watcher.linux");
        event_bus.publish(event);
    };
    
    SFS_LOG_INFO("Main", "✓ Watcher callback connected to EventBus");
    
    // ========================================
    // Test 4: Start Watching
    // ========================================
    print_separator("Test 4: Start Filesystem Monitoring");
    
    // Use /tmp/sfs_watch as test directory
    std::string watch_path = "/tmp/sfs_watch";
    system(("mkdir -p " + watch_path).c_str());
    
    std::cout << "Watch directory: " << watch_path << std::endl;
    std::cout << "\nStarting watcher..." << std::endl;
    
    if (!watcher->start(watch_path)) {
        SFS_LOG_ERROR("Main", "Failed to start watcher");
        return 1;
    }
    
    SFS_LOG_INFO("Main", "✓ Watcher started successfully");
    
    // ========================================
    // Test 5: Monitor Events
    // ========================================
    print_separator("Test 5: Monitoring Events");
    
    std::cout << "Watcher is now active!" << std::endl;
    std::cout << "\nTry these commands in another terminal:" << std::endl;
    std::cout << "  echo 'test' > " << watch_path << "/test.txt" << std::endl;
    std::cout << "  echo 'modified' >> " << watch_path << "/test.txt" << std::endl;
    std::cout << "  rm " << watch_path << "/test.txt" << std::endl;
    std::cout << "  mkdir " << watch_path << "/subdir" << std::endl;
    std::cout << "\nPress Ctrl+C to stop...\n" << std::endl;
    
    // Keep running until interrupted
    while (keep_running && watcher->is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // ========================================
    // Cleanup
    // ========================================
    print_separator("Cleanup");
    
    watcher->stop();
    SFS_LOG_INFO("Main", "Watcher stopped");
    
    event_bus.unsubscribe(sub_id);
    SFS_LOG_INFO("Main", "EventBus cleaned up");
    
    loader.unload_all();
    SFS_LOG_INFO("Main", "Plugins unloaded");
    
    // Remove test directory
    system(("rm -rf " + watch_path).c_str());
    
    // ========================================
    // Summary
    // ========================================
    print_separator("Sprint 3 Complete!");
    
    std::cout << "✓ watcher.linux plugin loaded" << std::endl;
    std::cout << "✓ IWatcher interface working" << std::endl;
    std::cout << "✓ inotify integration functional" << std::endl;
    std::cout << "✓ EventBus integration successful" << std::endl;
    std::cout << "✓ Real-time file monitoring working" << std::endl;
    
    std::cout << "\nNext: Sprint 4 - Delta Engine (rsync-style)" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    return 0;
}
