#ifndef SFS_FILE_API_H
#define SFS_FILE_API_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace sfs {
namespace core {

/**
 * @brief File chunk metadata
 * 
 * Represents a chunk of a file with its offset, size, and hash.
 */
struct FileChunk {
    uint64_t offset;            // Byte offset in file
    uint64_t size;              // Chunk size in bytes
    std::string hash;           // SHA-256 hash of chunk data
    std::vector<uint8_t> data;  // Chunk data (optional, for transfer)
    
    FileChunk() : offset(0), size(0) {}
    
    FileChunk(uint64_t off, uint64_t sz, const std::string& h = "")
        : offset(off), size(sz), hash(h) {}
};

/**
 * @brief File metadata
 */
struct FileInfo {
    std::string path;           // Absolute or relative path
    uint64_t size;              // File size in bytes
    uint64_t modified_time;     // Last modification time (Unix timestamp)
    std::string hash;           // Full file hash (SHA-256)
    bool is_directory;          // True if directory
    
    FileInfo() : size(0), modified_time(0), is_directory(false) {}
};

/**
 * @brief IFileAPI - File system abstraction interface
 * 
 * Provides platform-independent file operations:
 * - Reading/writing files
 * - Hashing (SHA-256)
 * - Chunking for delta sync
 * - File metadata queries
 * 
 * This is the ONLY way Core and plugins should interact with files.
 */
class IFileAPI {
public:
    virtual ~IFileAPI() = default;
    
    /**
     * @brief Check if file or directory exists
     * 
     * @param path File path
     * @return true if exists
     */
    virtual bool exists(const std::string& path) const = 0;
    
    /**
     * @brief Check if path is a directory
     */
    virtual bool is_directory(const std::string& path) const = 0;
    
    /**
     * @brief Remove file or directory
     * 
     * @param path Path to remove
     * @return true if removed successfully
     */
    virtual bool remove(const std::string& path) = 0;
    
    /**
     * @brief Read entire file into memory
     * 
     * @param path File path
     * @return File contents or empty vector on error
     */
    virtual std::vector<uint8_t> read_all(const std::string& path) const = 0;
    
    /**
     * @brief Write data to file
     * 
     * Creates parent directories if needed.
     * 
     * @param path File path
     * @param data Data to write
     * @return true if written successfully
     */
    virtual bool write_all(const std::string& path, const std::vector<uint8_t>& data) = 0;
    
    /**
     * @brief Get file size
     * 
     * @param path File path
     * @return File size in bytes, or 0 on error
     */
    virtual uint64_t file_size(const std::string& path) const = 0;
    
    /**
     * @brief Get file modification time
     * 
     * @param path File path
     * @return Unix timestamp (seconds since epoch)
     */
    virtual uint64_t file_modified_time(const std::string& path) const = 0;
    
    /**
     * @brief Calculate SHA-256 hash of file
     * 
     * @param path File path
     * @return Hex-encoded SHA-256 hash (64 characters)
     */
    virtual std::string hash(const std::string& path) const = 0;
    
    /**
     * @brief Split file into chunks
     * 
     * Divides file into fixed-size chunks (except last chunk).
     * Each chunk includes hash for integrity verification.
     * 
     * @param path File path
     * @param chunk_size Chunk size in bytes (default: 4096)
     * @return Vector of chunks with metadata
     */
    virtual std::vector<FileChunk> split_into_chunks(
        const std::string& path,
        size_t chunk_size = 4096
    ) const = 0;
    
    /**
     * @brief Get file metadata
     * 
     * @param path File path
     * @return FileInfo structure
     */
    virtual FileInfo get_file_info(const std::string& path) const = 0;
    
    /**
     * @brief List directory contents
     * 
     * @param path Directory path
     * @param recursive If true, recursively list subdirectories
     * @return Vector of file paths
     */
    virtual std::vector<std::string> list_directory(
        const std::string& path,
        bool recursive = false
    ) const = 0;
    
    /**
     * @brief Create directory
     * 
     * Creates parent directories as needed.
     * 
     * @param path Directory path
     * @return true if created successfully
     */
    virtual bool create_directory(const std::string& path) = 0;
};

} // namespace core
} // namespace sfs

#endif // SFS_FILE_API_H
