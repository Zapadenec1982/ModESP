/**
 * @file module_registry.cpp
 * @brief Central registry for all system modules
 * 
 * This file manually registers all available modules with ModuleManager.
 * TODO: Replace with auto-generation from module descriptors
 */

#include "module_manager.h"
// #include "sensor_module.h"  // TODO: Fix module paths
// #include "actuator_module.h" // TODO: Fix module paths
#include "logger_module.h"
#include <esp_log.h>
#include <memory>

static const char* TAG = "ModuleRegistry";

namespace ModuleRegistry {

esp_err_t register_all_modules() {
    ESP_LOGI(TAG, "Registering all system modules...");
    
    esp_err_t ret;
    
    // Register Logger Module (CRITICAL priority)
    {
        auto logger_module = std::make_unique<ModESP::LoggerModule>();
        ret = ModuleManager::register_module(std::move(logger_module), ModuleType::CRITICAL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register Logger: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "✅ Logger registered (CRITICAL)");
    }
    
    // TODO: Register Sensor Module (HIGH priority) - disabled until module paths fixed
    /*
    {
        extern ESPhal& hal; // From application.cpp
        auto sensor_module = std::make_unique<SensorModule>(hal);
        ret = ModuleManager::register_module(std::move(sensor_module), ModuleType::HIGH);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register SensorModule: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "✅ SensorModule registered (HIGH)");
    }
    */
    
    // TODO: Register Actuator Module (STANDARD priority) - disabled until module paths fixed
    /*
    {
        extern ESPhal& hal; // From application.cpp
        auto actuator_module = std::make_unique<ActuatorModule>(hal);
        ret = ModuleManager::register_module(std::move(actuator_module), ModuleType::STANDARD);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register ActuatorModule: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "✅ ActuatorModule registered (STANDARD)");
    }
    */
    
    ESP_LOGI(TAG, "All modules registered successfully");
    return ESP_OK;
}

} // namespace ModuleRegistry 