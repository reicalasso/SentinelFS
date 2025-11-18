#ifndef SFS_FILE_API_IMPL_H
#define SFS_FILE_API_IMPL_H

#include "file_api.h"

namespace sfs {
namespace core {

/**
 * @brief FileAPI - Concrete implementation of IFileAPI
 * 
 * Uses std::filesystem for file operations and OpenSSL for hashing.
 */
class FileAPI : public IFileAPI {
public:
    FileAPI();
    ~FileAPI() override;
    
    bool exists(const std::string& path) const override;
    bool is_directory(const std::string& path) const override;
    bool remove(const std::string& path) override;
    std::vector<uint8_t> read_all(const std::string& path) const override;
    bool write_all(const std::string& path, const std::vector<uint8_t>& data) override;
    uint64_t file_size(const std::string& path) const override;
    uint64_t file_modified_time(const std::string& path) const override;
    std::string hash(const std::string& path) const override;
    std::vector<FileChunk> split_into_chunks(const std::string& path, size_t chunk_size) const override;
    FileInfo get_file_info(const std::string& path) const override;
    std::vector<std::string> list_directory(const std::string& path, bool recursive) const override;
    bool create_directory(const std::string& path) override;

private:
    /**
     * @brief Calculate SHA-256 hash of data
     * 
     * @param data Input data
     * @return Hex-encoded hash
     */
    std::string calculate_sha256(const std::vector<uint8_t>& data) const;
    
    /**
     * @brief Calculate SHA-256 hash of file stream
     * 
     * More memory-efficient for large files.
     */
    std::string calculate_file_hash(const std::string& path) const;
};

} // namespace core
} // namespace sfs

#endif // SFS_FILE_API_IMPL_H
