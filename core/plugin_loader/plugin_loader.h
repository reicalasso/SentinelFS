#ifndef SFS_PLUGIN_LOADER_H
#define SFS_PLUGIN_LOADER_H

#include "../plugin_api.h"
#include <string>
#include <memory>
#include <map>
#include <vector>

namespace sfs {
namespace core {

/**
 * @brief Loaded plugin handle
 * 
 * Represents a loaded plugin with its metadata and instance.
 */
struct PluginHandle {
    std::string path;                   // Full path to plugin shared library
    void* library_handle;               // OS-specific library handle
    SFS_PluginInfo info;               // Plugin metadata
    void* instance;                     // Plugin instance (opaque pointer)
    
    SFS_PluginInfoFunc info_func;
    SFS_PluginCreateFunc create_func;
    SFS_PluginDestroyFunc destroy_func;
    
    PluginHandle()
        : library_handle(nullptr)
        , instance(nullptr)
        , info_func(nullptr)
        , create_func(nullptr)
        , destroy_func(nullptr) {}
};

/**
 * @brief PluginLoader - Dynamic plugin loading and management
 * 
 * Loads plugins from shared libraries (.so/.dylib/.dll),
 * validates their ABI, and manages their lifecycle.
 * 
 * Core does NOT know about plugin implementation details.
 * It only interacts through the C API.
 */
class PluginLoader {
public:
    PluginLoader();
    ~PluginLoader();
    
    // Prevent copying
    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;
    
    /**
     * @brief Load a plugin from path
     * 
     * @param path Path to plugin shared library
     * @return true if loaded successfully
     */
    bool load_plugin(const std::string& path);
    
    /**
     * @brief Unload a plugin by name
     * 
     * @param plugin_name Plugin name from plugin_info()
     * @return true if unloaded successfully
     */
    bool unload_plugin(const std::string& plugin_name);
    
    /**
     * @brief Unload all plugins
     */
    void unload_all();
    
    /**
     * @brief Get plugin instance by name
     * 
     * Returns the opaque instance pointer. Caller must cast it
     * to the appropriate interface type.
     * 
     * @param plugin_name Plugin name
     * @return Plugin instance pointer or nullptr if not found
     */
    void* get_plugin_instance(const std::string& plugin_name) const;
    
    /**
     * @brief Get plugin info by name
     * 
     * @param plugin_name Plugin name
     * @return Plugin info or nullptr if not found
     */
    const SFS_PluginInfo* get_plugin_info(const std::string& plugin_name) const;
    
    /**
     * @brief Get all loaded plugin names
     * 
     * @return Vector of plugin names
     */
    std::vector<std::string> get_loaded_plugins() const;
    
    /**
     * @brief Check if plugin is loaded
     * 
     * @param plugin_name Plugin name
     * @return true if plugin is loaded
     */
    bool is_loaded(const std::string& plugin_name) const;

private:
    std::map<std::string, std::unique_ptr<PluginHandle>> plugins_;
    
    /**
     * @brief Load dynamic library (OS-specific)
     */
    void* load_library(const std::string& path);
    
    /**
     * @brief Unload dynamic library (OS-specific)
     */
    void unload_library(void* handle);
    
    /**
     * @brief Get function pointer from library (OS-specific)
     */
    void* get_symbol(void* handle, const char* symbol_name);
    
    /**
     * @brief Get last error message (OS-specific)
     */
    std::string get_error() const;
};

} // namespace core
} // namespace sfs

#endif // SFS_PLUGIN_LOADER_H
