#pragma once

#include "IStorageAPI.h"
#include "SQLiteHandler.h"
#include <optional>

namespace SentinelFS {

/**
 * @brief Manages file metadata CRUD operations
 */
class FileMetadataManager {
public:
    explicit FileMetadataManager(SQLiteHandler* handler) : handler_(handler) {}

    /**
     * @brief Add or update file metadata
     */
    bool addFile(const std::string& path, const std::string& hash, 
                 long long timestamp, long long size);

    /**
     * @brief Retrieve file metadata by path
     */
    std::optional<FileMetadata> getFile(const std::string& path);

    /**
     * @brief Remove file from database
     */
    bool removeFile(const std::string& path);

private:
    SQLiteHandler* handler_;
};

} // namespace SentinelFS
