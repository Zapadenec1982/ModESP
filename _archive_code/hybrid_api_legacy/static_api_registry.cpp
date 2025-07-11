/**
 * @file static_api_registry.cpp
 * @brief Implementation of Static API Registry
 * 
 * Part of TODO-006: Hybrid API Contract Implementation
 */

#include "static_api_registry.h"
#include "api_dispatcher.h"
#include "system_contract.h"
#include "shared_state.h"
#include "event_bus.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"

using namespace ModespContract;

constexpr const char* StaticApiRegistry::TAG;

esp_err_t StaticApiRegistry::register_all_static_apis(ApiDispatcher* dispatcher) {
    ESP_LOGI(TAG, "Registering all static APIs...");
    
    esp_err_t ret = ESP_OK;
    
    // Register system APIs (always available)
    ret = register_system_apis(dispatcher);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register system APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register sensor base APIs
    ret = register_sensor_base_apis(dispatcher);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register sensor APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register actuator base APIs
    ret = register_actuator_base_apis(dispatcher);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register actuator APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register climate control APIs
    ret = register_climate_apis(dispatcher);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register climate APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register network APIs
    ret = register_network_apis(dispatcher);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register network APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register configuration APIs
    ret = register_configuration_apis(dispatcher);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register configuration APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "All static APIs registered successfully");
    return ESP_OK;
}

esp_err_t StaticApiRegistry::register_system_apis(ApiDispatcher* dispatcher) {
    ESP_LOGI(TAG, "Registering system APIs...");
    
    // System status
    dispatcher->register_method("system.get_status", create_system_status_handler(),
                               "Get comprehensive system status information");
    
    // System uptime
    dispatcher->register_method("system.get_uptime", create_system_uptime_handler(),
                               "Get system uptime in seconds");
    
    // System restart
    dispatcher->register_method("system.restart", create_system_restart_handler(),
                               "Restart the system");
    
    // Memory information
    dispatcher->register_method("system.get_memory_info", 
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            result = {
                {"free_heap", esp_get_free_heap_size()},
                {"minimum_free_heap", esp_get_minimum_free_heap_size()},
                {"largest_free_block", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)}
            };
            return ESP_OK;
        }, "Get system memory information");
    
    ESP_LOGI(TAG, "System APIs registered");
    return ESP_OK;
}

esp_err_t StaticApiRegistry::register_sensor_base_apis(ApiDispatcher* dispatcher) {
    ESP_LOGI(TAG, "Registering sensor base APIs...");
    
    // Get all sensor readings
    dispatcher->register_method("sensor.get_all_readings", create_sensor_get_all_handler(),
                               "Get readings from all configured sensors");
    
    // Get temperature (generic)
    dispatcher->register_method("sensor.get_temperature",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            float temperature;
            if (SharedState::get(State::SensorTemperature, temperature)) {
                result = {
                    {"value", temperature},
                    {"unit", "celsius"},
                    {"timestamp", esp_timer_get_time()}
                };
                return ESP_OK;
            }
            return ESP_ERR_NOT_FOUND;
        }, "Get current temperature reading");
    
    // Get humidity (generic)
    dispatcher->register_method("sensor.get_humidity",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            float humidity;
            if (SharedState::get(State::SensorHumidity, humidity)) {
                result = {
                    {"value", humidity},
                    {"unit", "percent"},
                    {"timestamp", esp_timer_get_time()}
                };
                return ESP_OK;
            }
            return ESP_ERR_NOT_FOUND;
        }, "Get current humidity reading");
    
    ESP_LOGI(TAG, "Sensor base APIs registered");
    return ESP_OK;
}

esp_err_t StaticApiRegistry::register_actuator_base_apis(ApiDispatcher* dispatcher) {
    ESP_LOGI(TAG, "Registering actuator base APIs...");
    
    // Get compressor status
    dispatcher->register_method("actuator.get_compressor_status",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            nlohmann::json compressor_state;
            if (SharedState::get(State::ActuatorCompressor, compressor_state)) {
                result = compressor_state;
                result["timestamp"] = esp_timer_get_time();
                return ESP_OK;
            }
            return ESP_ERR_NOT_FOUND;
        }, "Get compressor status and runtime information");
    
    // Get all actuator states
    dispatcher->register_method("actuator.get_all_states",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            result = nlohmann::json::object();
            
            nlohmann::json state;
            if (SharedState::get(State::ActuatorCompressor, state)) {
                result["compressor"] = state;
            }
            if (SharedState::get(State::ActuatorEvaporatorFan, state)) {
                result["evaporator_fan"] = state;
            }
            if (SharedState::get(State::ActuatorDefrostHeater, state)) {
                result["defrost_heater"] = state;
            }
            if (SharedState::get(State::ActuatorLight, state)) {
                result["light"] = state;
            }
            
            result["timestamp"] = esp_timer_get_time();
            return ESP_OK;
        }, "Get status of all actuators");
    
    ESP_LOGI(TAG, "Actuator base APIs registered");
    return ESP_OK;
}

