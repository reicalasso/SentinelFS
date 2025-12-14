#include "YaraScanner.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <cstring>

// Note: This implementation provides a YARA-compatible interface
// For full YARA support, link against libyara and use yr_* functions

namespace SentinelFS {
namespace Zer0 {

// ============================================================================
// Simple pattern matching engine (YARA-like functionality without libyara)
// ============================================================================

struct CompiledPattern {
    std::string name;
    std::vector<uint8_t> pattern;
    std::vector<uint8_t> mask;  // 0xFF = exact match, 0x00 = wildcard
    bool isRegex{false};
    bool isWide{false};
    bool noCase{false};
};

struct CompiledRule {
    std::string name;
    std::string description;
    std::string category;
    std::string severity;
    std::vector<std::string> tags;
    std::map<std::string, std::string> metadata;
    std::vector<CompiledPattern> patterns;
    std::string condition;
};

class YaraScanner::Impl {
public:
    std::vector<std::string> ruleStrings;
    std::vector<CompiledRule> compiledRules;
    MatchCallback matchCallback;
    int timeoutMs{60000};
    std::string lastError;
    Stats stats;
    mutable std::mutex mutex;
    bool initialized{false};
    
    // Simple pattern matching
    std::vector<std::pair<size_t, size_t>> findPattern(
        const std::vector<uint8_t>& data,
        const CompiledPattern& pattern) {
        
        std::vector<std::pair<size_t, size_t>> matches;
        
        if (pattern.pattern.empty() || data.size() < pattern.pattern.size()) {
            return matches;
        }
        
        for (size_t i = 0; i <= data.size() - pattern.pattern.size(); ++i) {
            bool match = true;
            for (size_t j = 0; j < pattern.pattern.size(); ++j) {
                uint8_t dataByte = data[i + j];
                uint8_t patternByte = pattern.pattern[j];
                uint8_t maskByte = pattern.mask.size() > j ? pattern.mask[j] : 0xFF;
                
                if (pattern.noCase && maskByte == 0xFF) {
                    dataByte = std::tolower(dataByte);
                    patternByte = std::tolower(patternByte);
                }
                
                if ((dataByte & maskByte) != (patternByte & maskByte)) {
                    match = false;
                    break;
                }
            }
            
            if (match) {
                matches.push_back({i, pattern.pattern.size()});
                // Skip ahead to avoid overlapping matches
                i += pattern.pattern.size() - 1;
            }
        }
        
        return matches;
    }
    
    // Parse hex pattern like { 90 90 ?? 90 }
    CompiledPattern parseHexPattern(const std::string& name, const std::string& hex) {
        CompiledPattern pattern;
        pattern.name = name;
        
        std::string cleaned;
        for (char c : hex) {
            if (std::isxdigit(c) || c == '?') {
                cleaned += c;
            }
        }
        
        for (size_t i = 0; i < cleaned.size(); i += 2) {
            if (i + 1 >= cleaned.size()) break;
            
            if (cleaned[i] == '?' && cleaned[i + 1] == '?') {
                pattern.pattern.push_back(0x00);
                pattern.mask.push_back(0x00);
            } else {
                char hex_str[3] = {cleaned[i], cleaned[i + 1], '\0'};
                pattern.pattern.push_back(static_cast<uint8_t>(std::strtol(hex_str, nullptr, 16)));
                pattern.mask.push_back(0xFF);
            }
        }
        
        return pattern;
    }
    
    // Parse string pattern
    CompiledPattern parseStringPattern(const std::string& name, const std::string& str,
                                        bool wide, bool nocase) {
        CompiledPattern pattern;
        pattern.name = name;
        pattern.isWide = wide;
        pattern.noCase = nocase;
        
        for (char c : str) {
            pattern.pattern.push_back(static_cast<uint8_t>(c));
            pattern.mask.push_back(0xFF);
            
            if (wide) {
                pattern.pattern.push_back(0x00);
                pattern.mask.push_back(0xFF);
            }
        }
        
        return pattern;
    }
    
