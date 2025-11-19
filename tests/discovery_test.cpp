#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include "PluginLoader.h"
#include "INetworkAPI.h"
#include "EventBus.h"

using namespace SentinelFS;

int main() {
    std::cout << "Starting Discovery Test..." << std::endl;

    EventBus eventBus;
    PluginLoader loader;

    // Path to the plugin might need adjustment based on build dir
    // Assuming we run from build directory
    std::string pluginPath = "./plugins/network/libnetwork_plugin.so";
    
    auto plugin = loader.loadPlugin(pluginPath, &eventBus);
    if (!plugin) {
        // Try alternative path if running from build root
        pluginPath = "plugins/network/libnetwork_plugin.so";
        plugin = loader.loadPlugin(pluginPath, &eventBus);
    }

    if (!plugin->initialize(&eventBus)) {
        std::cerr << "Failed to initialize plugin" << std::endl;
        return 1;
    }

    auto network = dynamic_cast<INetworkAPI*>(plugin.get());
    if (!network) {
        std::cerr << "Plugin is not a NetworkAPI" << std::endl;
        return 1;
    }

    bool discovered = false;
    eventBus.subscribe("PEER_DISCOVERED", [&](const std::any& data) {
        try {
            std::string msg = std::any_cast<std::string>(data);
            std::cout << "TEST: Received discovery message: " << msg << std::endl;
            discovered = true;
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Bad cast in event handler" << std::endl;
        }
    });

    int port = 9999;
    network->startDiscovery(port);

    std::cout << "Broadcasting presence..." << std::endl;
    // Give the receiver thread a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    network->broadcastPresence(port);

    // Wait for discovery
    for (int i = 0; i < 5; ++i) {
        if (discovered) break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Retrying broadcast..." << std::endl;
        network->broadcastPresence(port); 
    }

    network->shutdown();

    if (discovered) {
        std::cout << "Discovery Test PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "Discovery Test FAILED" << std::endl;
        return 1;
    }
}
