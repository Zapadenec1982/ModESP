#include "config.h"
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>

namespace ModuChill {

static const char* TAG = "Config";
static const char* NVS_NAMESPACE = "moduchill";
static const char* CONFIG_KEY = "config_json";

Config* Config::instance_ = nullptr;

Config::Config() 
    : version_(1)
    , is_dirty_(false) {
    // Set default configuration
    data_ = {
        {"version", version_},
        {"system", {
            {"name", "ModuChill"},
            {"log_level", "INFO"}
        }},
        {"climate", {
            {"setpoint", 4.0},
            {"hysteresis", 0.5},
            {"min_temp", -10.0},
            {"max_temp", 10.0}
        }},
        {"compressor", {
            {"min_off_time", 180},
            {"min_on_time", 120},
            {"start_delay", 5}
        }},
        {"defrost", {
            {"enabled", true},
            {"interval", 21600},
            {"max_duration", 1800},
            {"temp_threshold", -5.0}
        }},
        {"network", {
            {"wifi", {
                {"ssid", ""},
                {"password", ""},
                {"enabled", false}
            }},
            {"mqtt", {
                {"broker", ""},
                {"port", 1883},
                {"enabled", false}
            }}
        }}
    };
}

Config* Config::get_instance() {
    if (!instance_) {
        instance_ = new Config();
    }
    return instance_;
}

void Config::init() {
    ESP_LOGI(TAG, "Config Manager initialized");
}

esp_err_t Config::load() {
    auto* config = get_instance();
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, using defaults");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Get JSON string size
    size_t length = 0;
    err = nvs_get_str(handle, CONFIG_KEY, nullptr, &length);
    
    if (err == ESP_OK && length > 0) {
        // Allocate buffer and read
        std::string json_str(length - 1, '\0');
        err = nvs_get_str(handle, CONFIG_KEY, json_str.data(), &length);
        
        if (err == ESP_OK) {
            try {
                json loaded = json::parse(json_str);
                
                // Check version and migrate if needed
                int loaded_version = loaded.value("version", 0);
                if (loaded_version < config->version_) {
                    ESP_LOGI(TAG, "Migrating config from v%d to v%d", 
                             loaded_version, config->version_);
                    config->migrate(loaded, loaded_version);
                } else {
                    config->data_ = loaded;
                }
                
                ESP_LOGI(TAG, "Configuration loaded from NVS");
            } catch (const json::exception& e) {
                ESP_LOGE(TAG, "Failed to parse config JSON: %s", e.what());
                err = ESP_ERR_INVALID_ARG;
            }
        }
    } else {
        ESP_LOGW(TAG, "No config in NVS, using defaults");
        err = ESP_ERR_NOT_FOUND;
    }
    
    nvs_close(handle);
    
    config->is_dirty_ = false;
    return err;
}

esp_err_t Config::save() {
    auto* config = get_instance();
    
    if (!config->is_dirty_) {
        ESP_LOGD(TAG, "Config not changed, skipping save");
        return ESP_OK;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    // Serialize to JSON
    std::string json_str = config->data_.dump();
    
    // Save to NVS
    err = nvs_set_str(handle, CONFIG_KEY, json_str.c_str());
    
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Configuration saved to NVS (%zu bytes)", json_str.length());
            config->is_dirty_ = false;
        }
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(err));
    }
    
    nvs_close(handle);
    return err;
}
json Config::get_all() {
    auto* config = get_instance();
    return config->data_;
}

json Config::get_section(const std::string& path) {
    auto* config = get_instance();
    
    // Navigate through nested objects
    std::istringstream iss(path);
    std::string token;
    json* current = &config->data_;
    
    while (std::getline(iss, token, '.')) {
        if (current->contains(token)) {
            current = &(*current)[token];
        } else {
            return {};
        }
    }
    
    return *current;
}

bool Config::has(const std::string& path) {
    auto* config = get_instance();
    
    std::istringstream iss(path);
    std::string token;
    json* current = &config->data_;
    
    while (std::getline(iss, token, '.')) {
        if (current->contains(token)) {
            current = &(*current)[token];
        } else {
            return false;
        }
    }
    
    return true;
}

void Config::set_value(const std::string& path, const json& value) {
    auto* config = get_instance();
    
    // Navigate and create nested objects if needed
    std::istringstream iss(path);
    std::vector<std::string> tokens;
    std::string token;
    
    while (std::getline(iss, token, '.')) {
        tokens.push_back(token);
    }
    
    if (tokens.empty()) return;
    
    json* current = &config->data_;
    
    // Navigate to parent
    for (size_t i = 0; i < tokens.size() - 1; ++i) {
        if (!current->contains(tokens[i])) {
            (*current)[tokens[i]] = json::object();
        }
        current = &(*current)[tokens[i]];
    }
    
    // Set value
    (*current)[tokens.back()] = value;
    config->is_dirty_ = true;
    
    ESP_LOGD(TAG, "Set %s = %s", path.c_str(), value.dump().c_str());
}

void Config::reset_to_defaults() {
    auto* config = get_instance();
    
    ESP_LOGI(TAG, "Resetting configuration to defaults");
    
    // Re-create default config
    Config temp_config;
    config->data_ = temp_config.data_;
    config->is_dirty_ = true;
    
    // Save immediately
    save();
}

bool Config::is_dirty() {
    return get_instance()->is_dirty_;
}

void Config::discard_changes() {
    auto* config = get_instance();
    
    if (config->is_dirty_) {
        ESP_LOGI(TAG, "Discarding unsaved changes");
        load(); // Reload from NVS
    }
}

void Config::migrate(json& data, int from_version) {
    // Example migration logic
    if (from_version < 1) {
        // Add new fields introduced in v1
        if (!data.contains("defrost")) {
            data["defrost"] = {
                {"enabled", true},
                {"interval", 21600},
                {"max_duration", 1800},
                {"temp_threshold", -5.0}
            };
        }
    }
    
    // Update version
    data["version"] = version_;
}

} // namespace ModuChill