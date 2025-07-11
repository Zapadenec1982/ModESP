/**
 * @file manifest_processor.cpp
 * @brief Implementation of Manifest Processor
 * 
 * Part of TODO-006: Hybrid API Contract Implementation
 * Phase 3: Manifest Integration
 */

#include "manifest_processor.h"
#include "sensor_driver_registry.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <fstream>
#include <sstream>

// Forward declaration of helper function
nlohmann::json build_sensor_item_schema(const std::vector<std::string>& available_types);

constexpr const char* ManifestProcessor::TAG;

nlohmann::json ManifestProcessor::load_module_manifest(const std::string& module_name) {
    ESP_LOGI(TAG, "Loading manifest for module: %s", module_name.c_str());
    
    // Try embedded data first (compile-time manifests)
    auto manifest = load_manifest_from_embedded_data(module_name);
    if (!manifest.empty()) {
        ESP_LOGI(TAG, "Loaded manifest from embedded data for: %s", module_name.c_str());
        return manifest;
    }
    
    // Try file system (runtime manifests)
    manifest = load_manifest_from_filesystem(module_name);
    if (!manifest.empty()) {
        ESP_LOGI(TAG, "Loaded manifest from filesystem for: %s", module_name.c_str());
        return manifest;
    }
    
    ESP_LOGW(TAG, "No manifest found for module: %s", module_name.c_str());
    return nlohmann::json{};
}

std::vector<std::string> ManifestProcessor::get_available_sensor_types() {
    ESP_LOGI(TAG, "Getting available sensor types...");
    
    auto& registry = SensorDriverRegistry::instance();
    auto types = registry.get_registered_types();
    
    ESP_LOGI(TAG, "Found %zu available sensor types", types.size());
    for (const auto& type : types) {
        ESP_LOGD(TAG, "  - %s", type.c_str());
    }
    
    return types;
}

nlohmann::json ManifestProcessor::get_sensor_type_schema(const std::string& type) {
    ESP_LOGI(TAG, "Getting UI schema for sensor type: %s", type.c_str());
    
    // Get schema directly from driver
    auto& registry = SensorDriverRegistry::instance();
    auto driver = registry.create_driver(type);
    if (!driver) {
        ESP_LOGW(TAG, "Could not create driver for type: %s", type.c_str());
        return nlohmann::json{};
    }
    
    auto schema = driver->get_ui_schema();
    ESP_LOGD(TAG, "Retrieved schema for %s", type.c_str());
    return schema;
}

nlohmann::json ManifestProcessor::build_runtime_api_schema() {
    ESP_LOGI(TAG, "Building runtime API schema...");
    
    nlohmann::json schema = {
        {"version", "1.0"},
        {"timestamp", esp_timer_get_time()},
        {"static_apis", nlohmann::json::array()},
        {"dynamic_apis", nlohmann::json::array()},
        {"configuration_apis", nlohmann::json::array()}
    };
    
    // TODO: Integrate with ApiDispatcher to get actual registered methods
    // For now, return basic structure
    
    ESP_LOGI(TAG, "Runtime API schema built");
    return schema;
}

nlohmann::json ManifestProcessor::build_ui_schema_for_current_config() {
    ESP_LOGI(TAG, "Building UI schema for current configuration...");
    
    nlohmann::json ui_schema = {
        {"version", "1.0"},
        {"timestamp", esp_timer_get_time()},
        {"sensor_controls", nlohmann::json::array()},
        {"actuator_controls", nlohmann::json::array()},
        {"configuration_controls", nlohmann::json::array()}
    };
    
    // Get available sensor types and build configuration UI
    auto sensor_types = get_available_sensor_types();
    
    nlohmann::json sensor_config_control = {
        {"id", "sensor_configuration"},
        {"type", "object"},
        {"title", "Sensor Configuration"},
        {"properties", {
            {"sensors", {
                {"type", "array"},
                {"title", "Configured Sensors"},
                {"items", build_sensor_item_schema(sensor_types)}
            }}
        }}
    };
    
    ui_schema["configuration_controls"].push_back(sensor_config_control);
    
    ESP_LOGI(TAG, "UI schema built for current configuration");
    return ui_schema;
}

std::vector<nlohmann::json> ManifestProcessor::extract_rpc_methods_from_manifest(const nlohmann::json& manifest) {
    std::vector<nlohmann::json> methods;
    
    if (manifest.contains("rpc_api") && manifest["rpc_api"].contains("methods")) {
        for (const auto& [method_name, method_def] : manifest["rpc_api"]["methods"].items()) {
            nlohmann::json method_info = {
                {"name", method_name},
                {"description", method_def.value("description", "")},
                {"params", method_def.value("params", nlohmann::json::object())},
                {"returns", method_def.value("returns", nlohmann::json::object())}
            };
            methods.push_back(method_info);
        }
    }
    
    ESP_LOGI(TAG, "Extracted %zu RPC methods from manifest", methods.size());
    return methods;
}

