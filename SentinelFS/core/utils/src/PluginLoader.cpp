#include "PluginLoader.h"
#include "Logger.h"
#include <dlfcn.h>
#include <iostream>

namespace SentinelFS {

    using CreatePluginFunc = IPlugin* (*)();
    using DestroyPluginFunc = void (*)(IPlugin*);

    std::shared_ptr<IPlugin> PluginLoader::loadPlugin(const std::string& path, EventBus* eventBus) {
        void* handle = nullptr;
        CreatePluginFunc create = nullptr;
        DestroyPluginFunc destroy = nullptr;
        IPlugin* raw_plugin = nullptr;
        
        try {
            // Open library
            handle = dlopen(path.c_str(), RTLD_LAZY);
            if (!handle) {
                const char* error = dlerror();
                std::string errorMsg = "Cannot open library: " + std::string(error ? error : "unknown error");
                Logger::instance().error(errorMsg, "PluginLoader");
                return nullptr;
            }

            // Reset errors
            dlerror();
            
            // Load create function
            create = (CreatePluginFunc) dlsym(handle, "create_plugin");
            const char* dlsym_error = dlerror();
            if (dlsym_error) {
                std::string errorMsg = "Cannot load symbol 'create_plugin': " + std::string(dlsym_error);
                Logger::instance().error(errorMsg, "PluginLoader");
                dlclose(handle);
                return nullptr;
            }

            // Load destroy function
            destroy = (DestroyPluginFunc) dlsym(handle, "destroy_plugin");
            dlsym_error = dlerror();
            if (dlsym_error) {
                std::string errorMsg = "Cannot load symbol 'destroy_plugin': " + std::string(dlsym_error);
                Logger::instance().error(errorMsg, "PluginLoader");
                dlclose(handle);
                return nullptr;
            }

            // Create plugin instance
            raw_plugin = create();
            if (!raw_plugin) {
                std::string errorMsg = "Failed to create plugin instance";
                Logger::instance().error(errorMsg, "PluginLoader");
                if (destroy) {
                    destroy(raw_plugin);
                }
                dlclose(handle);
                return nullptr;
            }

            // Create shared_ptr with custom deleter that calls destroy_plugin
            // Note: Handle is NOT closed in deleter - it's managed separately
            std::shared_ptr<IPlugin> plugin(raw_plugin, [destroy](IPlugin* p) {
                if (p && destroy) {
                    try {
                        destroy(p);
                    } catch (...) {
                        // Swallow exceptions in destructor to prevent std::terminate
                        Logger::instance().error("Exception in plugin destroy function", "PluginLoader");
                    }
                }
            });

            // Initialize plugin
            if (!plugin->initialize(eventBus)) {
                std::string errorMsg = "Failed to initialize plugin";
                Logger::instance().error(errorMsg, "PluginLoader");
                // plugin destructor will be called here, which calls destroy
                // But we still need to close the handle
                dlclose(handle);
                return nullptr;
            }

            // Store plugin and handle only after successful initialization
            std::string name = plugin->getName();
            handles_[name] = handle;
            plugins_[name] = plugin;

            return plugin;
        } catch (const std::exception& e) {
            std::string errorMsg = "Exception loading plugin: " + std::string(e.what());
            Logger::instance().error(errorMsg, "PluginLoader");
            
            // Cleanup on exception
            if (raw_plugin && destroy) {
                try {
                    destroy(raw_plugin);
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
            }
            if (handle) {
                dlclose(handle);
            }
            return nullptr;
        } catch (...) {
            std::string errorMsg = "Unknown exception loading plugin";
            Logger::instance().error(errorMsg, "PluginLoader");
            
            // Cleanup on exception
            if (raw_plugin && destroy) {
                try {
                    destroy(raw_plugin);
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
            }
            if (handle) {
                dlclose(handle);
            }
            return nullptr;
        }
    }

    void PluginLoader::unloadPlugin(const std::string& name) {
        try {
            // First shutdown the plugin
            auto pluginIt = plugins_.find(name);
            if (pluginIt != plugins_.end()) {
                try {
                    pluginIt->second->shutdown();
                } catch (const std::exception& e) {
                    std::string errorMsg = "Exception shutting down plugin " + name + ": " + std::string(e.what());
                    Logger::instance().error(errorMsg, "PluginLoader");
                } catch (...) {
                    std::string errorMsg = "Unknown exception shutting down plugin " + name;
                    Logger::instance().error(errorMsg, "PluginLoader");
                }
                
                // Remove plugin (triggers deleter which calls destroy_plugin)
                plugins_.erase(pluginIt);
            }

            // Then close the library handle
            auto handleIt = handles_.find(name);
            if (handleIt != handles_.end()) {
                dlclose(handleIt->second);
                handles_.erase(handleIt);
            }
        } catch (const std::exception& e) {
            std::string errorMsg = "Exception unloading plugin " + name + ": " + std::string(e.what());
            Logger::instance().error(errorMsg, "PluginLoader");
        } catch (...) {
            std::string errorMsg = "Unknown exception unloading plugin " + name;
            Logger::instance().error(errorMsg, "PluginLoader");
        }
    }

}
