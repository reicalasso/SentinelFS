#ifndef SFS_PLUGIN_API_H
#define SFS_PLUGIN_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Plugin API Version
 * Used for ABI compatibility checking
 */
#define SFS_PLUGIN_API_VERSION 1

/**
 * @brief Plugin Types
 */
typedef enum {
    SFS_PLUGIN_TYPE_UNKNOWN = 0,
    SFS_PLUGIN_TYPE_FILESYSTEM = 1,
    SFS_PLUGIN_TYPE_NETWORK = 2,
    SFS_PLUGIN_TYPE_STORAGE = 3,
    SFS_PLUGIN_TYPE_ML = 4
} SFS_PluginType;

/**
 * @brief Plugin Information Structure
 * 
 * This structure is returned by plugin_info() and contains
 * metadata about the plugin.
 */
typedef struct {
    const char* name;           // Plugin name (e.g., "watcher.linux")
    const char* version;        // Plugin version (e.g., "1.0.0")
    const char* author;         // Plugin author
    const char* description;    // Brief description
    SFS_PluginType type;        // Plugin type
    uint32_t api_version;       // API version this plugin was built against
} SFS_PluginInfo;

/**
 * @brief Plugin Interface - Get Plugin Information
 * 
 * Every plugin MUST implement this function.
 * Returns static metadata about the plugin.
 * 
 * @return Plugin information structure
 */
typedef SFS_PluginInfo (*SFS_PluginInfoFunc)(void);

/**
 * @brief Plugin Interface - Create Plugin Instance
 * 
 * Every plugin MUST implement this function.
 * Creates and returns an opaque pointer to the plugin instance.
 * The returned pointer should point to a C++ object implementing
 * the appropriate interface (IWatcher, IDiscovery, etc.)
 * 
 * @return Opaque pointer to plugin instance
 */
typedef void* (*SFS_PluginCreateFunc)(void);

/**
 * @brief Plugin Interface - Destroy Plugin Instance
 * 
 * Every plugin MUST implement this function.
 * Destroys the plugin instance created by plugin_create().
 * 
 * @param instance Opaque pointer returned by plugin_create()
 */
typedef void (*SFS_PluginDestroyFunc)(void* instance);

/**
 * @brief Standard plugin entry points
 * 
 * All plugins must export these three functions:
 * 
 * extern "C" SFS_PluginInfo plugin_info();
 * extern "C" void* plugin_create();
 * extern "C" void plugin_destroy(void* instance);
 */

#ifdef __cplusplus
}
#endif

#endif // SFS_PLUGIN_API_H
