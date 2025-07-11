/**
 * @file dynamic_api_builder.cpp
 * @brief Implementation of Dynamic API Builder
 * 
 * Part of TODO-006: Hybrid API Contract Implementation
 */

#include "dynamic_api_builder.h"
#include "api_dispatcher.h"
#include "sensor_driver_registry.h"
#include "system_contract.h"
#include "shared_state.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <fstream>

using namespace ModespContract;

constexpr const char* DynamicApiBuilder::TAG;

DynamicApiBuilder::DynamicApiBuilder(ApiDispatcher* dispatcher) 
    : dispatcher_(dispatcher) {
    ESP_LOGI(TAG, "DynamicApiBuilder created");
}

esp_err_t DynamicApiBuilder::build_all_dynamic_apis() {
    ESP_LOGI(TAG, "Building all dynamic APIs...");
    
    esp_err_t ret = ESP_OK;
    
    // Build sensor APIs based on configuration
    ret = build_sensor_apis();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build sensor APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Build defrost APIs based on configuration
    ret = build_defrost_apis();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build defrost APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Build scenario APIs based on configuration
    ret = build_scenario_apis();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build scenario APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "All dynamic APIs built successfully");
    return ESP_OK;
}

esp_err_t DynamicApiBuilder::rebuild_dynamic_apis() {
    ESP_LOGI(TAG, "Rebuilding dynamic APIs...");
    
    // TODO: Clear existing dynamic APIs from dispatcher
    // This will be implemented when ApiDispatcher supports method removal
    
    return build_all_dynamic_apis();
}

esp_err_t DynamicApiBuilder::build_sensor_apis() {
    ESP_LOGI(TAG, "Building sensor APIs...");
    
    auto sensor_config = load_sensors_config();
    
    if (!sensor_config.contains("sensors") || !sensor_config["sensors"].is_array()) {
        ESP_LOGW(TAG, "No sensors configuration found");
        return ESP_OK;
    }
    
    for (const auto& sensor : sensor_config["sensors"]) {
        if (!sensor.contains("type") || !sensor.contains("role")) {
            ESP_LOGW(TAG, "Invalid sensor configuration, skipping");
            continue;
        }
        
        std::string type = sensor["type"];
        std::string role = sensor["role"];
        
        ESP_LOGI(TAG, "Building APIs for sensor %s (type: %s)", role.c_str(), type.c_str());
        
        // Check if sensor type is available
        if (!is_sensor_type_available(type)) {
            ESP_LOGW(TAG, "Sensor type %s not available, skipping", type.c_str());
            continue;
        }
        
        // Get UI schema from driver
        auto& registry = SensorDriverRegistry::instance();
        auto driver = registry.create_driver(type);
        if (!driver) {
            ESP_LOGW(TAG, "Could not create driver for type %s", type.c_str());
            continue;
        }
        
        auto ui_schema = driver->get_ui_schema();
        
        // Generate APIs from schema
        esp_err_t ret = generate_sensor_apis_from_schema(role, type, ui_schema);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to generate APIs for sensor %s", role.c_str());
        }
    }
    
    ESP_LOGI(TAG, "Sensor APIs built");
    return ESP_OK;
}

esp_err_t DynamicApiBuilder::generate_sensor_apis_from_schema(
    const std::string& role, 
    const std::string& type,
    const nlohmann::json& schema) {
    
    ESP_LOGI(TAG, "Generating APIs for sensor %s from schema", role.c_str());
    
    std::string base_method = "sensor." + role + ".";
    
    // Register base methods for this sensor
    dispatcher_->register_dynamic_method(base_method + "get_value",
        create_sensor_get_value_handler(role),
        "Get current value from sensor " + role);
    
    dispatcher_->register_dynamic_method(base_method + "get_diagnostics", 
        create_sensor_get_diagnostics_handler(role),
        "Get diagnostics for sensor " + role);
    
    dispatcher_->register_dynamic_method(base_method + "calibrate",
        create_sensor_calibrate_handler(role),
        "Calibrate sensor " + role);
    
    // Generate set/get methods for each property in the UI schema
    if (schema.contains("properties") && schema["properties"].is_object()) {
        for (const auto& [prop, definition] : schema["properties"].items()) {
            // Create set method for this property
            dispatcher_->register_dynamic_method(base_method + "set_" + prop,
                create_sensor_set_property_handler(role, prop, definition),
                "Set " + prop + " for sensor " + role);
            
            // Create get method for this property
            dispatcher_->register_dynamic_method(base_method + "get_" + prop,
                create_sensor_get_property_handler(role, prop),
                "Get " + prop + " for sensor " + role);
        }
    }
    
    ESP_LOGI(TAG, "Generated APIs for sensor %s", role.c_str());
    return ESP_OK;
}

