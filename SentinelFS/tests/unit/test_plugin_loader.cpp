#include "PluginLoader.h"
#include "EventBus.h"
#include <iostream>
#include <cassert>

using namespace SentinelFS;

void test_plugin_loader() {
    std::cout << "Running test_plugin_loader..." << std::endl;
    
    PluginLoader loader;
    EventBus eventBus;
    
    // Try to load a non-existent plugin
    auto plugin = loader.loadPlugin("non_existent_plugin.so", &eventBus);
    assert(plugin == nullptr);
    
    // Try to unload a non-existent plugin (should not crash)
    loader.unloadPlugin("non_existent_plugin");
    
    std::cout << "test_plugin_loader passed." << std::endl;
}

int main() {
    try {
        test_plugin_loader();
        std::cout << "All PluginLoader tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
