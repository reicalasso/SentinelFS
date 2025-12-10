#pragma once

#include "IPlugin.h"
#include <vector>
#include <string>
#include <cstdint>

namespace SentinelFS {

    class IFileAPI : public IPlugin {
    public:
        virtual ~IFileAPI() = default;

        /**
         * @brief Read the contents of a file.
         * @param path The path to the file.
         * @return A vector containing the file data.
         */
        virtual std::vector<uint8_t> readFile(const std::string& path) = 0;

        /**
         * @brief Start watching a directory for changes.
         * @param path The path to the directory.
         */
        virtual void startWatching(const std::string& path) = 0;

        /**
         * @brief Stop watching a directory.
         * @param path The path to the directory.
         */
        virtual void stopWatching(const std::string& path) = 0;

        /**
         * @brief Write data to a file.
         * @param path The path to the file.
         * @param data The data to write.
         * @return true if the write was successful, false otherwise.
         */
        virtual bool writeFile(const std::string& path, const std::vector<uint8_t>& data) = 0;
    };

}
