#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <esp_err.h>

namespace ModuChill {

using json = nlohmann::json;

/**
 * Configuration validation result
 */
struct ConfigValidation {
    bool valid;
    std::string error_message;
    std::vector<std::string> missing_fields;
    std::vector<std::string> invalid_fields;
};

/**
 * Config - JSON-based configuration with NVS persistence
 * 
 * Manages persistent configuration with validation, versioning and migration.
 * Separates configuration from code, allowing runtime behavior changes.
 */
class Config {
private:
    // Current configuration
    json config_;
    json defaults_;
    
    // State tracking
    bool is_dirty_;
    uint32_t version_;
    
    // NVS namespace
    static constexpr const char* NVS_NAMESPACE = "moduchill_cfg";
    static constexpr const char* NVS_KEY = "config";
    static constexpr const char* NVS_VERSION_KEY = "version";
    
    // Configuration limits
    static constexpr size_t MAX_CONFIG_SIZE = 4096;
    
    // Singleton
    static Config* instance_;
    
    Config();
    ~Config() = default;
    
    // Internal methods
    esp_err_t load_from_nvs();
    esp_err_t save_to_nvs();
    void load_defaults();
    bool migrate_config(uint32_t from_version, uint32_t to_version);
    
public:
    // Lifecycle
    static void init();
    static esp_err_t load();
    static esp_err_t save();
    static void reset_to_defaults();
    
    // Access methods
    static json get_all();
    static json get_section(const std::string& path);
    
    template<typename T>
    static T get(const std::string& path, const T& default_value);
    
    template<typename T>
    static void set(const std::string& path, const T& value);
    
    // State management
    static bool is_dirty() { return instance_->is_dirty_; }
    static void discard_changes();
    
    // Validation
    static ConfigValidation validate();
    static ConfigValidation validate_section(const std::string& path, const json& schema);
    
    // Import/Export
    static std::string export_json();
    static esp_err_t import_json(const std::string& json_str);
    
    // Version
    static uint32_t get_version() { return instance_->version_; }
    
private:
    static Config* get_instance();
    static json get_value_at_path(const json& obj, const std::string& path);
    static void set_value_at_path(json& obj, const std::string& path, const json& value);
};

// Template implementations
template<typename T>
T Config::get(const std::string& path, const T& default_value) {
    auto* instance = get_instance();
    if (!instance) return default_value;
    
    try {
        json value = get_value_at_path(instance->config_, path);
        return value.get<T>();
    } catch (...) {
        return default_value;
    }
}

template<typename T>
void Config::set(const std::string& path, const T& value) {
    auto* instance = get_instance();
    if (!instance) return;
    
    set_value_at_path(instance->config_, path, value);
    instance->is_dirty_ = true;
}

} // namespace ModuChill