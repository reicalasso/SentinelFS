#include "PluginLoader.h"
#include "EventBus.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

int main() {
    std::cout << "Starting SentinelFS Daemon..." << std::endl;

    SentinelFS::EventBus eventBus;
    SentinelFS::PluginLoader loader;
    std::vector<std::shared_ptr<SentinelFS::IPlugin>> plugins;

    // Load plugins
    // Assuming running from build/app/daemon/
    // Plugins are in ../../plugins/
    std::vector<std::string> pluginPaths = {
        "../../plugins/filesystem/libfilesystem_plugin.so",
        "../../plugins/network/libnetwork_plugin.so",
        "../../plugins/storage/libstorage_plugin.so",
        "../../plugins/ml/libml_plugin.so"
    };

    for (const auto& path : pluginPaths) {
        auto plugin = loader.loadPlugin(path, &eventBus);
        if (plugin) {
            std::cout << "Loaded plugin: " << plugin->getName() << std::endl;
            plugins.push_back(plugin);
        } else {
            std::cerr << "Failed to load plugin: " << path << std::endl;
        }
    }

    std::cout << "SentinelFS Daemon running. Press Ctrl+C to exit." << std::endl;

    // Main loop
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup (unreachable in this simple loop, but good practice)
    plugins.clear();
    return 0;
}