esp_err_t StaticApiRegistry::register_climate_apis(ApiDispatcher* dispatcher) {
    ESP_LOGI(TAG, "Registering climate control APIs...");
    
    // Get setpoint
    dispatcher->register_method("climate.get_setpoint", create_climate_get_setpoint_handler(),
                               "Get current temperature setpoint");
    
    // Set setpoint
    dispatcher->register_method("climate.set_setpoint",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            if (!params.contains("value") || !params["value"].is_number()) {
                return ESP_ERR_INVALID_ARG;
            }
            
            float setpoint = params["value"];
            if (setpoint < -40.0f || setpoint > 60.0f) {
                return ESP_ERR_INVALID_ARG;
            }
            
            esp_err_t ret = SharedState::set(State::ClimateSetpoint, setpoint);
            if (ret == ESP_OK) {
                // Publish change event
                EventBus::publish(Event::ClimateSetpointChanged, {
                    {"old", nullptr}, // TODO: get old value
                    {"new", setpoint},
                    {"source", "api"}
                });
                
                result = {
                    {"success", true},
                    {"setpoint", setpoint}
                };
            }
            return ret;
        }, "Set temperature setpoint");
    
    // Get climate mode
    dispatcher->register_method("climate.get_mode",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            std::string mode;
            if (SharedState::get_typed<std::string>(State::ClimateMode).has_value()) {
                mode = SharedState::get_typed<std::string>(State::ClimateMode).value();
                result = {
                    {"mode", mode},
                    {"timestamp", esp_timer_get_time()}
                };
                return ESP_OK;
            }
            return ESP_ERR_NOT_FOUND;
        }, "Get current climate control mode");
    
    ESP_LOGI(TAG, "Climate control APIs registered");
    return ESP_OK;
}

esp_err_t StaticApiRegistry::register_network_apis(ApiDispatcher* dispatcher) {
    ESP_LOGI(TAG, "Registering network APIs...");
    
    // WiFi status
    dispatcher->register_method("wifi.get_status", create_wifi_status_handler(),
                               "Get WiFi connection status");
    
    // Get IP address
    dispatcher->register_method("network.get_ip_address",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            std::string ip_address;
            if (SharedState::get_typed<std::string>(State::NetworkIpAddress).has_value()) {
                ip_address = SharedState::get_typed<std::string>(State::NetworkIpAddress).value();
                result = {
                    {"ip_address", ip_address},
                    {"timestamp", esp_timer_get_time()}
                };
                return ESP_OK;
            }
            return ESP_ERR_NOT_FOUND;
        }, "Get current IP address");
    
    // MQTT status
    dispatcher->register_method("mqtt.get_status",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            bool mqtt_connected;
            if (SharedState::get_typed<bool>(State::NetworkMqttConnected).has_value()) {
                mqtt_connected = SharedState::get_typed<bool>(State::NetworkMqttConnected).value();
                result = {
                    {"connected", mqtt_connected},
                    {"timestamp", esp_timer_get_time()}
                };
                return ESP_OK;
            }
            return ESP_ERR_NOT_FOUND;
        }, "Get MQTT connection status");
    
    ESP_LOGI(TAG, "Network APIs registered");
    return ESP_OK;
}

esp_err_t StaticApiRegistry::register_configuration_apis(ApiDispatcher* dispatcher) {
    ESP_LOGI(TAG, "Registering configuration APIs...");
    
    // Get sensors configuration
    dispatcher->register_method("config.get_sensors", create_config_get_sensors_handler(),
                               "Get current sensor configuration");
    
    // Get available sensor types
    dispatcher->register_method("config.get_available_sensor_types", 
                               create_config_get_available_types_handler(),
                               "Get list of available sensor driver types");
    
    // Get system configuration
    dispatcher->register_method("config.get_system",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            result = {
                {"firmware_version", "1.0.0"}, // TODO: get from build system
                {"compilation_date", __DATE__ " " __TIME__},
                {"free_heap", esp_get_free_heap_size()},
                {"uptime_seconds", esp_timer_get_time() / 1000000}
            };
            return ESP_OK;
        }, "Get system configuration and status");
    
    ESP_LOGI(TAG, "Configuration APIs registered");
    return ESP_OK;
}

