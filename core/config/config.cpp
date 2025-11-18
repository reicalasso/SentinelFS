#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace sfs {
namespace core {

Config::Config() {
}

Config::~Config() {
}

bool Config::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    
    return load_from_string(buffer.str());
}

bool Config::load_from_string(const std::string& json) {
    // NOTE: This is a MINIMAL JSON parser for demonstration.
    // For production use, integrate nlohmann/json or similar library.
    
    // For now, just store some hardcoded defaults
    // This will be replaced with proper JSON parsing
    clear();
    
    // Parse would happen here
    // For MVP, we'll support simple key-value pairs
    
    return parse_json(json);
}

std::string Config::get_string(const std::string& key, const std::string& default_value) const {
    const ConfigValue* val = get_value(key);
    if (val && std::holds_alternative<std::string>(*val)) {
        return std::get<std::string>(*val);
    }
    return default_value;
}

int64_t Config::get_int(const std::string& key, int64_t default_value) const {
    const ConfigValue* val = get_value(key);
    if (val && std::holds_alternative<int64_t>(*val)) {
        return std::get<int64_t>(*val);
    }
    return default_value;
}

double Config::get_double(const std::string& key, double default_value) const {
    const ConfigValue* val = get_value(key);
    if (val && std::holds_alternative<double>(*val)) {
        return std::get<double>(*val);
    }
    return default_value;
}

bool Config::get_bool(const std::string& key, bool default_value) const {
    const ConfigValue* val = get_value(key);
    if (val && std::holds_alternative<bool>(*val)) {
        return std::get<bool>(*val);
    }
    return default_value;
}

std::vector<std::string> Config::get_string_array(const std::string& key) const {
    const ConfigValue* val = get_value(key);
    if (val && std::holds_alternative<std::vector<std::string>>(*val)) {
        return std::get<std::vector<std::string>>(*val);
    }
    return {};
}

bool Config::has_key(const std::string& key) const {
    return data_.find(key) != data_.end();
}

void Config::set_string(const std::string& key, const std::string& value) {
    data_[key] = value;
}

void Config::set_int(const std::string& key, int64_t value) {
    data_[key] = value;
}

void Config::set_double(const std::string& key, double value) {
    data_[key] = value;
}

void Config::set_bool(const std::string& key, bool value) {
    data_[key] = value;
}

void Config::clear() {
    data_.clear();
}

const ConfigValue* Config::get_value(const std::string& key) const {
    auto it = data_.find(key);
    if (it != data_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool Config::parse_json(const std::string& json) {
    // MINIMAL JSON PARSER - FOR DEMONSTRATION ONLY
    // TODO: Replace with proper JSON library (nlohmann/json recommended)
    
    // Remove whitespace
    std::string trimmed = json;
    trimmed.erase(std::remove_if(trimmed.begin(), trimmed.end(), ::isspace), trimmed.end());
    
    // For now, just set some defaults to make the system runnable
    // This is NOT a real parser
    
    // Example defaults
    set_string("core.log_level", "INFO");
    set_string("core.plugin_dir", "./plugins");
    set_int("network.port", 8888);
    set_bool("network.enable_discovery", true);
    
    return true;
}

} // namespace core
} // namespace sfs
