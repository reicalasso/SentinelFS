#include "IPlugin.h"
#include <iostream>

namespace SentinelFS {

    class MLPlugin : public IPlugin {
    public:
        bool initialize() override {
            std::cout << "MLPlugin initialized" << std::endl;
            return true;
        }

        void shutdown() override {
            std::cout << "MLPlugin shutdown" << std::endl;
        }

        std::string getName() const override {
            return "MLPlugin";
        }

        std::string getVersion() const override {
            return "1.0.0";
        }
    };

    extern "C" {
        IPlugin* create_plugin() {
            return new MLPlugin();
        }

        void destroy_plugin(IPlugin* plugin) {
            delete plugin;
        }
    }

}