    // Simple YARA rule parser
    bool parseRule(const std::string& ruleText, CompiledRule& rule) {
        // Very simplified parser - real implementation would use proper lexer/parser
        std::istringstream iss(ruleText);
        std::string line;
        
        enum class Section { NONE, META, STRINGS, CONDITION };
        Section section = Section::NONE;
        
        while (std::getline(iss, line)) {
            // Trim whitespace
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            line = line.substr(start);
            
            // Skip comments
            if (line.empty() || line[0] == '/' || line[0] == '*') continue;
            
            // Check for rule start
            if (line.find("rule ") == 0) {
                size_t nameStart = 5;
                size_t nameEnd = line.find_first_of(" {", nameStart);
                rule.name = line.substr(nameStart, nameEnd - nameStart);
                continue;
            }
            
            // Check for section markers
            if (line.find("meta:") == 0) {
                section = Section::META;
                continue;
            } else if (line.find("strings:") == 0) {
                section = Section::STRINGS;
                continue;
            } else if (line.find("condition:") == 0) {
                section = Section::CONDITION;
                continue;
            }
            
            // Parse section content
            switch (section) {
                case Section::META: {
                    size_t eqPos = line.find('=');
                    if (eqPos != std::string::npos) {
                        std::string key = line.substr(0, eqPos);
                        std::string value = line.substr(eqPos + 1);
                        
                        // Trim
                        key.erase(key.find_last_not_of(" \t\"") + 1);
                        key.erase(0, key.find_first_not_of(" \t\""));
                        value.erase(value.find_last_not_of(" \t\"") + 1);
                        value.erase(0, value.find_first_not_of(" \t\""));
                        
                        rule.metadata[key] = value;
                        
                        if (key == "description") rule.description = value;
                        else if (key == "severity") rule.severity = value;
                        else if (key == "category") rule.category = value;
                    }
                    break;
                }
                
                case Section::STRINGS: {
                    size_t eqPos = line.find('=');
                    if (eqPos != std::string::npos) {
                        std::string name = line.substr(0, eqPos);
                        std::string value = line.substr(eqPos + 1);
                        
                        name.erase(name.find_last_not_of(" \t") + 1);
                        name.erase(0, name.find_first_not_of(" \t$"));
                        
                        bool wide = value.find("wide") != std::string::npos;
                        bool nocase = value.find("nocase") != std::string::npos;
                        
                        // Check if hex pattern
                        size_t hexStart = value.find('{');
                        size_t hexEnd = value.find('}');
                        if (hexStart != std::string::npos && hexEnd != std::string::npos) {
                            std::string hex = value.substr(hexStart + 1, hexEnd - hexStart - 1);
                            rule.patterns.push_back(parseHexPattern(name, hex));
                        } else {
                            // String pattern
                            size_t strStart = value.find('"');
                            size_t strEnd = value.rfind('"');
                            if (strStart != std::string::npos && strEnd > strStart) {
                                std::string str = value.substr(strStart + 1, strEnd - strStart - 1);
                                rule.patterns.push_back(parseStringPattern(name, str, wide, nocase));
                            }
                        }
                    }
                    break;
                }
                
                case Section::CONDITION: {
                    rule.condition += line + " ";
                    break;
                }
                
                default:
                    break;
            }
        }
        
        return !rule.name.empty();
    }
    
    // Evaluate condition (simplified)
    bool evaluateCondition(const CompiledRule& rule, 
                           const std::map<std::string, std::vector<std::pair<size_t, size_t>>>& matches) {
        // Simplified condition evaluation
        // Real implementation would parse and evaluate the full YARA condition syntax
        
        std::string cond = rule.condition;
        
        // "any of them" - any pattern matches
        if (cond.find("any of them") != std::string::npos) {
            for (const auto& [name, offsets] : matches) {
                if (!offsets.empty()) return true;
            }
            return false;
        }
        
        // "all of them" - all patterns match
        if (cond.find("all of them") != std::string::npos) {
            for (const auto& pattern : rule.patterns) {
                auto it = matches.find(pattern.name);
                if (it == matches.end() || it->second.empty()) return false;
            }
            return true;
        }
        
        // "X of ($pattern*)" - X patterns from group match
        // Simplified: just check if any pattern matches
        for (const auto& [name, offsets] : matches) {
            if (!offsets.empty()) return true;
        }
        
        return false;
    }
};

// ============================================================================
// YaraScanner Implementation
// ============================================================================

YaraScanner::YaraScanner() : impl_(std::make_unique<Impl>()) {}

YaraScanner::~YaraScanner() = default;

bool YaraScanner::initialize() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->initialized = true;
    return true;
}

void YaraScanner::shutdown() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->initialized = false;
    impl_->compiledRules.clear();
    impl_->ruleStrings.clear();
}

bool YaraScanner::loadRulesFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        impl_->lastError = "Failed to open file: " + path;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadRulesFromString(buffer.str(), std::filesystem::path(path).stem().string());
}

