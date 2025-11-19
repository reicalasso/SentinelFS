#pragma once

#include "IPlugin.h"
#include <string>
#include <optional>

namespace SentinelFS {

    struct FileMetadata {
        std::string path;
        std::string hash;
        long long timestamp;
        long long size;
    };

    class IStorageAPI : public IPlugin {
    public:
        virtual ~IStorageAPI() = default;

        /**
         * @brief Add or update file metadata in the storage.
         */
        virtual bool addFile(const std::string& path, const std::string& hash, long long timestamp, long long size) = 0;

        /**
         * @brief Retrieve file metadata by path.
         */
        virtual std::optional<FileMetadata> getFile(const std::string& path) = 0;

        /**
         * @brief Remove file metadata by path.
         */
        virtual bool removeFile(const std::string& path) = 0;
    };

}
