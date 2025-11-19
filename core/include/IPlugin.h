#pragma once

#include <string>

namespace SentinelFS {

    class EventBus; // Forward declaration

    class IPlugin {
    public:
        virtual ~IPlugin() = default;

        /**
         * @brief Initialize the plugin.
         * @param eventBus Pointer to the central EventBus.
         * @return true if initialization was successful, false otherwise.
         */
        virtual bool initialize(EventBus* eventBus) = 0;

        /**
         * @brief Shutdown the plugin and release resources.
         */
        virtual void shutdown() = 0;

        /**
         * @brief Get the name of the plugin.
         * @return The name of the plugin.
         */
        virtual std::string getName() const = 0;

        /**
         * @brief Get the version of the plugin.
         * @return The version string.
         */
        virtual std::string getVersion() const = 0;
    };

}
