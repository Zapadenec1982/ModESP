/**
 * @file driver_init.cpp
 * @brief Explicit initialization of all sensor drivers
 * 
 * This file contains explicit registration of all built-in sensor drivers
 * to prevent linker from optimizing away static registration code.
 */

#include "sensor_driver_registry.h"
#include "ntc_driver.h"
#include "ds18b20_driver.h"
#include <esp_log.h>

static const char* TAG = "DriverInit";

void SensorDriverRegistry::initialize_builtin_drivers() {
    ESP_LOGI(TAG, "Initializing built-in sensor drivers...");
    
    auto& registry = instance();
    
    // Register NTC driver
    registry.register_driver("NTC", []() -> std::unique_ptr<ISensorDriver> {
        return std::make_unique<NTCDriver>();
    });
    
    // Register DS18B20 driver  
    registry.register_driver("DS18B20", []() -> std::unique_ptr<ISensorDriver> {
        return std::make_unique<DS18B20Driver>();
    });
    
    // Get registered types using public method
    auto registered_types = registry.get_registered_types();
    ESP_LOGI(TAG, "Built-in driver registration complete. Total drivers: %zu", 
             registered_types.size());
    
    // Log all registered drivers
    for (const auto& type : registered_types) {
        ESP_LOGI(TAG, "  - %s", type.c_str());
    }
} 