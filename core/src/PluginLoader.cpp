#include "PluginLoader.h"
#include <dlfcn.h>
#include <iostream>

namespace SentinelFS {

    using CreatePluginFunc = IPlugin* (*)();
    using DestroyPluginFunc = void (*)(IPlugin*);

    std::shared_ptr<IPlugin> PluginLoader::loadPlugin(const std::string& path, EventBus* eventBus) {
        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "Cannot open library: " << dlerror() << '\n';
            return nullptr;
        }

        // Reset errors
        dlerror();
        
        // Load create function
        CreatePluginFunc create = (CreatePluginFunc) dlsym(handle, "create_plugin");
        const char* dlsym_error = dlerror();
        if (dlsym_error) {
            std::cerr << "Cannot load symbol 'create_plugin': " << dlsym_error << '\n';
            dlclose(handle);
            return nullptr;
        }

        // Load destroy function
        DestroyPluginFunc destroy = (DestroyPluginFunc) dlsym(handle, "destroy_plugin");
        dlsym_error = dlerror();
        if (dlsym_error) {
            std::cerr << "Cannot load symbol 'destroy_plugin': " << dlsym_error << '\n';
            dlclose(handle);
            return nullptr;
        }

        IPlugin* raw_plugin = create();
        if (!raw_plugin) {
            std::cerr << "Failed to create plugin instance\n";
            dlclose(handle);
            return nullptr;
        }

        // Create shared_ptr with custom deleter that calls destroy_plugin
        std::shared_ptr<IPlugin> plugin(raw_plugin, [destroy](IPlugin* p) {
            destroy(p);
        });

        if (!plugin->initialize(eventBus)) {
             std::cerr << "Failed to initialize plugin\n";
             // plugin destructor will be called here, which calls destroy
             dlclose(handle);
             return nullptr;
        }

        std::string name = plugin->getName();
        handles_[name] = handle;
        plugins_[name] = plugin;

        return plugin;
    }

    void PluginLoader::unloadPlugin(const std::string& name) {
        // First remove the plugin, which triggers the deleter (destroy_plugin)
        if (plugins_.find(name) != plugins_.end()) {
            plugins_[name]->shutdown();
            plugins_.erase(name);
        }

        // Then close the library handle
        if (handles_.find(name) != handles_.end()) {
            dlclose(handles_[name]);
            handles_.erase(name);
        }
    }

}