esp_err_t DynamicApiBuilder::build_defrost_apis() {
    ESP_LOGI(TAG, "Building defrost APIs...");
    
    auto defrost_config = load_defrost_config();
    
    if (!defrost_config.contains("type")) {
        ESP_LOGW(TAG, "No defrost configuration found");
        return ESP_OK;
    }
    
    std::string defrost_type = defrost_config["type"];
    ESP_LOGI(TAG, "Building APIs for defrost type: %s", defrost_type.c_str());
    
    // Generate APIs based on defrost type
    if (defrost_type == "TIME_BASED") {
        dispatcher_->register_dynamic_method("defrost.set_interval",
            [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                // TODO: Implement time-based defrost interval setting
                result = {"message", "Time-based defrost interval set"};
                return ESP_OK;
            }, "Set defrost interval for time-based defrost");
            
        dispatcher_->register_dynamic_method("defrost.set_duration",
            [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                // TODO: Implement defrost duration setting
                result = {"message", "Defrost duration set"};
                return ESP_OK;
            }, "Set defrost duration");
            
    } else if (defrost_type == "TEMPERATURE_BASED") {
        dispatcher_->register_dynamic_method("defrost.set_trigger_temp",
            [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                // TODO: Implement temperature trigger setting
                result = {"message", "Temperature trigger set"};
                return ESP_OK;
            }, "Set temperature trigger for defrost");
            
        dispatcher_->register_dynamic_method("defrost.set_end_temp",
            [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                // TODO: Implement end temperature setting
                result = {"message", "End temperature set"};
                return ESP_OK;
            }, "Set defrost end temperature");
            
    } else if (defrost_type == "ADAPTIVE") {
        dispatcher_->register_dynamic_method("defrost.set_algorithm_params",
            [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                // TODO: Implement adaptive algorithm parameters
                result = {"message", "Adaptive algorithm parameters set"};
                return ESP_OK;
            }, "Set adaptive defrost algorithm parameters");
            
        dispatcher_->register_dynamic_method("defrost.get_prediction",
            [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                // TODO: Implement defrost prediction
                result = {
                    {"next_defrost_in_minutes", 45},
                    {"confidence", 0.85}
                };
                return ESP_OK;
            }, "Get next defrost prediction");
    }
    
    ESP_LOGI(TAG, "Defrost APIs built for type: %s", defrost_type.c_str());
    return ESP_OK;
}

esp_err_t DynamicApiBuilder::build_scenario_apis() {
    ESP_LOGI(TAG, "Building scenario APIs...");
    
    auto scenario_config = load_scenario_config();
    
    // TODO: Implement scenario-based API generation
    // For now, just log that scenarios are being processed
    ESP_LOGI(TAG, "Scenario APIs built (placeholder)");
    return ESP_OK;
}

// Handler factory implementations
JsonRpcHandler DynamicApiBuilder::create_sensor_get_value_handler(const std::string& role) {
    return [role](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        // TODO: Get value from specific sensor by role
        // For now, return placeholder data
        result = {
            {"sensor_role", role},
            {"value", 23.5},
            {"unit", "celsius"},
            {"timestamp", esp_timer_get_time()}
        };
        return ESP_OK;
    };
}

