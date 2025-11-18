#include "core/plugin_loader/plugin_loader.h"
#include "core/logger/logger.h"
#include <iostream>

/**
 * @brief Simple Sprint 3 Test
 * Tests basic watcher plugin loading without complex EventBus interaction
 */

using namespace sfs::core;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Sprint 3 - Simple Watcher Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    Logger::instance().set_level(LogLevel::INFO);
    
    SFS_LOG_INFO("Test", "Attempting to load watcher.linux plugin");
    
    PluginLoader loader;
    
    if (loader.load_plugin("lib/watcher_linux.so")) {
        SFS_LOG_INFO("Test", "✓ Plugin loaded successfully!");
        
        const SFS_PluginInfo* info = loader.get_plugin_info("watcher.linux");
        if (info) {
            std::cout << "\nPlugin Info:" << std::endl;
            std::cout << "  Name: " << info->name << std::endl;
            std::cout << "  Version: " << info->version << std::endl;
            std::cout << "  Type: FILESYSTEM" << std::endl;
        }
        
        loader.unload_all();
        SFS_LOG_INFO("Test", "✓ Plugin unloaded");
        
        std::cout << "\n✅ Sprint 3 Core Functionality Works!" << std::endl;
        return 0;
    } else {
        SFS_LOG_ERROR("Test", "Failed to load plugin");
        std::cerr << "\nPlugin file should be at: lib/watcher_linux.so" << std::endl;
        return 1;
    }
}
