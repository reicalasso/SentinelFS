#include "PatternMatcher.h"
#include <fstream>
#include <algorithm>
#include <filesystem>

namespace SentinelFS {

// Known ransomware extensions
static const std::vector<std::string> RANSOMWARE_EXTENSIONS = {
    // Common ransomware extensions
    ".locked", ".encrypted", ".crypto", ".crypt", ".crypted",
    ".enc", ".locky", ".cerber", ".zepto", ".odin",
    ".thor", ".aesir", ".zzzzz", ".micro", ".mp3",  // Teslacrypt
    ".vvv", ".ccc", ".xxx", ".ttt", ".abc",
    ".ecc", ".ezz", ".exx", ".xyz",
    ".aaa", ".rrr", ".zzz",
    ".wncry", ".wcry", ".wncrypt",  // WannaCry
    ".WNCRY", ".WCRY",
    ".petya", ".petwrap", ".notpetya",
    ".bad", ".badsanta",
    ".dharma", ".arrow", ".adobe", ".combo", ".cmb",
    ".wallet", ".onion",
    ".sage", ".globe", ".purge",
    ".cryptolocker", ".cryptowall", ".cryp1",
    ".kratos", ".mira", ".gefest", ".cuba", ".id",
    ".conti", ".lockbit", ".ryuk",
    ".revil", ".sodinokibi", ".darkside",
    ".babuk", ".blackmatter", ".hive",
    ".avoslocker", ".blackcat", ".alphv",
    // Generic suspicious
    ".pay", ".pay2key", ".ransom",
    ".ciphered", ".encoded", ".crypttt",
};

// Known ransom note filenames
static const std::vector<std::string> RANSOM_NOTE_NAMES = {
    // Case insensitive patterns
    "readme.txt", "readme.html", "readme.hta",
    "readme_encrypted.txt", "readme_encrypted.html",
    "decrypt_your_files.txt", "decrypt_your_files.html",
    "how_to_decrypt.txt", "how_to_decrypt.html",
    "how_to_recover.txt", "how_to_recover.html",
    "decrypt_instructions.txt", "decrypt_instructions.html",
    "your_files.txt", "your_files.html",
    "restore_files.txt", "restore_files.html",
    "help_decrypt.txt", "help_decrypt.html",
    "read_me.txt", "read_me.html", "read_it.txt",
    "!readme!.txt", "_readme.txt",
    "attention.txt", "attention!!!.txt",
    "warning.txt", "warning!!!.txt",
    "important.txt", "important!!!.txt",
    "recovery.txt", "recovery_key.txt",
    "ransom_note.txt",
    // Specific ransomware notes
    "@please_read_me@.txt",  // WannaCry
    "_help_instructions.html",
    "how_can_i_decrypt_my_files.txt",
    "#decryption#.txt",
    "!!!read_this!!!.txt",
    "files_encrypted.txt",
    "unlock_files.txt",
    "pay_ransom.txt",
    "bitcoin.txt", "monero.txt",
};

PatternMatcher::PatternMatcher() {
    initializePatterns();
}

void PatternMatcher::initializePatterns() {
    // Extension patterns
    for (const auto& ext : RANSOMWARE_EXTENSIONS) {
        std::string escaped = std::regex_replace(ext, std::regex("\\."), "\\.");
        extensionPatterns_.push_back({
            "RANSOMWARE_EXTENSION:" + ext,
            std::regex(escaped + "$", std::regex::icase),
            ThreatLevel::HIGH,
            "Known ransomware extension detected"
        });
    }
    
    // Filename patterns for ransom notes
    for (const auto& name : RANSOM_NOTE_NAMES) {
        std::string escaped = std::regex_replace(name, std::regex("([.!@#$%^&*()\\[\\]{}|\\\\])"), "\\$1");
        filenamePatterns_.push_back({
            "RANSOM_NOTE:" + name,
            std::regex("(^|[/\\\\])" + escaped + "$", std::regex::icase),
            ThreatLevel::CRITICAL,
            "Potential ransom note detected"
        });
    }
    
    // Pattern for encrypted-looking filenames (original.ext.encrypted)
    filenamePatterns_.push_back({
        "DOUBLE_EXTENSION",
        std::regex("\\.[a-z0-9]{1,5}\\.(locked|encrypted|crypto|crypt|enc)$", std::regex::icase),
        ThreatLevel::HIGH,
        "Suspicious double extension (original extension + encryption extension)"
    });
    
    // Pattern for UUID/random-looking extensions
    filenamePatterns_.push_back({
        "RANDOM_EXTENSION",
        std::regex("\\.[a-f0-9]{6,}$", std::regex::icase),
        ThreatLevel::MEDIUM,
        "Random/UUID-like file extension"
    });
    
    // Pattern for files with IDs in name (file.id[xyz].ext)
    filenamePatterns_.push_back({
        "ID_IN_FILENAME",
        std::regex("\\.(id\\[[a-zA-Z0-9_-]+\\]|\\[[a-zA-Z0-9_-]+\\]\\.)\\w+$", std::regex::icase),
        ThreatLevel::MEDIUM,
        "ID pattern in filename (common in ransomware)"
    });
    
    // Content patterns (for checking file headers)
    contentPatterns_.push_back({
        "BITCOIN_ADDRESS",
        std::regex("\\b[13][a-km-zA-HJ-NP-Z1-9]{25,34}\\b"),
        ThreatLevel::MEDIUM,
        "Bitcoin address found in file"
    });
    
    contentPatterns_.push_back({
        "MONERO_ADDRESS",
        std::regex("\\b4[0-9AB][1-9A-HJ-NP-Za-km-z]{93}\\b"),
        ThreatLevel::MEDIUM,
        "Monero address found in file"
    });
    
    contentPatterns_.push_back({
        "RANSOM_KEYWORDS",
        std::regex("(your files (have been|are) encrypted|pay (the ransom|bitcoin|monero)|decrypt(ion)? key|recover your files)", std::regex::icase),
        ThreatLevel::CRITICAL,
        "Ransom-related text found in file"
    });
}

PatternMatcher::PatternMatch PatternMatcher::checkPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    totalChecks_++;
    