JsonRpcHandler DynamicApiBuilder::create_sensor_get_diagnostics_handler(const std::string& role) {
    return [role](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        // TODO: Get diagnostics from specific sensor by role
        result = {
            {"sensor_role", role},
            {"status", "OK"},
            {"last_reading_time", esp_timer_get_time()},
            {"error_count", 0},
            {"response_time_ms", 25}
        };
        return ESP_OK;
    };
}

JsonRpcHandler DynamicApiBuilder::create_sensor_calibrate_handler(const std::string& role) {
    return [role](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        // TODO: Implement sensor calibration
        if (!params.contains("reference_value")) {
            return ESP_ERR_INVALID_ARG;
        }
        
        float reference_value = params["reference_value"];
        
        result = {
            {"sensor_role", role},
            {"calibration_success", true},
            {"reference_value", reference_value},
            {"old_offset", 0.0},
            {"new_offset", 0.5}
        };
        return ESP_OK;
    };
}

JsonRpcHandler DynamicApiBuilder::create_sensor_set_property_handler(
    const std::string& role, 
    const std::string& property,
    const nlohmann::json& definition) {
    
    return [role, property, definition](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        if (!params.contains("value")) {
            return ESP_ERR_INVALID_ARG;
        }
        
        // TODO: Validate value against definition
        // TODO: Set property on actual sensor driver
        
        result = {
            {"sensor_role", role},
            {"property", property},
            {"value", params["value"]},
            {"success", true}
        };
        return ESP_OK;
    };
}

JsonRpcHandler DynamicApiBuilder::create_sensor_get_property_handler(
    const std::string& role, 
    const std::string& property) {
    
    return [role, property](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        // TODO: Get property value from actual sensor driver
        result = {
            {"sensor_role", role},
            {"property", property},
            {"value", "placeholder_value"}
        };
        return ESP_OK;
    };
}

// Configuration loader implementations
nlohmann::json DynamicApiBuilder::load_sensors_config() {
    // TODO: Load from actual configuration file system
    // For now, return placeholder configuration
    return {
        {"sensors", nlohmann::json::array({
            {
                {"role", "temperature_1"},
                {"type", "DS18B20_Async"},
                {"config", {
                    {"resolution", 12},
                    {"use_crc", true}
                }}
            },
            {
                {"role", "temperature_2"},
                {"type", "NTC"},
                {"config", {
                    {"ntc_type", "10K_3950"},
                    {"series_resistor", 10000}
                }}
            }
        })}
    };
}

nlohmann::json DynamicApiBuilder::load_defrost_config() {
    // TODO: Load from actual configuration file system
    return {
        {"type", "TIME_BASED"},
        {"interval_hours", 6},
        {"duration_minutes", 30}
    };
}

nlohmann::json DynamicApiBuilder::load_scenario_config() {
    // TODO: Load from actual configuration file system
    return {
        {"scenarios", nlohmann::json::array()}
    };
}

// Utility method implementations
bool DynamicApiBuilder::is_sensor_type_available(const std::string& type) {
    auto& registry = SensorDriverRegistry::instance();
    auto available_types = registry.get_registered_types();
    
    return std::find(available_types.begin(), available_types.end(), type) != available_types.end();
}

std::string DynamicApiBuilder::get_method_name(const std::string& role, const std::string& action) {
    return "sensor." + role + "." + action;
}

esp_err_t DynamicApiBuilder::validate_property_value(const nlohmann::json& value, 
                                                    const nlohmann::json& definition) {
    // TODO: Implement comprehensive validation based on JSON schema definition
    // For now, just basic type checking
    
    if (definition.contains("type")) {
        std::string expected_type = definition["type"];
        
        if (expected_type == "string" && !value.is_string()) {
            return ESP_ERR_INVALID_ARG;
        } else if (expected_type == "number" && !value.is_number()) {
            return ESP_ERR_INVALID_ARG;
        } else if (expected_type == "boolean" && !value.is_boolean()) {
            return ESP_ERR_INVALID_ARG;
        } else if (expected_type == "integer" && !value.is_number_integer()) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    
    return ESP_OK;
}
