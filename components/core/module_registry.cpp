/**
 * @file module_registry.cpp
 * @brief Module registration for ModuChill system
 * 
 * This file contains the registration of all available modules.
 * Add new modules here to make them available to the system.
 */

#include "module_manager.h"
#include "wifi_manager.h"
#include "rtc_module.h"
#include "sensor_module.h"
// #include "actuator_module.h"
#include "application.h"
#include <esp_log.h>
#include <memory>

static const char* TAG = "ModuleRegistry";

namespace ModuleRegistry {

/**
 * @brief Register all available modules with the ModuleManager
 * 
 * This function should be called during system initialization to register
 * all modules that should be available in the system.
 */
esp_err_t register_all_modules() {
    ESP_LOGI(TAG, "Registering all modules...");
    
    esp_err_t ret = ESP_OK;
    
    // Register WiFiManager as a HIGH priority module
    // HIGH priority modules handle real-time I/O operations like network connectivity
    auto wifi_manager = std::make_unique<WiFiManager>();
    ret = ModuleManager::register_module(std::move(wifi_manager), ModuleType::HIGH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFiManager: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register SensorModule (requires ESPhal)
    auto sensor_module = std::make_unique<SensorModule>(Application::get_hal());
    ret = ModuleManager::register_module(std::move(sensor_module), ModuleType::HIGH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register SensorModule: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register ActuatorModule (requires ESPhal)
    // auto actuator_module = std::make_unique<ActuatorModule>(Application::get_hal());
    // ret = ModuleManager::register_module(std::move(actuator_module), ModuleType::HIGH);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to register ActuatorModule: %s", esp_err_to_name(ret));
    //     return ret;
    // }
    
    // Register RTCModule (simple version without ESPhal)
    auto rtc_module = std::make_unique<RTCModule>();
    ret = ModuleManager::register_module(std::move(rtc_module), ModuleType::BACKGROUND);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register RTCModule: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Module registration completed");
    return ESP_OK;
}

} // namespace ModuleRegistry 