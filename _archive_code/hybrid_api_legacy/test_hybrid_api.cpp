/**
 * @file test_hybrid_api.cpp
 * @brief Simple test for Hybrid API System
 * 
 * Part of TODO-006: Hybrid API Contract Implementation
 * Phase 5: Integration and Testing
 */

#include "test_hybrid_api.h"
#include "api_dispatcher.h"
#include "static_api_registry.h"
#include "dynamic_api_builder.h"
#include "configuration_manager.h"
#include "esp_log.h"
#include "nlohmann/json.hpp"

static const char* TAG = "TestHybridAPI";

extern "C" {

/**
 * @brief Test function for hybrid API system
 * 
 * This function tests the basic functionality of the hybrid API system
 * without requiring full system initialization.
 */
void test_hybrid_api_system() {
    ESP_LOGI(TAG, "=== Testing Hybrid API System ===");
    
    // Create API dispatcher
    ApiDispatcher dispatcher;
    
    // Test static API registration
    ESP_LOGI(TAG, "Testing static API registration...");
    esp_err_t ret = StaticApiRegistry::register_all_static_apis(&dispatcher);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Static APIs registered successfully");
    } else {
        ESP_LOGE(TAG, "❌ Failed to register static APIs: %s", esp_err_to_name(ret));
        return;
    }
    
    // Test dynamic API building
    ESP_LOGI(TAG, "Testing dynamic API building...");
    DynamicApiBuilder builder(&dispatcher);
    ret = builder.build_all_dynamic_apis();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Dynamic APIs built successfully");
    } else {
        ESP_LOGE(TAG, "❌ Failed to build dynamic APIs: %s", esp_err_to_name(ret));
        return;
    }
    
    // Test configuration manager
    ESP_LOGI(TAG, "Testing configuration manager...");
    ret = ConfigurationManager::instance().initialize();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Configuration Manager initialized successfully");
    } else {
        ESP_LOGE(TAG, "❌ Failed to initialize Configuration Manager: %s", esp_err_to_name(ret));
        return;
    }
    
    // Test API method listing
    ESP_LOGI(TAG, "Testing API method listing...");
    auto methods = dispatcher.get_available_methods_by_category();
    ESP_LOGI(TAG, "API Methods Summary:");
    ESP_LOGI(TAG, "  Static methods: %d", methods["static_methods"].size());
    ESP_LOGI(TAG, "  Dynamic methods: %d", methods["dynamic_methods"].size());
    
    // Test sample API call
    ESP_LOGI(TAG, "Testing sample API call...");
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"method", "system.get_status"},
        {"id", 1}
    };
    
    nlohmann::json response;
    ret = dispatcher.execute_json_rpc(request, response);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Sample API call successful");
        ESP_LOGI(TAG, "Response: %s", response.dump().c_str());
    } else {
        ESP_LOGE(TAG, "❌ Sample API call failed: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "=== Hybrid API System Test Complete ===");
}

/**
 * @brief Test configuration change workflow
 */
void test_configuration_workflow() {
    ESP_LOGI(TAG, "=== Testing Configuration Workflow ===");
    
    auto& config_mgr = ConfigurationManager::instance();
    
    // Test sensor configuration update
    nlohmann::json sensor_config = {
        {"sensors", nlohmann::json::array({
            {
                {"role", "temperature_1"},
                {"type", "DS18B20_Async"},
                {"config", {{"resolution", 12}}}
            }
        })}
    };
    
    ESP_LOGI(TAG, "Testing sensor configuration update...");
    esp_err_t ret = config_mgr.update_sensor_configuration(sensor_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Sensor configuration updated");
        ESP_LOGI(TAG, "Restart required: %s", config_mgr.is_restart_required() ? "YES" : "NO");
        ESP_LOGI(TAG, "Restart reason: %s", config_mgr.get_restart_reason().c_str());
    } else {
        ESP_LOGE(TAG, "❌ Failed to update sensor configuration: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "=== Configuration Workflow Test Complete ===");
}

} // extern "C"
