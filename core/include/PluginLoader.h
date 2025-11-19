#pragma once

#include "IPlugin.h"
#include <string>
#include <memory>
#include <unordered_map>

namespace SentinelFS {

    class PluginLoader {
    public:
        /**
         * @brief Load a plugin from a shared library.
         * @param path The path to the shared library (.so file).
         * @return A shared pointer to the loaded plugin instance, or nullptr if failed.
         */
        std::shared_ptr<IPlugin> loadPlugin(const std::string& path);

        /**
         * @brief Unload a plugin by name.
         * @param name The name of the plugin to unload.
         */
        void unloadPlugin(const std::string& name);

    private:
        std::unordered_map<std::string, void*> handles_;
        std::unordered_map<std::string, std::shared_ptr<IPlugin>> plugins_;
    };

}
