#include "PluginLoader.h"
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
                std::cerr << "Cannot open library: " << (error ? error : "unknown error") << '\n';
                return nullptr;
            }

            // Reset errors
            dlerror();
            
            // Load create function
            create = (CreatePluginFunc) dlsym(handle, "create_plugin");
            const char* dlsym_error = dlerror();
            if (dlsym_error) {
                std::cerr << "Cannot load symbol 'create_plugin': " << dlsym_error << '\n';
                dlclose(handle);
                return nullptr;
            }

            // Load destroy function
            destroy = (DestroyPluginFunc) dlsym(handle, "destroy_plugin");
            dlsym_error = dlerror();
            if (dlsym_error) {
                std::cerr << "Cannot load symbol 'destroy_plugin': " << dlsym_error << '\n';
                dlclose(handle);
                return nullptr;
            }

            // Create plugin instance
            raw_plugin = create();
            if (!raw_plugin) {
                std::cerr << "Failed to create plugin instance\n";
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
                        std::cerr << "Exception in plugin destroy function\n";
                    }
                }
            });

            // Initialize plugin
            if (!plugin->initialize(eventBus)) {
                std::cerr << "Failed to initialize plugin\n";
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
            std::cerr << "Exception loading plugin: " << e.what() << '\n';
            
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
            std::cerr << "Unknown exception loading plugin\n";
            
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
                    std::cerr << "Exception shutting down plugin " << name << ": " << e.what() << '\n';
                } catch (...) {
                    std::cerr << "Unknown exception shutting down plugin " << name << '\n';
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
            std::cerr << "Exception unloading plugin " << name << ": " << e.what() << '\n';
        } catch (...) {
            std::cerr << "Unknown exception unloading plugin " << name << '\n';
        }
    }

}
