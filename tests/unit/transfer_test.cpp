#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include "PluginLoader.h"
#include "INetworkAPI.h"
#include "EventBus.h"

using namespace SentinelFS;

int main() {
    std::cout << "Starting Transfer Test..." << std::endl;

    EventBus eventBus;
    PluginLoader loader;

    std::string pluginPath = "plugins/network/libnetwork_plugin.so";
    auto plugin = loader.loadPlugin(pluginPath, &eventBus);
    
    if (!plugin) {
        // Try alternative path
        pluginPath = "../plugins/network/libnetwork_plugin.so";
        plugin = loader.loadPlugin(pluginPath, &eventBus);
    }

    if (!plugin) {
        std::cerr << "Failed to load network plugin" << std::endl;
        return 1;
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

    bool dataReceived = false;
    std::string receivedMsg;
    std::string connectedPeerId;
    bool peerConnected = false;

    eventBus.subscribe("PEER_CONNECTED", [&](const std::any& data) {
        try {
            connectedPeerId = std::any_cast<std::string>(data);
            std::cout << "TEST: Peer connected: " << connectedPeerId << std::endl;
            peerConnected = true;
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Bad cast in PEER_CONNECTED handler" << std::endl;
        }
    });

    eventBus.subscribe("DATA_RECEIVED", [&](const std::any& data) {
        try {
            receivedMsg = std::any_cast<std::string>(data);
            std::cout << "TEST: Received data: " << receivedMsg << std::endl;
            dataReceived = true;
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Bad cast in event handler" << std::endl;
        }
    });

    int port = 8080;
    network->startListening(port);

    // Give the listener a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string target = "127.0.0.1";
    if (!network->connectToPeer(target, port)) {
        std::cerr << "Failed to connect to peer" << std::endl;
        return 1;
    }

    // Wait for handshake to complete (PEER_CONNECTED event)
    for (int i = 0; i < 5; ++i) {
        if (peerConnected) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!peerConnected) {
        std::cerr << "Handshake failed or timed out" << std::endl;
        return 1;
    }

    std::string msg = "Hello SentinelFS!";
    std::vector<uint8_t> data(msg.begin(), msg.end());
    
    // Use the ID obtained from handshake
    if (!network->sendData(connectedPeerId, data)) {
        std::cerr << "Failed to send data" << std::endl;
        return 1;
    }

    // Wait for data
    for (int i = 0; i < 5; ++i) {
        if (dataReceived) break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    network->shutdown();

    if (dataReceived && receivedMsg == msg) {
        std::cout << "Transfer Test PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "Transfer Test FAILED" << std::endl;
        return 1;
    }
}
