#include "IPlugin.h"
#include <iostream>

namespace SentinelFS {

    class StoragePlugin : public IPlugin {
    public:
        bool initialize() override {
            std::cout << "StoragePlugin initialized" << std::endl;
            return true;
        }

        void shutdown() override {
            std::cout << "StoragePlugin shutdown" << std::endl;
        }

        std::string getName() const override {
            return "StoragePlugin";
        }

        std::string getVersion() const override {
            return "1.0.0";
        }
    };

    extern "C" {
        IPlugin* create_plugin() {
            return new StoragePlugin();
        }

        void destroy_plugin(IPlugin* plugin) {
            delete plugin;
        }
    }

}