    PatternMatch result;
    std::string filename = extractFilename(path);
    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    
    // Check extension patterns
    for (const auto& pattern : extensionPatterns_) {
        if (std::regex_search(lowerPath, pattern.pattern)) {
            result.matched = true;
            result.level = pattern.level;
            result.patternName = pattern.name;
            result.description = pattern.description;
            result.matchedValue = extractExtension(path);
            
            totalMatches_++;
            matchCounts_[pattern.name]++;
            return result;
        }
    }
    
    // Check filename patterns
    for (const auto& pattern : filenamePatterns_) {
        if (std::regex_search(path, pattern.pattern)) {
            result.matched = true;
            result.level = pattern.level;
            result.patternName = pattern.name;
            result.description = pattern.description;
            result.matchedValue = filename;
            
            totalMatches_++;
            matchCounts_[pattern.name]++;
            return result;
        }
    }
    
    // Check custom patterns
    for (const auto& pattern : customPatterns_) {
        if (std::regex_search(path, pattern.pattern)) {
            result.matched = true;
            result.level = pattern.level;
            result.patternName = pattern.name;
            result.description = pattern.description;
            result.matchedValue = path;
            
            totalMatches_++;
            matchCounts_[pattern.name]++;
            return result;
        }
    }
    
    return result;
}

PatternMatcher::PatternMatch PatternMatcher::checkContent(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PatternMatch result;
    
    // Only check text-like files
    std::string ext = extractExtension(path);
    std::vector<std::string> textExts = {".txt", ".html", ".htm", ".hta", ".rtf", ".md"};
    
    bool isTextFile = std::find(textExts.begin(), textExts.end(), ext) != textExts.end();
    if (!isTextFile) {
        return result;
    }
    
    // Read file content (limited)
    std::ifstream file(path);
    if (!file) {
        return result;
    }
    
    // Read first 10KB
    std::string content;
    content.resize(10240);
    file.read(&content[0], content.size());
    content.resize(file.gcount());
    
    // Check content patterns
    for (const auto& pattern : contentPatterns_) {
        std::smatch match;
        if (std::regex_search(content, match, pattern.pattern)) {
            result.matched = true;
            result.level = pattern.level;
            result.patternName = pattern.name;
            result.description = pattern.description;
            result.matchedValue = match.str().substr(0, 100);  // Limit matched text
            
            totalMatches_++;
            matchCounts_[pattern.name]++;
            return result;
        }
    }
    
    return result;
}

void PatternMatcher::recordEvent(const std::string& action, const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    FileEvent event;
    event.timestamp = std::chrono::steady_clock::now();
    event.action = action;
    event.newPath = path;
    
    // Track renames (CREATE followed by DELETE of similar file)
    if (action == "CREATE") {
        // Check if there was a recent delete that looks like a rename
        for (auto it = recentEvents_.rbegin(); it != recentEvents_.rend(); ++it) {
            if (it->action == "DELETE") {
                // Check if the paths are similar (same directory, different extension)
                std::filesystem::path newP(path);
                std::filesystem::path oldP(it->newPath);
                
                if (newP.parent_path() == oldP.parent_path() &&
                    newP.stem() == oldP.stem() &&
                    newP.extension() != oldP.extension()) {
                    renamedFiles_[it->newPath] = path;
                    event.oldPath = it->newPath;
                    break;
                }
            }
        }
    }
    
    recentEvents_.push_back(event);
    pruneOldEvents();
}