nlohmann::json ManifestProcessor::extract_events_from_manifest(const nlohmann::json& manifest) {
    nlohmann::json events = {
        {"publishes", nlohmann::json::array()},
        {"subscribes", nlohmann::json::array()}
    };
    
    if (manifest.contains("event_bus")) {
        if (manifest["event_bus"].contains("publishes")) {
            for (const auto& [event_name, event_def] : manifest["event_bus"]["publishes"].items()) {
                events["publishes"].push_back({
                    {"name", event_name},
                    {"description", event_def.value("description", "")},
                    {"payload", event_def.value("payload", nlohmann::json::object())}
                });
            }
        }
        
        if (manifest["event_bus"].contains("subscribes")) {
            for (const auto& [event_name, event_def] : manifest["event_bus"]["subscribes"].items()) {
                events["subscribes"].push_back({
                    {"name", event_name},
                    {"description", event_def.value("description", "")},
                    {"handler", event_def.value("handler", "")}
                });
            }
        }
    }
    
    return events;
}

bool ManifestProcessor::validate_manifest(const nlohmann::json& manifest) {
    // Basic validation
    if (!manifest.is_object()) {
        ESP_LOGE(TAG, "Manifest must be an object");
        return false;
    }
    
    if (!manifest.contains("module") || !manifest["module"].is_object()) {
        ESP_LOGE(TAG, "Manifest must contain 'module' object");
        return false;
    }
    
    auto module_info = manifest["module"];
    if (!module_info.contains("name") || !module_info["name"].is_string()) {
        ESP_LOGE(TAG, "Module must have a 'name' string");
        return false;
    }
    
    if (!module_info.contains("version") || !module_info["version"].is_string()) {
        ESP_LOGE(TAG, "Module must have a 'version' string");
        return false;
    }
    
    ESP_LOGI(TAG, "Manifest validation passed for module: %s", 
             module_info["name"].get<std::string>().c_str());
    return true;
}

// Private helper methods
nlohmann::json ManifestProcessor::load_manifest_from_embedded_data(const std::string& module_name) {
    // TODO: Implement loading from embedded data
    // This would use ESP-IDF's EMBED_FILES functionality
    ESP_LOGD(TAG, "Attempting to load embedded manifest for: %s", module_name.c_str());
    
    // For now, return empty - will be implemented when we add manifest embedding
    return nlohmann::json{};
}

nlohmann::json ManifestProcessor::load_manifest_from_filesystem(const std::string& module_name) {
    std::string manifest_path = get_manifest_path(module_name);
    
    ESP_LOGD(TAG, "Attempting to load manifest from: %s", manifest_path.c_str());
    
    try {
        std::ifstream file(manifest_path);
        if (!file.is_open()) {
            ESP_LOGD(TAG, "Could not open manifest file: %s", manifest_path.c_str());
            return nlohmann::json{};
        }
        
        nlohmann::json manifest;
        file >> manifest;
        
        if (validate_manifest(manifest)) {
            return manifest;
        } else {
            ESP_LOGW(TAG, "Invalid manifest file: %s", manifest_path.c_str());
            return nlohmann::json{};
        }
        
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Error loading manifest %s: %s", manifest_path.c_str(), e.what());
        return nlohmann::json{};
    }
}

std::string ManifestProcessor::get_manifest_path(const std::string& module_name) {
    // Standard path for module manifests
    return "/spiffs/manifests/" + module_name + "_manifest.json";
}

// Helper method for building sensor item schema
nlohmann::json build_sensor_item_schema(const std::vector<std::string>& available_types) {
    nlohmann::json schema = {
        {"type", "object"},
        {"properties", {
            {"role", {
                {"type", "string"},
                {"title", "Sensor Role"},
                {"enum", {"temperature_1", "temperature_2", "humidity", "door_sensor", "pressure"}}
            }},
            {"type", {
                {"type", "string"},
                {"title", "Sensor Type"},
                {"enum", available_types},
                {"onChange", "restart_required"}
            }},
            {"config", {
                {"type", "object"},
                {"title", "Configuration"},
                {"description", "Sensor-specific configuration parameters"}
            }}
        }},
        {"required", {"role", "type"}}
    };
    
    return schema;
}
