/**
 * @file dynamic_api_builder.h
 * @brief Dynamic API Builder for runtime API registration
 * 
 * Part of TODO-006: Hybrid API Contract Implementation
 * Handles 20% of dynamic APIs that are generated at boot time based on configuration
 */

#pragma once

#include "esp_err.h"
#include "nlohmann/json.hpp"
#include "json_rpc_interface.h"
#include <string>

// Forward declarations
class ApiDispatcher;
class SensorDriverRegistry;

/**
 * @brief Dynamic API Builder - manages runtime API registration
 * 
 * This class generates and registers APIs based on the current system configuration.
 * It reads configuration files and creates driver-specific and scenario-specific APIs.
 */
class DynamicApiBuilder {
public:
    /**
     * @brief Constructor
     * 
     * @param dispatcher The API dispatcher to register methods with
     */
    explicit DynamicApiBuilder(ApiDispatcher* dispatcher);
    
    /**
     * @brief Build all dynamic APIs based on current configuration
     * 
     * This is the main entry point that reads configuration and generates
     * all dynamic APIs. Called once during system initialization.
     * 
     * @return ESP_OK on success
     */
    esp_err_t build_all_dynamic_apis();
    
    /**
     * @brief Rebuild dynamic APIs (e.g., after configuration change)
     * 
     * Clears all dynamic APIs and rebuilds them from current configuration.
     * 
     * @return ESP_OK on success
     */
    esp_err_t rebuild_dynamic_apis();

private:
    ApiDispatcher* dispatcher_;
    
    // Configuration-driven API builders
    esp_err_t build_sensor_apis();
    esp_err_t build_defrost_apis();
    esp_err_t build_scenario_apis();
    
    // Sensor driver-specific API generation
    esp_err_t generate_sensor_apis_from_schema(const std::string& role, 
                                              const std::string& type,
                                              const nlohmann::json& schema);
    
    // Driver-specific API registration
    void register_ds18b20_apis(const std::string& role);
    void register_ntc_apis(const std::string& role);
    void register_gpio_apis(const std::string& role);
    
    // Generic handler factories for driver properties
    JsonRpcHandler create_sensor_set_property_handler(const std::string& role, 
                                                     const std::string& property,
                                                     const nlohmann::json& definition);
    JsonRpcHandler create_sensor_get_property_handler(const std::string& role, 
                                                     const std::string& property);
    JsonRpcHandler create_sensor_get_value_handler(const std::string& role);
    JsonRpcHandler create_sensor_get_diagnostics_handler(const std::string& role);
    JsonRpcHandler create_sensor_calibrate_handler(const std::string& role);
    
    // Configuration loaders
    nlohmann::json load_sensors_config();
    nlohmann::json load_defrost_config();
    nlohmann::json load_scenario_config();
    
    // Utility methods
    bool is_sensor_type_available(const std::string& type);
    std::string get_method_name(const std::string& role, const std::string& action);
    esp_err_t validate_property_value(const nlohmann::json& value, 
                                     const nlohmann::json& definition);
    
    // Tag for logging
    static constexpr const char* TAG = "DynamicApiBuilder";
};

