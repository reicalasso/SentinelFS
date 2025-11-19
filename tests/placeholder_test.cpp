#include "PluginLoader.h"
#include "IFileAPI.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>

int main() {
    SentinelFS::PluginLoader loader;
    
    // Path to the filesystem plugin shared library
    // Assuming build directory structure: tests/sentinel_tests is running from build/tests/
    // So plugins are in ../plugins/filesystem/libfilesystem_plugin.so
    std::string pluginPath = "../plugins/filesystem/libfilesystem_plugin.so"; 

    std::cout << "Loading plugin from: " << pluginPath << std::endl;
    auto plugin = loader.loadPlugin(pluginPath);

    if (!plugin) {
        std::cerr << "Failed to load plugin" << std::endl;
        return 1;
    }

    std::cout << "Plugin loaded: " << plugin->getName() << " v" << plugin->getVersion() << std::endl;

    // Cast to IFileAPI
    auto filePlugin = std::dynamic_pointer_cast<SentinelFS::IFileAPI>(plugin);
    if (filePlugin) {
        std::cout << "Plugin implements IFileAPI" << std::endl;
        
        // Test write
        std::string testFile = "test_output.txt";
        std::string content = "Hello SentinelFS!";
        std::vector<uint8_t> data(content.begin(), content.end());
        
        if (filePlugin->writeFile(testFile, data)) {
            std::cout << "Successfully wrote to " << testFile << std::endl;
        } else {
            std::cerr << "Failed to write file" << std::endl;
            return 1;
        }

        // Test read
        auto readData = filePlugin->readFile(testFile);
        std::string readContent(readData.begin(), readData.end());
        std::cout << "Read content: " << readContent << std::endl;

        if (readContent == content) {
            std::cout << "Read/Write test passed!" << std::endl;
        } else {
            std::cerr << "Read content mismatch!" << std::endl;
            return 1;
        }

    } else {
        std::cerr << "Plugin does not implement IFileAPI" << std::endl;
        return 1;
    }

    std::string pluginName = plugin->getName();
    
    // Release shared pointers before unloading the library
    filePlugin.reset();
    plugin.reset();

    loader.unloadPlugin(pluginName);
    std::cout << "Plugin unloaded" << std::endl;

    return 0;
}