/**
 * @file configuration_manager.cpp
 * @brief Implementation of Configuration Manager
 * 
 * Part of TODO-006: Hybrid API Contract Implementation
 */

#include "configuration_manager.h"
#include "event_bus.h"
#include "system_contract.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

using namespace ModespContract;

constexpr const char* ConfigurationManager::TAG;

ConfigurationManager& ConfigurationManager::instance() {
    static ConfigurationManager instance;
    return instance;
}

esp_err_t ConfigurationManager::initialize() {
    ESP_LOGI(TAG, "Initializing Configuration Manager...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Check if restart was required in previous session
    // This helps track if configuration changes were applied
    nvs_handle_t nvs_handle;
    ret = nvs_open("config_mgr", NVS_READONLY, &nvs_handle);
    if (ret == ESP_OK) {
        size_t required_size = 0;
        ret = nvs_get_str(nvs_handle, "restart_reason", NULL, &required_size);
        if (ret == ESP_OK && required_size > 0) {
            ESP_LOGI(TAG, "Previous restart was for configuration changes");
            // Clear the flag since restart was completed
            clear_restart_required();
        }
        nvs_close(nvs_handle);
    }
    
    ESP_LOGI(TAG, "Configuration Manager initialized");
    return ESP_OK;
}

esp_err_t ConfigurationManager::update_sensor_configuration(const nlohmann::json& config) {
    ESP_LOGI(TAG, "Updating sensor configuration...");
    
    // Validate configuration
    esp_err_t ret = validate_sensor_config(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Invalid sensor configuration");
        return ret;
    }
    
    // Save configuration and mark restart required
    return save_config_and_mark_restart("sensors", config, "sensor_configuration_changed");
}

esp_err_t ConfigurationManager::update_defrost_configuration(const nlohmann::json& config) {
    ESP_LOGI(TAG, "Updating defrost configuration...");
    
    // Validate configuration
    esp_err_t ret = validate_defrost_config(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Invalid defrost configuration");
        return ret;
    }
    
    // Save configuration and mark restart required
    return save_config_and_mark_restart("defrost", config, "defrost_configuration_changed");
}

esp_err_t ConfigurationManager::update_scenario_configuration(const nlohmann::json& config) {
    ESP_LOGI(TAG, "Updating scenario configuration...");
    
    // Validate configuration
    esp_err_t ret = validate_scenario_config(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Invalid scenario configuration");
        return ret;
    }
    
    // Save configuration and mark restart required
    return save_config_and_mark_restart("scenarios", config, "scenario_configuration_changed");
}

bool ConfigurationManager::is_restart_required() {
    return restart_required_;
}

std::string ConfigurationManager::get_restart_reason() {
    return restart_reason_;
}

void ConfigurationManager::schedule_restart_if_required() {
    if (!restart_required_) {
        ESP_LOGW(TAG, "Restart not required, ignoring schedule request");
        return;
    }
    
    ESP_LOGI(TAG, "Scheduling restart for reason: %s", restart_reason_.c_str());
    
    // Publish event to notify all clients about upcoming restart
    EventBus::publish(Event::SystemShutdown, {
        {"reason", restart_reason_},
        {"countdown_seconds", 5},
        {"type", "configuration_restart"}
    });
    
    // Schedule delayed restart
    schedule_delayed_restart(5000); // 5 seconds delay
}

void ConfigurationManager::mark_restart_required(const std::string& reason) {
    restart_required_ = true;
    restart_reason_ = reason;
    
    ESP_LOGI(TAG, "Restart marked as required: %s", reason.c_str());
    
    // Publish event to notify about restart requirement
    EventBus::publish("config.restart_required", {
        {"reason", reason},
        {"restart_required", true}
    });
}

void ConfigurationManager::clear_restart_required() {
    if (restart_required_) {
        ESP_LOGI(TAG, "Clearing restart requirement");
        restart_required_ = false;
        restart_reason_.clear();
        
        // Clear from NVS as well
        nvs_handle_t nvs_handle;
        if (nvs_open("config_mgr", NVS_READWRITE, &nvs_handle) == ESP_OK) {
            nvs_erase_key(nvs_handle, "restart_reason");
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }
    }
}

// Validation method implementations
esp_err_t ConfigurationManager::validate_sensor_config(const nlohmann::json& config) {
    // Basic validation for sensor configuration
    if (!config.is_object()) {
        ESP_LOGE(TAG, "Sensor config must be an object");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!config.contains("sensors") || !config["sensors"].is_array()) {
        ESP_LOGE(TAG, "Sensor config must contain 'sensors' array");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate each sensor
    for (const auto& sensor : config["sensors"]) {
        if (!sensor.contains("role") || !sensor["role"].is_string()) {
            ESP_LOGE(TAG, "Each sensor must have a 'role' string");
            return ESP_ERR_INVALID_ARG;
        }
        
        if (!sensor.contains("type") || !sensor["type"].is_string()) {
            ESP_LOGE(TAG, "Each sensor must have a 'type' string");
            return ESP_ERR_INVALID_ARG;
        }
        
        // TODO: Validate that sensor type is available
    }
    
    return ESP_OK;
}

esp_err_t ConfigurationManager::validate_defrost_config(const nlohmann::json& config) {
    // Basic validation for defrost configuration
    if (!config.is_object()) {
        ESP_LOGE(TAG, "Defrost config must be an object");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!config.contains("type") || !config["type"].is_string()) {
        ESP_LOGE(TAG, "Defrost config must contain 'type' string");
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: Validate specific defrost type parameters
    
    return ESP_OK;
}

esp_err_t ConfigurationManager::validate_scenario_config(const nlohmann::json& config) {
    // Basic validation for scenario configuration
    if (!config.is_object()) {
        ESP_LOGE(TAG, "Scenario config must be an object");
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: Implement scenario-specific validation
    
    return ESP_OK;
}

// NVS persistence methods
esp_err_t ConfigurationManager::save_config_to_nvs(const std::string& key, const nlohmann::json& config) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    std::string config_str = config.dump();
    ret = nvs_set_str(nvs_handle, key.c_str(), config_str.c_str());
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config to NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration saved to NVS: %s", key.c_str());
    }
    
    return ret;
}

esp_err_t ConfigurationManager::load_config_from_nvs(const std::string& key, nlohmann::json& config) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("config", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    size_t required_size = 0;
    ret = nvs_get_str(nvs_handle, key.c_str(), NULL, &required_size);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    
    std::string config_str(required_size, '\0');
    ret = nvs_get_str(nvs_handle, key.c_str(), &config_str[0], &required_size);
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        try {
            config = nlohmann::json::parse(config_str);
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "Failed to parse config JSON: %s", e.what());
            return ESP_ERR_INVALID_ARG;
        }
    }
    
    return ret;
}