bool YaraScanner::loadRulesFromString(const std::string& rules, const std::string& namespace_) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ruleStrings.push_back(rules);
    return true;
}

bool YaraScanner::loadRulesFromDirectory(const std::string& dirPath) {
    if (!std::filesystem::exists(dirPath)) {
        impl_->lastError = "Directory does not exist: " + dirPath;
        return false;
    }
    
    bool success = true;
    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        if (entry.path().extension() == ".yar" || entry.path().extension() == ".yara") {
            if (!loadRulesFromFile(entry.path().string())) {
                success = false;
            }
        }
    }
    
    return success;
}

bool YaraScanner::compileRules() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    impl_->compiledRules.clear();
    
    for (const auto& ruleString : impl_->ruleStrings) {
        // Split into individual rules
        std::istringstream iss(ruleString);
        std::string line;
        std::string currentRule;
        int braceCount = 0;
        bool inRule = false;
        
        while (std::getline(iss, line)) {
            if (line.find("rule ") != std::string::npos && !inRule) {
                inRule = true;
                currentRule = line + "\n";
                braceCount = 0;
                for (char c : line) {
                    if (c == '{') braceCount++;
                    if (c == '}') braceCount--;
                }
            } else if (inRule) {
                currentRule += line + "\n";
                for (char c : line) {
                    if (c == '{') braceCount++;
                    if (c == '}') braceCount--;
                }
                
                if (braceCount <= 0) {
                    CompiledRule rule;
                    if (impl_->parseRule(currentRule, rule)) {
                        impl_->compiledRules.push_back(rule);
                    }
                    inRule = false;
                    currentRule.clear();
                }
            }
        }
    }
    
    return !impl_->compiledRules.empty();
}

YaraScanResult YaraScanner::scanFile(const std::string& path) {
    YaraScanResult result;
    result.filePath = path;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Read file
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        result.error = "Failed to open file: " + path;
        impl_->stats.scanErrors++;
        return result;
    }
    
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
    
    result = scanBuffer(buffer, path);
    result.filePath = path;
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.scanTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    impl_->stats.filesScanned++;
    impl_->stats.bytesScanned += buffer.size();
    impl_->stats.totalScanTimeMs += result.scanTimeMs;
    
    return result;
}

YaraScanResult YaraScanner::scanBuffer(const std::vector<uint8_t>& buffer, const std::string& identifier) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    YaraScanResult result;
    result.success = true;
    result.bytesScanned = buffer.size();
    
    for (const auto& rule : impl_->compiledRules) {
        std::map<std::string, std::vector<std::pair<size_t, size_t>>> patternMatches;
        
        // Find all pattern matches
        for (const auto& pattern : rule.patterns) {
            auto matches = impl_->findPattern(buffer, pattern);
            if (!matches.empty()) {
                patternMatches[pattern.name] = matches;
            }
        }
        
        // Evaluate condition
        if (impl_->evaluateCondition(rule, patternMatches)) {
            YaraMatch match;
            match.ruleName = rule.name;
            match.ruleDescription = rule.description;
            match.category = rule.category;
            match.severity = rule.severity;
            match.metadata = rule.metadata;
            
            for (const auto& [name, offsets] : patternMatches) {
                match.matchedStrings.push_back(name);
                for (const auto& [offset, length] : offsets) {
                    match.matchOffsets.push_back({offset, length});
                }
            }
            
            result.matches.push_back(match);
            impl_->stats.matchesFound++;
            
            if (impl_->matchCallback) {
                impl_->matchCallback(match);
            }
        }
    }
    
    return result;
}

YaraScanResult YaraScanner::scanProcess(pid_t pid) {
    YaraScanResult result;
    
#ifdef __linux__
    std::string mapsPath = "/proc/" + std::to_string(pid) + "/maps";
    std::string memPath = "/proc/" + std::to_string(pid) + "/mem";
    
    std::ifstream mapsFile(mapsPath);
    if (!mapsFile) {
        result.error = "Failed to open process maps";
        return result;
    }
    
    // Read memory regions and scan them
    // This is a simplified implementation
    result.success = true;
#else
    result.error = "Process scanning not supported on this platform";
#endif
    
    return result;
}

