#include "MagicBytes.h"
#include <algorithm>
#include <cctype>

namespace SentinelFS {
namespace Zer0 {

MagicBytes::MagicBytes() {
    initSignatures();
    initExtensionMap();
}

void MagicBytes::initSignatures() {
    // Images
    signatures_.push_back({{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}, 0, FileCategory::IMAGE, "PNG image", false});
    signatures_.push_back({{0xFF, 0xD8, 0xFF}, 0, FileCategory::IMAGE, "JPEG image", false});
    signatures_.push_back({{0x47, 0x49, 0x46, 0x38, 0x37, 0x61}, 0, FileCategory::IMAGE, "GIF87a image", false});
    signatures_.push_back({{0x47, 0x49, 0x46, 0x38, 0x39, 0x61}, 0, FileCategory::IMAGE, "GIF89a image", false});
    signatures_.push_back({{0x42, 0x4D}, 0, FileCategory::IMAGE, "BMP image", false});
    signatures_.push_back({{0x00, 0x00, 0x01, 0x00}, 0, FileCategory::IMAGE, "ICO icon", false});
    signatures_.push_back({{0x49, 0x49, 0x2A, 0x00}, 0, FileCategory::IMAGE, "TIFF image (LE)", false});
    signatures_.push_back({{0x4D, 0x4D, 0x00, 0x2A}, 0, FileCategory::IMAGE, "TIFF image (BE)", false});
    // AVIF/HEIF - ISO Base Media File Format (ftyp box at offset 4)
    signatures_.push_back({{0x66, 0x74, 0x79, 0x70, 0x61, 0x76, 0x69, 0x66}, 4, FileCategory::IMAGE, "AVIF image", false});  // ftypavif
    signatures_.push_back({{0x66, 0x74, 0x79, 0x70, 0x68, 0x65, 0x69, 0x63}, 4, FileCategory::IMAGE, "HEIC image", false});  // ftypheic
    signatures_.push_back({{0x66, 0x74, 0x79, 0x70, 0x68, 0x65, 0x69, 0x78}, 4, FileCategory::IMAGE, "HEIF image", false});  // ftypheix
    signatures_.push_back({{0x66, 0x74, 0x79, 0x70, 0x6D, 0x69, 0x66, 0x31}, 4, FileCategory::IMAGE, "HEIF image", false});  // ftypmif1
    // WebP
    signatures_.push_back({{0x52, 0x49, 0x46, 0x46}, 0, FileCategory::IMAGE, "WebP/RIFF image", false});  // RIFF (WebP starts with RIFF)
    
    // Documents
    signatures_.push_back({{0x25, 0x50, 0x44, 0x46}, 0, FileCategory::DOCUMENT, "PDF document", false});
    signatures_.push_back({{0x7B, 0x5C, 0x72, 0x74, 0x66}, 0, FileCategory::DOCUMENT, "RTF document", false});
    
    // Archives (also Office documents)
    signatures_.push_back({{0x50, 0x4B, 0x03, 0x04}, 0, FileCategory::ARCHIVE, "ZIP/Office archive", false});
    signatures_.push_back({{0x1F, 0x8B}, 0, FileCategory::ARCHIVE, "GZIP archive", false});
    signatures_.push_back({{0x42, 0x5A, 0x68}, 0, FileCategory::ARCHIVE, "BZIP2 archive", false});
    signatures_.push_back({{0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00}, 0, FileCategory::ARCHIVE, "XZ archive", false});
    signatures_.push_back({{0x28, 0xB5, 0x2F, 0xFD}, 0, FileCategory::ARCHIVE, "ZSTD archive", false});
    signatures_.push_back({{0x52, 0x61, 0x72, 0x21, 0x1A, 0x07}, 0, FileCategory::ARCHIVE, "RAR archive", false});
    signatures_.push_back({{0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C}, 0, FileCategory::ARCHIVE, "7-Zip archive", false});
    
    // Executables - CRITICAL for security
    signatures_.push_back({{0x7F, 0x45, 0x4C, 0x46}, 0, FileCategory::EXECUTABLE, "ELF executable", true});
    signatures_.push_back({{0x4D, 0x5A}, 0, FileCategory::EXECUTABLE, "Windows PE executable", true});
    signatures_.push_back({{0xFE, 0xED, 0xFA, 0xCE}, 0, FileCategory::EXECUTABLE, "Mach-O 32-bit", true});
    signatures_.push_back({{0xFE, 0xED, 0xFA, 0xCF}, 0, FileCategory::EXECUTABLE, "Mach-O 64-bit", true});
    signatures_.push_back({{0xCF, 0xFA, 0xED, 0xFE}, 0, FileCategory::EXECUTABLE, "Mach-O 64-bit (reverse)", true});
    signatures_.push_back({{0xCA, 0xFE, 0xBA, 0xBE}, 0, FileCategory::EXECUTABLE, "Java class/Mach-O fat", true});
    signatures_.push_back({{0x23, 0x21}, 0, FileCategory::EXECUTABLE, "Script (shebang)", true});
    
    // Audio
    signatures_.push_back({{0x49, 0x44, 0x33}, 0, FileCategory::AUDIO, "MP3 with ID3", false});
    signatures_.push_back({{0xFF, 0xFB}, 0, FileCategory::AUDIO, "MP3 audio", false});
    signatures_.push_back({{0xFF, 0xFA}, 0, FileCategory::AUDIO, "MP3 audio", false});
    signatures_.push_back({{0x66, 0x4C, 0x61, 0x43}, 0, FileCategory::AUDIO, "FLAC audio", false});
    signatures_.push_back({{0x4F, 0x67, 0x67, 0x53}, 0, FileCategory::AUDIO, "OGG audio", false});
    
    // Video
    signatures_.push_back({{0x1A, 0x45, 0xDF, 0xA3}, 0, FileCategory::VIDEO, "MKV/WebM video", false});
    signatures_.push_back({{0x46, 0x4C, 0x56}, 0, FileCategory::VIDEO, "FLV video", false});
    
    // Database
    signatures_.push_back({{0x53, 0x51, 0x4C, 0x69, 0x74, 0x65}, 0, FileCategory::DATABASE, "SQLite database", false});
    
    // Config/Text
    signatures_.push_back({{0x3C, 0x3F, 0x78, 0x6D, 0x6C}, 0, FileCategory::CONFIG, "XML document", false});
    signatures_.push_back({{0xEF, 0xBB, 0xBF}, 0, FileCategory::TEXT, "UTF-8 BOM text", false});
    signatures_.push_back({{0xFF, 0xFE}, 0, FileCategory::TEXT, "UTF-16 LE text", false});
    signatures_.push_back({{0xFE, 0xFF}, 0, FileCategory::TEXT, "UTF-16 BE text", false});
}

void MagicBytes::initExtensionMap() {
    // Images
    extensionMap_["png"] = FileCategory::IMAGE;
    extensionMap_["jpg"] = FileCategory::IMAGE;
    extensionMap_["jpeg"] = FileCategory::IMAGE;
    extensionMap_["gif"] = FileCategory::IMAGE;
    extensionMap_["bmp"] = FileCategory::IMAGE;
    extensionMap_["ico"] = FileCategory::IMAGE;
    extensionMap_["tiff"] = FileCategory::IMAGE;
    extensionMap_["tif"] = FileCategory::IMAGE;
    extensionMap_["webp"] = FileCategory::IMAGE;
    extensionMap_["svg"] = FileCategory::IMAGE;
    extensionMap_["heic"] = FileCategory::IMAGE;
    extensionMap_["heif"] = FileCategory::IMAGE;
    extensionMap_["avif"] = FileCategory::IMAGE;
    
    // Documents
    extensionMap_["pdf"] = FileCategory::DOCUMENT;
    extensionMap_["doc"] = FileCategory::DOCUMENT;
    extensionMap_["docx"] = FileCategory::DOCUMENT;
    extensionMap_["xls"] = FileCategory::DOCUMENT;
    extensionMap_["xlsx"] = FileCategory::DOCUMENT;
    extensionMap_["ppt"] = FileCategory::DOCUMENT;
    extensionMap_["pptx"] = FileCategory::DOCUMENT;
    extensionMap_["odt"] = FileCategory::DOCUMENT;
    extensionMap_["ods"] = FileCategory::DOCUMENT;
    extensionMap_["odp"] = FileCategory::DOCUMENT;
    extensionMap_["rtf"] = FileCategory::DOCUMENT;
    
    // Archives
    extensionMap_["zip"] = FileCategory::ARCHIVE;
    extensionMap_["tar"] = FileCategory::ARCHIVE;
    extensionMap_["gz"] = FileCategory::ARCHIVE;
    extensionMap_["bz2"] = FileCategory::ARCHIVE;
    extensionMap_["xz"] = FileCategory::ARCHIVE;
    extensionMap_["7z"] = FileCategory::ARCHIVE;
    extensionMap_["rar"] = FileCategory::ARCHIVE;
    extensionMap_["zst"] = FileCategory::ARCHIVE;
    
    // Executables
    extensionMap_["exe"] = FileCategory::EXECUTABLE;
    extensionMap_["dll"] = FileCategory::EXECUTABLE;
    extensionMap_["so"] = FileCategory::EXECUTABLE;
    extensionMap_["dylib"] = FileCategory::EXECUTABLE;
    extensionMap_["bin"] = FileCategory::EXECUTABLE;
    extensionMap_["elf"] = FileCategory::EXECUTABLE;
    extensionMap_["app"] = FileCategory::EXECUTABLE;
    extensionMap_["msi"] = FileCategory::EXECUTABLE;
    extensionMap_["deb"] = FileCategory::EXECUTABLE;
    extensionMap_["rpm"] = FileCategory::EXECUTABLE;
    extensionMap_["sh"] = FileCategory::EXECUTABLE;
    extensionMap_["bash"] = FileCategory::EXECUTABLE;
    extensionMap_["bat"] = FileCategory::EXECUTABLE;
    extensionMap_["cmd"] = FileCategory::EXECUTABLE;
    extensionMap_["ps1"] = FileCategory::EXECUTABLE;
    extensionMap_["py"] = FileCategory::EXECUTABLE;
    extensionMap_["pl"] = FileCategory::EXECUTABLE;
    extensionMap_["rb"] = FileCategory::EXECUTABLE;
    
    // Audio
    extensionMap_["mp3"] = FileCategory::AUDIO;
    extensionMap_["wav"] = FileCategory::AUDIO;
    extensionMap_["flac"] = FileCategory::AUDIO;
    extensionMap_["ogg"] = FileCategory::AUDIO;
    extensionMap_["m4a"] = FileCategory::AUDIO;
    extensionMap_["aac"] = FileCategory::AUDIO;
    extensionMap_["wma"] = FileCategory::AUDIO;
    
    // Video
    extensionMap_["mp4"] = FileCategory::VIDEO;
    extensionMap_["mkv"] = FileCategory::VIDEO;
    extensionMap_["avi"] = FileCategory::VIDEO;
    extensionMap_["mov"] = FileCategory::VIDEO;
    extensionMap_["wmv"] = FileCategory::VIDEO;
    extensionMap_["flv"] = FileCategory::VIDEO;
    extensionMap_["webm"] = FileCategory::VIDEO;
    
    // Database
    extensionMap_["db"] = FileCategory::DATABASE;
    extensionMap_["sqlite"] = FileCategory::DATABASE;
    extensionMap_["sqlite3"] = FileCategory::DATABASE;
    
    // Config
    extensionMap_["json"] = FileCategory::CONFIG;
    extensionMap_["yaml"] = FileCategory::CONFIG;
    extensionMap_["yml"] = FileCategory::CONFIG;
    extensionMap_["xml"] = FileCategory::CONFIG;
    extensionMap_["ini"] = FileCategory::CONFIG;
    extensionMap_["toml"] = FileCategory::CONFIG;
    extensionMap_["conf"] = FileCategory::CONFIG;
    extensionMap_["cfg"] = FileCategory::CONFIG;
    
    // Text/Source
    extensionMap_["txt"] = FileCategory::TEXT;
    extensionMap_["md"] = FileCategory::TEXT;
    extensionMap_["rst"] = FileCategory::TEXT;
    extensionMap_["log"] = FileCategory::TEXT;
    extensionMap_["csv"] = FileCategory::TEXT;
    extensionMap_["c"] = FileCategory::TEXT;
    extensionMap_["cpp"] = FileCategory::TEXT;
    extensionMap_["h"] = FileCategory::TEXT;
    extensionMap_["hpp"] = FileCategory::TEXT;
    extensionMap_["java"] = FileCategory::TEXT;
    extensionMap_["js"] = FileCategory::TEXT;
    extensionMap_["ts"] = FileCategory::TEXT;
    extensionMap_["tsx"] = FileCategory::TEXT;
    extensionMap_["jsx"] = FileCategory::TEXT;
    extensionMap_["html"] = FileCategory::TEXT;
    extensionMap_["css"] = FileCategory::TEXT;
    extensionMap_["scss"] = FileCategory::TEXT;
    extensionMap_["less"] = FileCategory::TEXT;
    extensionMap_["go"] = FileCategory::TEXT;
    extensionMap_["rs"] = FileCategory::TEXT;
    extensionMap_["swift"] = FileCategory::TEXT;
    extensionMap_["kt"] = FileCategory::TEXT;
    extensionMap_["scala"] = FileCategory::TEXT;
    extensionMap_["php"] = FileCategory::TEXT;
    extensionMap_["sql"] = FileCategory::TEXT;
}

FileCategory MagicBytes::detectCategory(const std::vector<uint8_t>& header) const {
    if (header.empty()) return FileCategory::UNKNOWN;
    
    for (const auto& sig : signatures_) {
        if (header.size() < sig.offset + sig.magic.size()) continue;
        
        bool match = true;
        for (size_t i = 0; i < sig.magic.size(); ++i) {
            if (header[sig.offset + i] != sig.magic[i]) {
                match = false;
                break;
            }
        }
        
        if (match) {
            return sig.category;
        }
    }
    
    // Check if it looks like text
    bool isText = true;
    for (size_t i = 0; i < std::min(header.size(), size_t(512)); ++i) {
        uint8_t c = header[i];
        // Allow printable ASCII, tabs, newlines, and UTF-8 continuation bytes
        if (c < 0x09 || (c > 0x0D && c < 0x20 && c != 0x1B) || c == 0x7F) {
            // Not a text character (excluding escape sequences)
            if (c < 0x80) {  // ASCII range
                isText = false;
                break;
            }
        }
    }
    
    if (isText) return FileCategory::TEXT;
    
    return FileCategory::UNKNOWN;
}

FileCategory MagicBytes::getCategoryForExtension(const std::string& extension) const {
    std::string ext = extension;
    // Remove leading dot if present
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    auto it = extensionMap_.find(ext);
    if (it != extensionMap_.end()) {
        return it->second;
    }
    return FileCategory::UNKNOWN;
}

bool MagicBytes::validateHeader(const std::vector<uint8_t>& header, FileCategory expected) const {
    FileCategory detected = detectCategory(header);
    
    // Special cases
    if (expected == FileCategory::DOCUMENT && detected == FileCategory::ARCHIVE) {
        // Office documents are ZIP archives - this is valid
        return true;
    }
    
    if (expected == FileCategory::UNKNOWN) {
        // Can't validate unknown expected type
        return true;
    }
    
    if (detected == FileCategory::UNKNOWN) {
        // Couldn't detect - might be valid
        return true;
    }
    
    return detected == expected;
}

std::string MagicBytes::getDescription(const std::vector<uint8_t>& header) const {
    if (header.empty()) return "Empty file";
    
    for (const auto& sig : signatures_) {
        if (header.size() < sig.offset + sig.magic.size()) continue;
        
        bool match = true;
        for (size_t i = 0; i < sig.magic.size(); ++i) {
            if (header[sig.offset + i] != sig.magic[i]) {
                match = false;
                break;
            }
        }
        
        if (match) {
            return sig.description;
        }
    }
    
    return "Unknown format";
}

bool MagicBytes::isExecutable(const std::vector<uint8_t>& header) const {
    if (header.empty()) return false;
    
    for (const auto& sig : signatures_) {
        if (!sig.isExecutable) continue;
        if (header.size() < sig.offset + sig.magic.size()) continue;
        
        bool match = true;
        for (size_t i = 0; i < sig.magic.size(); ++i) {
            if (header[sig.offset + i] != sig.magic[i]) {
                match = false;
                break;
            }
        }
        
        if (match) {
            return true;
        }
    }
    
    return false;
}

bool MagicBytes::hasEmbeddedScript(const std::vector<uint8_t>& content) const {
    if (content.size() < 100) return false;
    
    // Look for script patterns in content
    const std::vector<std::vector<uint8_t>> scriptPatterns = {
        {'<', 's', 'c', 'r', 'i', 'p', 't'},           // <script
        {'<', 'S', 'C', 'R', 'I', 'P', 'T'},           // <SCRIPT
        {'j', 'a', 'v', 'a', 's', 'c', 'r', 'i', 'p', 't', ':'},  // javascript:
        {'v', 'b', 's', 'c', 'r', 'i', 'p', 't', ':'},  // vbscript:
        {'p', 'o', 'w', 'e', 'r', 's', 'h', 'e', 'l', 'l'},  // powershell
        {'c', 'm', 'd', '.', 'e', 'x', 'e'},           // cmd.exe
        {'w', 's', 'c', 'r', 'i', 'p', 't'},           // wscript
        {'c', 's', 'c', 'r', 'i', 'p', 't'},           // cscript
    };
    
    for (const auto& pattern : scriptPatterns) {
        auto it = std::search(content.begin(), content.end(), pattern.begin(), pattern.end());
        if (it != content.end()) {
            return true;
        }
    }
    
    return false;
}

} // namespace Zer0
} // namespace SentinelFS
