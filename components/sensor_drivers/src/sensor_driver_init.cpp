/**
 * @file sensor_driver_init.cpp
 * @brief Implementation of built-in sensor driver initialization
 * 
 * Drivers are conditionally compiled based on Kconfig settings
 * to save flash space when drivers are not needed.
 */

#include "sensor_driver_init.h"
#include "sensor_driver_registry.h"
#include <esp_log.h>

// Conditionally include driver headers
#ifdef CONFIG_SENSOR_DRIVER_NTC_ENABLED
#include "ntc_driver.h"
#endif

#ifdef CONFIG_SENSOR_DRIVER_DS18B20_ASYNC_ENABLED
#include "ds18b20_async_driver.h"
#endif

static const char* TAG = "SensorDriverInit";

void initialize_builtin_sensor_drivers() {
    ESP_LOGI(TAG, "Initializing built-in sensor drivers...");
    
    auto& registry = SensorDriverRegistry::instance();
    
#ifdef CONFIG_SENSOR_DRIVER_NTC_ENABLED
    // Register NTC driver
    registry.register_driver("NTC", []() -> std::unique_ptr<ISensorDriver> {
        return std::make_unique<NTCDriver>();
    });
    ESP_LOGI(TAG, "Registered NTC driver");
#endif

#ifdef CONFIG_SENSOR_DRIVER_DS18B20_ASYNC_ENABLED
    // Register DS18B20 Async driver
    registry.register_driver("DS18B20_Async", []() -> std::unique_ptr<ISensorDriver> {
        return std::make_unique<DS18B20AsyncDriver>();
    });
    ESP_LOGI(TAG, "Registered DS18B20_Async driver");
#endif
    
    // Get registered types using public method
    auto registered_types = registry.get_registered_types();
    ESP_LOGI(TAG, "Built-in driver registration complete. Total drivers: %zu", 
             registered_types.size());
    
    // Log all registered drivers
    for (const auto& type : registered_types) {
        ESP_LOGI(TAG, "  - %s", type.c_str());
    }
}
