#include "core/plugin_loader/plugin_loader.h"
#include "core/event_bus/event_bus.h"
#include "core/logger/logger.h"
#include "core/config/config.h"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * @brief SentinelFS-Neo Test Application
 * 
 * Demonstrates Core functionality:
 * - Plugin loading
 * - EventBus communication
 * - Logger usage
 * - Config management
 */

int main(int argc, char* argv[]) {
    using namespace sfs::core;
    
    std::cout << "==================================" << std::endl;
    std::cout << "SentinelFS-Neo v0.1.0 - Core Test" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << std::endl;
    
    // Initialize logger
    Logger::instance().set_level(LogLevel::DEBUG);
    Logger::instance().set_console_output(true);
    
    SFS_LOG_INFO("Main", "Starting SentinelFS-Neo Core test");
    
    // Initialize config
    Config config;
    config.set_string("core.name", "SentinelFS-Neo");
    config.set_string("core.version", "0.1.0");
    config.set_bool("core.test_mode", true);
    
    SFS_LOG_INFO("Main", "Configuration loaded");
    
    // Initialize EventBus
    EventBus event_bus;
    
    // Subscribe to test events
    auto sub_id = event_bus.subscribe("test.event", [](const Event& evt) {
        SFS_LOG_INFO("EventBus", "Received test event from: " + evt.source);
    });
    
    SFS_LOG_INFO("Main", "EventBus initialized");
    
    // Publish a test event
    Event test_event("test.event", std::string("Hello EventBus!"), "Main");
    event_bus.publish(test_event);
    
    // Initialize PluginLoader
    PluginLoader loader;
    
    SFS_LOG_INFO("Main", "PluginLoader initialized");
    
    // Try to load hello_plugin if it exists
    std::string plugin_path;
    
    #ifdef _WIN32
        plugin_path = "plugins/hello_plugin.dll";
    #elif __APPLE__
        plugin_path = "plugins/hello_plugin.dylib";
    #else
        plugin_path = "plugins/hello_plugin.so";
    #endif
    
    SFS_LOG_INFO("Main", "Attempting to load plugin: " + plugin_path);
    
    if (loader.load_plugin(plugin_path)) {
        SFS_LOG_INFO("Main", "Plugin loaded successfully!");
        
        // Get plugin info
        const SFS_PluginInfo* info = loader.get_plugin_info("hello_plugin");
        if (info) {
            std::cout << std::endl;
            std::cout << "Plugin Information:" << std::endl;
            std::cout << "  Name: " << info->name << std::endl;
            std::cout << "  Version: " << info->version << std::endl;
            std::cout << "  Author: " << info->author << std::endl;
            std::cout << "  Description: " << info->description << std::endl;
            std::cout << "  API Version: " << info->api_version << std::endl;
            std::cout << std::endl;
        }
        
        // Keep running for a moment
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Unload plugin
        loader.unload_plugin("hello_plugin");
        SFS_LOG_INFO("Main", "Plugin unloaded");
    } else {
        SFS_LOG_WARN("Main", "Could not load plugin (this is OK if not built yet)");
    }
    
    // Cleanup
    event_bus.unsubscribe(sub_id);
    
    std::cout << std::endl;
    SFS_LOG_INFO("Main", "Core test completed successfully");
    std::cout << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "Sprint 1 - Core Infrastructure ✓" << std::endl;
    std::cout << "==================================" << std::endl;
    
    return 0;
}
