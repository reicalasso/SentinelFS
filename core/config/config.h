#ifndef SFS_CONFIG_H
#define SFS_CONFIG_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <variant>

namespace sfs {
namespace core {

/**
 * @brief Configuration value types
 */
using ConfigValue = std::variant<
    std::string,
    int64_t,
    double,
    bool,
    std::vector<std::string>
>;

/**
 * @brief Config - JSON configuration loader
 * 
 * Simple JSON parser for loading configuration files.
 * Supports nested objects and basic types.
 */
class Config {
public:
    Config();
    ~Config();
    
    /**
     * @brief Load configuration from JSON file
     * 
     * @param path Path to JSON file
     * @return true if loaded successfully
     */
    bool load_from_file(const std::string& path);
    
    /**
     * @brief Load configuration from JSON string
     * 
     * @param json JSON string
     * @return true if parsed successfully
     */
    bool load_from_string(const std::string& json);
    
    /**
     * @brief Get string value
     * 
     * @param key Key path (e.g., "network.port")
     * @param default_value Default value if key not found
     * @return Value or default
     */
    std::string get_string(const std::string& key, const std::string& default_value = "") const;
    
    /**
     * @brief Get integer value
     */
    int64_t get_int(const std::string& key, int64_t default_value = 0) const;
    
    /**
     * @brief Get double value
     */
    double get_double(const std::string& key, double default_value = 0.0) const;
    
    /**
     * @brief Get boolean value
     */
    bool get_bool(const std::string& key, bool default_value = false) const;
    
    /**
     * @brief Get string array
     */
    std::vector<std::string> get_string_array(const std::string& key) const;
    
    /**
     * @brief Check if key exists
     */
    bool has_key(const std::string& key) const;
    
    /**
     * @brief Set a value (for programmatic configuration)
     */
    void set_string(const std::string& key, const std::string& value);
    void set_int(const std::string& key, int64_t value);
    void set_double(const std::string& key, double value);
    void set_bool(const std::string& key, bool value);
    
    /**
     * @brief Clear all configuration
     */
    void clear();

private:
    std::map<std::string, ConfigValue> data_;
    
    /**
     * @brief Parse JSON (simple implementation)
     * 
     * This is a minimal JSON parser. For production, consider using
     * a library like nlohmann/json or RapidJSON.
     */
    bool parse_json(const std::string& json);
    
    /**
     * @brief Get value by key path
     */
    const ConfigValue* get_value(const std::string& key) const;
};

} // namespace core
} // namespace sfs

#endif // SFS_CONFIG_H
