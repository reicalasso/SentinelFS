#include "core/plugin_api.h"
#include <iostream>
#include <cstring>

/**
 * @brief HelloPlugin - Minimal example plugin
 * 
 * Demonstrates the plugin architecture:
 * - Implements required C API (plugin_info, plugin_create, plugin_destroy)
 * - Shows how to create a plugin instance
 * - Provides a simple interface for testing
 */

class HelloPlugin {
public:
    HelloPlugin() {
        std::cout << "HelloPlugin: Instance created" << std::endl;
    }
    
    ~HelloPlugin() {
        std::cout << "HelloPlugin: Instance destroyed" << std::endl;
    }
    
    void say_hello() {
        std::cout << "HelloPlugin: Hello from plugin!" << std::endl;
    }
    
    std::string get_message() const {
        return "This is a test plugin for SentinelFS-Neo";
    }
};

// ============================================================================
// PLUGIN C API IMPLEMENTATION (REQUIRED BY ALL PLUGINS)
// ============================================================================

extern "C" {

/**
 * @brief Return plugin metadata
 * 
 * This function MUST be implemented by all plugins.
 */
SFS_PluginInfo plugin_info() {
    SFS_PluginInfo info;
    info.name = "hello_plugin";
    info.version = "1.0.0";
    info.author = "SentinelFS Team";
    info.description = "Example plugin demonstrating plugin architecture";
    info.type = SFS_PLUGIN_TYPE_UNKNOWN;  // This is just a test plugin
    info.api_version = SFS_PLUGIN_API_VERSION;
    
    return info;
}

/**
 * @brief Create plugin instance
 * 
 * This function MUST be implemented by all plugins.
 * Returns an opaque pointer to the plugin instance.
 */
void* plugin_create() {
    return new HelloPlugin();
}

/**
 * @brief Destroy plugin instance
 * 
 * This function MUST be implemented by all plugins.
 * Destroys the instance created by plugin_create().
 */
void plugin_destroy(void* instance) {
    delete static_cast<HelloPlugin*>(instance);
}

} // extern "C"