// Helper method implementations
JsonRpcHandler StaticApiRegistry::create_system_status_handler() {
    return [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        result = {
            {"uptime_seconds", esp_timer_get_time() / 1000000},
            {"free_heap", esp_get_free_heap_size()},
            {"min_free_heap", esp_get_minimum_free_heap_size()},
            {"reset_reason", esp_reset_reason()},
            {"chip_model", esp_get_chip_model()},
            {"timestamp", esp_timer_get_time()}
        };
        return ESP_OK;
    };
}

JsonRpcHandler StaticApiRegistry::create_system_uptime_handler() {
    return [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        uint64_t uptime_us = esp_timer_get_time();
        result = {
            {"uptime_seconds", uptime_us / 1000000},
            {"uptime_milliseconds", uptime_us / 1000},
            {"uptime_microseconds", uptime_us}
        };
        return ESP_OK;
    };
}

JsonRpcHandler StaticApiRegistry::create_system_restart_handler() {
    return [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        result = {"message", "System restart initiated"};
        
        // Schedule restart after a short delay to allow response to be sent
        esp_timer_handle_t restart_timer;
        esp_timer_create_args_t timer_args = {
            .callback = [](void*) { esp_restart(); },
            .arg = nullptr,
            .name = "restart_timer"
        };
        
        esp_timer_create(&timer_args, &restart_timer);
        esp_timer_start_once(restart_timer, 1000000); // 1 second delay
        
        return ESP_OK;
    };
}

JsonRpcHandler StaticApiRegistry::create_wifi_status_handler() {
    return [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        nlohmann::json wifi_status;
        if (SharedState::get(State::NetworkWifiStatus, wifi_status)) {
            result = wifi_status;
            result["timestamp"] = esp_timer_get_time();
            return ESP_OK;
        }
        return ESP_ERR_NOT_FOUND;
    };
}

JsonRpcHandler StaticApiRegistry::create_sensor_get_all_handler() {
    return [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        result = nlohmann::json::object();
        
        float value;
        if (SharedState::get_typed<float>(State::SensorTemperature).has_value()) {
            value = SharedState::get_typed<float>(State::SensorTemperature).value();
            result["temperature"] = {{"value", value}, {"unit", "celsius"}};
        }
        
        if (SharedState::get_typed<float>(State::SensorHumidity).has_value()) {
            value = SharedState::get_typed<float>(State::SensorHumidity).value();
            result["humidity"] = {{"value", value}, {"unit", "percent"}};
        }
        
        if (SharedState::get_typed<float>(State::SensorPressure).has_value()) {
            value = SharedState::get_typed<float>(State::SensorPressure).value();
            result["pressure"] = {{"value", value}, {"unit", "bar"}};
        }
        
        bool door_open;
        if (SharedState::get_typed<bool>(State::SensorDoorOpen).has_value()) {
            door_open = SharedState::get_typed<bool>(State::SensorDoorOpen).value();
            result["door_open"] = door_open;
        }
        
        result["timestamp"] = esp_timer_get_time();
        return ESP_OK;
    };
}

JsonRpcHandler StaticApiRegistry::create_climate_get_setpoint_handler() {
    return [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        if (SharedState::get_typed<float>(State::ClimateSetpoint).has_value()) {
            float setpoint = SharedState::get_typed<float>(State::ClimateSetpoint).value();
            result = {
                {"setpoint", setpoint},
                {"unit", "celsius"},
                {"timestamp", esp_timer_get_time()}
            };
            return ESP_OK;
        }
        return ESP_ERR_NOT_FOUND;
    };
}

JsonRpcHandler StaticApiRegistry::create_config_get_sensors_handler() {
    return [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        // TODO: Load sensors configuration from file system
        // For now, return empty configuration as placeholder
        result = {
            {"sensors", nlohmann::json::array()},
            {"message", "Configuration loading not yet implemented"},
            {"timestamp", esp_timer_get_time()}
        };
        return ESP_OK;
    };
}

JsonRpcHandler StaticApiRegistry::create_config_get_available_types_handler() {
    return [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        // TODO: Get available types from SensorDriverRegistry
        // For now, return hardcoded list based on compiled drivers
        nlohmann::json types = nlohmann::json::array();
        
        #ifdef CONFIG_SENSOR_DRIVER_DS18B20_ASYNC_ENABLED
        types.push_back("DS18B20_Async");
        #endif
        
        #ifdef CONFIG_SENSOR_DRIVER_NTC_ENABLED
        types.push_back("NTC");
        #endif
        
        #ifdef CONFIG_SENSOR_DRIVER_GPIO_INPUT_ENABLED
        types.push_back("GPIO_Input");
        #endif
        
        result = {
            {"available_types", types},
            {"timestamp", esp_timer_get_time()}
        };
        return ESP_OK;
    };
}

std::vector<RpcMethodInfo> StaticApiRegistry::get_all_static_methods() {
    // TODO: Implement method to return all registered static methods
    // This will be useful for documentation and debugging
    return {};
}