std::vector<YaraScanResult> YaraScanner::scanDirectory(
    const std::string& dirPath,
    bool recursive,
    ScanProgressCallback progressCallback) {
    
    std::vector<YaraScanResult> results;
    
    if (!std::filesystem::exists(dirPath)) {
        return results;
    }
    
    std::vector<std::string> files;
    
    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().string());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().string());
            }
        }
    }
    
    for (size_t i = 0; i < files.size(); ++i) {
        if (progressCallback) {
            int percent = static_cast<int>((i * 100) / files.size());
            progressCallback(files[i], percent);
        }
        
        results.push_back(scanFile(files[i]));
    }
    
    if (progressCallback) {
        progressCallback("", 100);
    }
    
    return results;
}

void YaraScanner::setMatchCallback(MatchCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->matchCallback = callback;
}

void YaraScanner::setTimeout(int timeoutMs) {
    impl_->timeoutMs = timeoutMs;
}

int YaraScanner::getRuleCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->compiledRules.size();
}

std::vector<std::string> YaraScanner::getRuleNames() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<std::string> names;
    for (const auto& rule : impl_->compiledRules) {
        names.push_back(rule.name);
    }
    return names;
}

bool YaraScanner::hasRule(const std::string& ruleName) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (const auto& rule : impl_->compiledRules) {
        if (rule.name == ruleName) return true;
    }
    return false;
}

void YaraScanner::clearRules() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->compiledRules.clear();
    impl_->ruleStrings.clear();
}

std::string YaraScanner::getLastError() const {
    return impl_->lastError;
}

YaraScanner::Stats YaraScanner::getStats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stats;
}

void YaraScanner::resetStats() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->stats = Stats{};
}

// ============================================================================
// YaraRuleBuilder Implementation
// ============================================================================

YaraRuleBuilder::YaraRuleBuilder(const std::string& ruleName) : name_(ruleName) {}

YaraRuleBuilder& YaraRuleBuilder::setDescription(const std::string& desc) {
    meta_["description"] = desc;
    return *this;
}

YaraRuleBuilder& YaraRuleBuilder::setAuthor(const std::string& author) {
    meta_["author"] = author;
    return *this;
}

YaraRuleBuilder& YaraRuleBuilder::setSeverity(const std::string& severity) {
    meta_["severity"] = severity;
    return *this;
}

YaraRuleBuilder& YaraRuleBuilder::setCategory(const std::string& category) {
    meta_["category"] = category;
    return *this;
}

YaraRuleBuilder& YaraRuleBuilder::addTag(const std::string& tag) {
    tags_.push_back(tag);
    return *this;
}

YaraRuleBuilder& YaraRuleBuilder::addMeta(const std::string& key, const std::string& value) {
    meta_[key] = value;
    return *this;
}

YaraRuleBuilder& YaraRuleBuilder::addStringPattern(const std::string& name, const std::string& pattern,
                                                    bool wide, bool nocase, bool ascii) {
    std::string str = "$" + name + " = \"" + pattern + "\"";
    if (ascii) str += " ascii";
    if (wide) str += " wide";
    if (nocase) str += " nocase";
    strings_.push_back(str);
    return *this;
}

YaraRuleBuilder& YaraRuleBuilder::addHexPattern(const std::string& name, const std::string& hexPattern) {
    strings_.push_back("$" + name + " = { " + hexPattern + " }");
    return *this;
}

YaraRuleBuilder& YaraRuleBuilder::addRegexPattern(const std::string& name, const std::string& regex) {
    strings_.push_back("$" + name + " = /" + regex + "/");
    return *this;
}

YaraRuleBuilder& YaraRuleBuilder::setCondition(const std::string& condition) {
    condition_ = condition;
    return *this;
}

std::string YaraRuleBuilder::build() const {
    std::ostringstream oss;
    
    oss << "rule " << name_;
    
    if (!tags_.empty()) {
        oss << " : ";
        for (size_t i = 0; i < tags_.size(); ++i) {
            if (i > 0) oss << " ";
            oss << tags_[i];
        }
    }
    
    oss << " {\n";
    
    if (!meta_.empty()) {
        oss << "    meta:\n";
        for (const auto& [key, value] : meta_) {
            oss << "        " << key << " = \"" << value << "\"\n";
        }
    }
    
    if (!strings_.empty()) {
        oss << "    strings:\n";
        for (const auto& str : strings_) {
            oss << "        " << str << "\n";
        }
    }
    
    oss << "    condition:\n";
    oss << "        " << (condition_.empty() ? "any of them" : condition_) << "\n";
    
    oss << "}\n";
    
    return oss.str();
}

} // namespace Zer0
} // namespace SentinelFS
