/**
 * @file configuration_manager.h
 * @brief Configuration Manager with restart pattern
 * 
 * Part of TODO-006: Hybrid API Contract Implementation
 * Manages configuration changes that require system restart to apply
 */

#pragma once

#include "esp_err.h"
#include "nlohmann/json.hpp"
#include <string>

/**
 * @brief Configuration Manager with restart pattern
 * 
 * Handles configuration changes that require system restart to apply properly.
 * This is used for changes that affect which APIs are available (e.g., sensor types).
 */
class ConfigurationManager {
public:
    /**
     * @brief Get singleton instance
     */
    static ConfigurationManager& instance();
    
    /**
     * @brief Initialize configuration manager
     * 
     * @return ESP_OK on success
     */
    esp_err_t initialize();
    
    /**
     * @brief Update sensor configuration (requires restart)
     * 
     * @param config New sensor configuration
     * @return ESP_OK on success
     */
    esp_err_t update_sensor_configuration(const nlohmann::json& config);
    
    /**
     * @brief Update defrost configuration (requires restart)
     * 
     * @param config New defrost configuration
     * @return ESP_OK on success
     */
    esp_err_t update_defrost_configuration(const nlohmann::json& config);
    
    /**
     * @brief Update scenario configuration (requires restart)
     * 
     * @param config New scenario configuration
     * @return ESP_OK on success
     */
    esp_err_t update_scenario_configuration(const nlohmann::json& config);
    
    /**
     * @brief Check if restart is required to apply configuration changes
     * 
     * @return true if restart is required
     */
    bool is_restart_required();
    
    /**
     * @brief Get reason why restart is required
     * 
     * @return Description of why restart is needed
     */
    std::string get_restart_reason();
    
    /**
     * @brief Schedule restart if required
     * 
     * Initiates graceful restart after a short delay if restart is required.
     * Publishes events to notify all clients.
     */
    void schedule_restart_if_required();
    
    /**
     * @brief Mark that restart is required
     * 
     * @param reason Reason why restart is needed
     */
    void mark_restart_required(const std::string& reason);
    
    /**
     * @brief Clear restart requirement flag
     * 
     * Called after successful restart to clear the flag.
     */
    void clear_restart_required();

private:
    ConfigurationManager() = default;
    
    bool restart_required_ = false;
    std::string restart_reason_;
    
    // Configuration validation
    esp_err_t validate_sensor_config(const nlohmann::json& config);
    esp_err_t validate_defrost_config(const nlohmann::json& config);
    esp_err_t validate_scenario_config(const nlohmann::json& config);
    
    // Configuration persistence
    esp_err_t save_config_to_nvs(const std::string& key, const nlohmann::json& config);
    esp_err_t load_config_from_nvs(const std::string& key, nlohmann::json& config);
    
    // Restart management
    esp_err_t save_config_and_mark_restart(const std::string& key, 
                                          const nlohmann::json& config,
                                          const std::string& reason);
    void schedule_delayed_restart(uint32_t delay_ms);
    
    // Static callback for restart timer
    static void restart_timer_callback(void* arg);
    
    // Tag for logging
    static constexpr const char* TAG = "ConfigurationManager";
};

