#include "PluginManager.h"
#include <iostream>
#include <cassert>
#include <string>

using namespace SentinelFS;

void test_plugin_registration() {
    std::cout << "Running test_plugin_registration..." << std::endl;
    
    PluginManager pm;
    PluginManager::Descriptor desc;
    desc.path = "libtest.so";
    desc.minVersion = "1.0.0";
    
    pm.registerPlugin("test_plugin", desc);
    
    assert(pm.isRegistered("test_plugin"));
    assert(!pm.isRegistered("unknown_plugin"));
    
    assert(pm.getPluginStatus("test_plugin") == PluginManager::PluginStatus::NOT_LOADED);
    
    std::cout << "test_plugin_registration passed." << std::endl;
}

int main() {
    try {
        test_plugin_registration();
        std::cout << "All PluginManager tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
