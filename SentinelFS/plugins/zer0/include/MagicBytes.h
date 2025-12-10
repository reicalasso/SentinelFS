#pragma once

/**
 * @file MagicBytes.h
 * @brief Magic byte signatures for file type detection
 */

#include "Zer0Plugin.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace SentinelFS {
namespace Zer0 {

/**
 * @brief Magic byte database
 */
class MagicBytes {
public:
    static MagicBytes& instance() {
        static MagicBytes instance;
        return instance;
    }
    
    /**
     * @brief Detect file category from magic bytes
     */
    FileCategory detectCategory(const std::vector<uint8_t>& header) const;
    
    /**
     * @brief Get expected category for extension
     */
    FileCategory getCategoryForExtension(const std::string& extension) const;
    
    /**
     * @brief Check if header matches expected category
     */
    bool validateHeader(const std::vector<uint8_t>& header, FileCategory expected) const;
    
    /**
     * @brief Get description for detected type
     */
    std::string getDescription(const std::vector<uint8_t>& header) const;
    
    /**
     * @brief Check if file is executable
     */
    bool isExecutable(const std::vector<uint8_t>& header) const;
    
    /**
     * @brief Check if file contains embedded script
     */
    bool hasEmbeddedScript(const std::vector<uint8_t>& content) const;

private:
    MagicBytes();
    
    struct Signature {
        std::vector<uint8_t> magic;
        size_t offset{0};
        FileCategory category;
        std::string description;
        bool isExecutable{false};
    };
    
    std::vector<Signature> signatures_;
    std::unordered_map<std::string, FileCategory> extensionMap_;
    
    void initSignatures();
    void initExtensionMap();
};

// Common magic bytes constants
namespace Magic {
    // Images
    constexpr uint8_t PNG[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    constexpr uint8_t JPEG[] = {0xFF, 0xD8, 0xFF};
    constexpr uint8_t GIF87[] = {0x47, 0x49, 0x46, 0x38, 0x37, 0x61};
    constexpr uint8_t GIF89[] = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61};
    constexpr uint8_t BMP[] = {0x42, 0x4D};
    constexpr uint8_t WEBP[] = {0x52, 0x49, 0x46, 0x46};  // + WEBP at offset 8
    constexpr uint8_t ICO[] = {0x00, 0x00, 0x01, 0x00};
    constexpr uint8_t TIFF_LE[] = {0x49, 0x49, 0x2A, 0x00};
    constexpr uint8_t TIFF_BE[] = {0x4D, 0x4D, 0x00, 0x2A};
    
    // Documents
    constexpr uint8_t PDF[] = {0x25, 0x50, 0x44, 0x46};  // %PDF
    constexpr uint8_t ZIP[] = {0x50, 0x4B, 0x03, 0x04};  // Also DOCX, XLSX, etc.
    constexpr uint8_t DOCX[] = {0x50, 0x4B, 0x03, 0x04}; // Same as ZIP
    constexpr uint8_t RTF[] = {0x7B, 0x5C, 0x72, 0x74, 0x66};  // {\rtf
    
    // Archives
    constexpr uint8_t GZIP[] = {0x1F, 0x8B};
    constexpr uint8_t BZIP2[] = {0x42, 0x5A, 0x68};  // BZh
    constexpr uint8_t XZ[] = {0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00};
    constexpr uint8_t ZSTD[] = {0x28, 0xB5, 0x2F, 0xFD};
    constexpr uint8_t RAR[] = {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07};
    constexpr uint8_t SEVENZ[] = {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C};
    constexpr uint8_t TAR[] = {0x75, 0x73, 0x74, 0x61, 0x72};  // at offset 257
    
    // Executables
    constexpr uint8_t ELF[] = {0x7F, 0x45, 0x4C, 0x46};  // \x7fELF
    constexpr uint8_t PE[] = {0x4D, 0x5A};  // MZ (Windows EXE/DLL)
    constexpr uint8_t MACHO32[] = {0xFE, 0xED, 0xFA, 0xCE};
    constexpr uint8_t MACHO64[] = {0xFE, 0xED, 0xFA, 0xCF};
    constexpr uint8_t SHEBANG[] = {0x23, 0x21};  // #!
    
    // Audio
    constexpr uint8_t MP3_ID3[] = {0x49, 0x44, 0x33};  // ID3
    constexpr uint8_t MP3_SYNC[] = {0xFF, 0xFB};
    constexpr uint8_t WAV[] = {0x52, 0x49, 0x46, 0x46};  // RIFF + WAVE at offset 8
    constexpr uint8_t FLAC[] = {0x66, 0x4C, 0x61, 0x43};  // fLaC
    constexpr uint8_t OGG[] = {0x4F, 0x67, 0x67, 0x53};  // OggS
    
    // Video
    constexpr uint8_t MP4[] = {0x00, 0x00, 0x00};  // + ftyp at offset 4
    constexpr uint8_t AVI[] = {0x52, 0x49, 0x46, 0x46};  // RIFF + AVI at offset 8
    constexpr uint8_t MKV[] = {0x1A, 0x45, 0xDF, 0xA3};
    constexpr uint8_t FLV[] = {0x46, 0x4C, 0x56};  // FLV
    
    // Database
    constexpr uint8_t SQLITE[] = {0x53, 0x51, 0x4C, 0x69, 0x74, 0x65};  // SQLite
    
    // Scripts (text-based but executable)
    constexpr uint8_t XML[] = {0x3C, 0x3F, 0x78, 0x6D, 0x6C};  // <?xml
    constexpr uint8_t HTML[] = {0x3C, 0x21, 0x44, 0x4F, 0x43};  // <!DOC
}

} // namespace Zer0
} // namespace SentinelFS