PatternMatcher::PatternMatch PatternMatcher::checkMassRenamePattern() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PatternMatch result;
    
    if (renamedFiles_.size() < static_cast<size_t>(MASS_RENAME_THRESHOLD)) {
        return result;
    }
    
    auto now = std::chrono::steady_clock::now();
    int recentRenames = 0;
    std::set<std::string> newExtensions;
    
    for (const auto& [oldPath, newPath] : renamedFiles_) {
        // Find the event
        for (const auto& event : recentEvents_) {
            if (event.newPath == newPath && event.oldPath == oldPath) {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(
                    now - event.timestamp
                ).count();
                
                if (age <= MASS_RENAME_WINDOW_SECONDS) {
                    recentRenames++;
                    std::string ext = extractExtension(newPath);
                    newExtensions.insert(ext);
                }
                break;
            }
        }
    }
    
    // Mass rename with single new extension = very suspicious
    if (recentRenames >= MASS_RENAME_THRESHOLD) {
        result.matched = true;
        result.patternName = "MASS_RENAME";
        
        if (newExtensions.size() == 1) {
            result.level = ThreatLevel::CRITICAL;
            result.description = "Mass file rename to single extension detected";
            result.matchedValue = std::to_string(recentRenames) + " files -> " + *newExtensions.begin();
        } else {
            result.level = ThreatLevel::HIGH;
            result.description = "Mass file rename detected";
            result.matchedValue = std::to_string(recentRenames) + " files renamed";
        }
        
        totalMatches_++;
        matchCounts_["MASS_RENAME"]++;
    }
    
    return result;
}

const std::vector<std::string>& PatternMatcher::getKnownRansomwareExtensions() {
    return RANSOMWARE_EXTENSIONS;
}

const std::vector<std::string>& PatternMatcher::getKnownRansomNoteNames() {
    return RANSOM_NOTE_NAMES;
}

void PatternMatcher::addCustomPattern(const std::string& name, const std::string& regexPattern,
                                       ThreatLevel level, const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        customPatterns_.push_back({
            name,
            std::regex(regexPattern, std::regex::icase),
            level,
            description
        });
    } catch (const std::regex_error&) {
        // Invalid regex, ignore
    }
}

void PatternMatcher::removeCustomPattern(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    customPatterns_.erase(
        std::remove_if(customPatterns_.begin(), customPatterns_.end(),
                      [&name](const RansomwarePattern& p) { return p.name == name; }),
        customPatterns_.end()
    );
}

std::string PatternMatcher::threatLevelToString(ThreatLevel level) {
    switch (level) {
        case ThreatLevel::NONE: return "NONE";
        case ThreatLevel::LOW: return "LOW";
        case ThreatLevel::MEDIUM: return "MEDIUM";
        case ThreatLevel::HIGH: return "HIGH";
        case ThreatLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

std::string PatternMatcher::extractExtension(const std::string& path) const {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    if (ext.empty()) return "";
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

std::string PatternMatcher::extractFilename(const std::string& path) const {
    std::filesystem::path p(path);
    return p.filename().string();
}

void PatternMatcher::pruneOldEvents() {
    auto now = std::chrono::steady_clock::now();
    
    // Remove old events
    recentEvents_.erase(
        std::remove_if(recentEvents_.begin(), recentEvents_.end(),
                      [&now](const FileEvent& e) {
                          auto age = std::chrono::duration_cast<std::chrono::seconds>(
                              now - e.timestamp
                          ).count();
                          return age > MASS_RENAME_WINDOW_SECONDS * 2;
                      }),
        recentEvents_.end()
    );
    
    // Limit size
    while (recentEvents_.size() > MAX_RECENT_EVENTS) {
        recentEvents_.erase(recentEvents_.begin());
    }
    
    // Prune old renamed files
    for (auto it = renamedFiles_.begin(); it != renamedFiles_.end();) {
        bool found = false;
        for (const auto& event : recentEvents_) {
            if (event.oldPath == it->first) {
                found = true;
                break;
            }
        }
        if (!found) {
            it = renamedFiles_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace SentinelFS
