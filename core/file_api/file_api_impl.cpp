#include "file_api_impl.h"
#include "../logger/logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

// OpenSSL for SHA-256
#include <openssl/sha.h>

namespace fs = std::filesystem;

namespace sfs {
namespace core {

FileAPI::FileAPI() {
}

FileAPI::~FileAPI() {
}

bool FileAPI::exists(const std::string& path) const {
    try {
        return fs::exists(path);
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "exists() failed: " + std::string(e.what()));
        return false;
    }
}

bool FileAPI::is_directory(const std::string& path) const {
    try {
        return fs::is_directory(path);
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "is_directory() failed: " + std::string(e.what()));
        return false;
    }
}

bool FileAPI::remove(const std::string& path) {
    try {
        return fs::remove_all(path) > 0;
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "remove() failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<uint8_t> FileAPI::read_all(const std::string& path) const {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            SFS_LOG_ERROR("FileAPI", "Failed to open file: " + path);
            return {};
        }
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        
        return buffer;
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "read_all() failed: " + std::string(e.what()));
        return {};
    }
}

bool FileAPI::write_all(const std::string& path, const std::vector<uint8_t>& data) {
    try {
        // Create parent directories
        fs::path file_path(path);
        if (file_path.has_parent_path()) {
            fs::create_directories(file_path.parent_path());
        }
        
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            SFS_LOG_ERROR("FileAPI", "Failed to create file: " + path);
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        return file.good();
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "write_all() failed: " + std::string(e.what()));
        return false;
    }
}

uint64_t FileAPI::file_size(const std::string& path) const {
    try {
        return fs::file_size(path);
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "file_size() failed: " + std::string(e.what()));
        return 0;
    }
}

uint64_t FileAPI::file_modified_time(const std::string& path) const {
    try {
        auto ftime = fs::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        return std::chrono::duration_cast<std::chrono::seconds>(
            sctp.time_since_epoch()
        ).count();
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "file_modified_time() failed: " + std::string(e.what()));
        return 0;
    }
}

std::string FileAPI::hash(const std::string& path) const {
    return calculate_file_hash(path);
}

std::vector<FileChunk> FileAPI::split_into_chunks(const std::string& path, size_t chunk_size) const {
    std::vector<FileChunk> chunks;
    
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            SFS_LOG_ERROR("FileAPI", "Failed to open file for chunking: " + path);
            return chunks;
        }
        
        uint64_t offset = 0;
        std::vector<uint8_t> buffer(chunk_size);
        
        while (file.good()) {
            file.read(reinterpret_cast<char*>(buffer.data()), chunk_size);
            std::streamsize bytes_read = file.gcount();
            
            if (bytes_read > 0) {
                FileChunk chunk;
                chunk.offset = offset;
                chunk.size = bytes_read;
                
                // Calculate hash for this chunk
                std::vector<uint8_t> chunk_data(buffer.begin(), buffer.begin() + bytes_read);
                chunk.hash = calculate_sha256(chunk_data);
                
                chunks.push_back(std::move(chunk));
                offset += bytes_read;
            }
        }
        
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "split_into_chunks() failed: " + std::string(e.what()));
    }
    
    return chunks;
}

FileInfo FileAPI::get_file_info(const std::string& path) const {
    FileInfo info;
    
    try {
        if (!fs::exists(path)) {
            return info;
        }
        
        info.path = path;
        info.is_directory = fs::is_directory(path);
        
        if (!info.is_directory) {
            info.size = fs::file_size(path);
            info.modified_time = file_modified_time(path);
            info.hash = calculate_file_hash(path);
        } else {
            info.size = 0;
            info.modified_time = 0;
        }
        
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "get_file_info() failed: " + std::string(e.what()));
    }
    
    return info;
}

std::vector<std::string> FileAPI::list_directory(const std::string& path, bool recursive) const {
    std::vector<std::string> files;
    
    try {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            return files;
        }
        
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                files.push_back(entry.path().string());
            }
        } else {
            for (const auto& entry : fs::directory_iterator(path)) {
                files.push_back(entry.path().string());
            }
        }
        
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "list_directory() failed: " + std::string(e.what()));
    }
    
    return files;
}

bool FileAPI::create_directory(const std::string& path) {
    try {
        return fs::create_directories(path);
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "create_directory() failed: " + std::string(e.what()));
        return false;
    }
}

std::string FileAPI::calculate_sha256(const std::vector<uint8_t>& data) const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
    
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return oss.str();
}

std::string FileAPI::calculate_file_hash(const std::string& path) const {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }
        
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        
        constexpr size_t buffer_size = 8192;
        std::vector<uint8_t> buffer(buffer_size);
        
        while (file.good()) {
            file.read(reinterpret_cast<char*>(buffer.data()), buffer_size);
            std::streamsize bytes_read = file.gcount();
            if (bytes_read > 0) {
                SHA256_Update(&sha256, buffer.data(), bytes_read);
            }
        }
        
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_Final(hash, &sha256);
        
        std::ostringstream oss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        
        return oss.str();
        
    } catch (const std::exception& e) {
        SFS_LOG_ERROR("FileAPI", "calculate_file_hash() failed: " + std::string(e.what()));
        return "";
    }
}

} // namespace core
} // namespace sfs
