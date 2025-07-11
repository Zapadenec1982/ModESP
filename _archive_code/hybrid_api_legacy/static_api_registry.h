/**
 * @file static_api_registry.h
 * @brief Static API Registry for compile-time API registration
 * 
 * Part of TODO-006: Hybrid API Contract Implementation
 * Handles 80% of static APIs that are generated at compile time from manifests
 */

#pragma once

#include "esp_err.h"
#include "nlohmann/json.hpp"
#include "json_rpc_interface.h"
#include <vector>
#include <string>

// Forward declaration
class ApiDispatcher;

/**
 * @brief Information about a registered RPC method
 */
struct RpcMethodInfo {
    std::string method;             // Method name (e.g., "system.get_status")
    std::string description;        // Human-readable description
    JsonRpcHandler handler;         // Method implementation
    nlohmann::json schema;          // Optional validation schema
    bool is_static;                 // true for static (compile-time), false for dynamic
    
    RpcMethodInfo(const std::string& m, const std::string& desc, 
                  JsonRpcHandler h, bool static_method = true)
        : method(m), description(desc), handler(h), is_static(static_method) {}
        
    RpcMethodInfo(const std::string& m, const std::string& desc, 
                  JsonRpcHandler h, const nlohmann::json& s, bool static_method = true)
        : method(m), description(desc), handler(h), schema(s), is_static(static_method) {}
};

/**
 * @brief Static API Registry - manages compile-time API registration
 * 
 * This class registers all static APIs that are available regardless of
 * runtime configuration. These APIs are determined at compile time from
 * module manifests and core system capabilities.
 */
class StaticApiRegistry {
public:
    /**
     * @brief Register all static APIs with the dispatcher
     * 
     * This is the main entry point that registers all static APIs.
     * Called once during system initialization.
     * 
     * @param dispatcher The API dispatcher to register methods with
     * @return ESP_OK on success
     */
    static esp_err_t register_all_static_apis(ApiDispatcher* dispatcher);
    
    /**
     * @brief Get list of all static API methods
     * 
     * @return Vector of method information for all static APIs
     */
    static std::vector<RpcMethodInfo> get_all_static_methods();
    
private:
    // System APIs - always available
    static esp_err_t register_system_apis(ApiDispatcher* dispatcher);
    
    // Sensor base APIs - available if sensor module is compiled
    static esp_err_t register_sensor_base_apis(ApiDispatcher* dispatcher);
    
    // Actuator base APIs - available if actuator module is compiled
    static esp_err_t register_actuator_base_apis(ApiDispatcher* dispatcher);
    
    // Climate control APIs - available if climate module is compiled
    static esp_err_t register_climate_apis(ApiDispatcher* dispatcher);
    
    // Network APIs - WiFi and connectivity
    static esp_err_t register_network_apis(ApiDispatcher* dispatcher);
    
    // Configuration APIs - for system configuration management
    static esp_err_t register_configuration_apis(ApiDispatcher* dispatcher);
    
    // Helper methods
    static JsonRpcHandler create_system_status_handler();
    static JsonRpcHandler create_system_uptime_handler();
    static JsonRpcHandler create_system_restart_handler();
    static JsonRpcHandler create_wifi_status_handler();
    static JsonRpcHandler create_sensor_get_all_handler();
    static JsonRpcHandler create_climate_get_setpoint_handler();
    static JsonRpcHandler create_config_get_sensors_handler();
    static JsonRpcHandler create_config_get_available_types_handler();
    
    // Tag for logging
    static constexpr const char* TAG = "StaticApiRegistry";
};

