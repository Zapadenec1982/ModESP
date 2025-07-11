/**
 * @file manifest_processor.h
 * @brief Manifest Processor for module manifest integration
 * 
 * Part of TODO-006: Hybrid API Contract Implementation
 * Phase 3: Manifest Integration
 */

#pragma once

#include "nlohmann/json.hpp"
#include <string>
#include <vector>

/**
 * @brief Manifest Processor for handling module manifests
 * 
 * This class processes module manifests to extract API information,
 * UI schemas, and configuration details for the hybrid API system.
 */
class ManifestProcessor {
public:
    /**
     * @brief Load module manifest from embedded data or file system
     * 
     * @param module_name Name of the module (e.g., "sensor_drivers")
     * @return JSON object containing the manifest, or empty if not found
     */
    static nlohmann::json load_module_manifest(const std::string& module_name);
    
    /**
     * @brief Get list of available sensor types from compiled drivers
     * 
     * @return Vector of available sensor type names
     */
    static std::vector<std::string> get_available_sensor_types();
    
    /**
     * @brief Get UI schema for specific sensor type
     * 
     * @param type Sensor type name (e.g., "DS18B20_Async", "NTC")
     * @return JSON schema for the sensor type UI
     */
    static nlohmann::json get_sensor_type_schema(const std::string& type);
    
    /**
     * @brief Build runtime API schema from current configuration
     * 
     * Combines static APIs with dynamic APIs based on current system configuration.
     * 
     * @return JSON object describing all available APIs
     */
    static nlohmann::json build_runtime_api_schema();
    
    /**
     * @brief Build UI schema for current configuration
     * 
     * Generates UI schema that adapts to current sensor/actuator configuration.
     * 
     * @return JSON object with adaptive UI schema
     */
    static nlohmann::json build_ui_schema_for_current_config();
    
    /**
     * @brief Get RPC methods from manifest
     * 
     * @param manifest Module manifest JSON
     * @return Vector of RPC method definitions
     */
    static std::vector<nlohmann::json> extract_rpc_methods_from_manifest(const nlohmann::json& manifest);
    
    /**
     * @brief Get event definitions from manifest
     * 
     * @param manifest Module manifest JSON
     * @return JSON object with event definitions
     */
    static nlohmann::json extract_events_from_manifest(const nlohmann::json& manifest);
    
    /**
     * @brief Validate manifest structure
     * 
     * @param manifest Manifest to validate
     * @return true if manifest is valid
     */
    static bool validate_manifest(const nlohmann::json& manifest);

private:
    /**
     * @brief Load manifest from embedded data (compile-time)
     * 
     * @param module_name Module name
     * @return Manifest JSON or empty if not found
     */
    static nlohmann::json load_manifest_from_embedded_data(const std::string& module_name);
    
    /**
     * @brief Load manifest from file system (runtime)
     * 
     * @param module_name Module name
     * @return Manifest JSON or empty if not found
     */
    static nlohmann::json load_manifest_from_filesystem(const std::string& module_name);
    
    /**
     * @brief Get manifest file path
     * 
     * @param module_name Module name
     * @return Full path to manifest file
     */
    static std::string get_manifest_path(const std::string& module_name);
    
    // Tag for logging
    static constexpr const char* TAG = "ManifestProcessor";
};