esp_err_t ConfigurationManager::save_config_and_mark_restart(const std::string& key, 
                                                            const nlohmann::json& config,
                                                            const std::string& reason) {
    // Save configuration
    esp_err_t ret = save_config_to_nvs(key, config);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Save restart reason to NVS
    nvs_handle_t nvs_handle;
    ret = nvs_open("config_mgr", NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_set_str(nvs_handle, "restart_reason", reason.c_str());
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    
    // Mark restart as required
    mark_restart_required(reason);
    
    ESP_LOGI(TAG, "Configuration saved and restart marked: %s", reason.c_str());
    return ESP_OK;
}

void ConfigurationManager::schedule_delayed_restart(uint32_t delay_ms) {
    ESP_LOGI(TAG, "Scheduling system restart in %lu ms", delay_ms);
    
    esp_timer_handle_t restart_timer;
    esp_timer_create_args_t timer_args = {
        .callback = restart_timer_callback,
        .arg = nullptr,
        .name = "config_restart_timer"
    };
    
    esp_err_t ret = esp_timer_create(&timer_args, &restart_timer);
    if (ret == ESP_OK) {
        esp_timer_start_once(restart_timer, delay_ms * 1000); // Convert ms to us
    } else {
        ESP_LOGE(TAG, "Failed to create restart timer: %s", esp_err_to_name(ret));
    }
}

void ConfigurationManager::restart_timer_callback(void* arg) {
    ESP_LOGI(TAG, "Restart timer expired, restarting system...");
    esp_restart();
}
