/**
 * @file sensor_driver_interface.h
 * @brief Universal interface for all sensor drivers in ModuChill system
 * 
 * This interface defines the contract that all sensor drivers must implement.
 * Each driver is self-contained and includes:
 * - Hardware communication logic
 * - Data processing and calibration
 * - Configuration management
 * - UI configuration for driver-specific settings
 */

#pragma once

#include <string>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>
#include "esp_err.h"

// Forward declarations
class ESPhal;

/**
 * @brief Sensor types supported by the system
 */
enum class SensorType {
    TEMPERATURE,
    HUMIDITY,
    PRESSURE,
    VOLTAGE,
    CURRENT,
    UNKNOWN
};

/**
 * @brief Sensor reading result with timestamp and status
 */
struct SensorReading {
    float value;                    // Processed sensor value
    std::string unit;              // Unit of measurement (°C, %, Pa, etc.)
    uint32_t timestamp_ms;         // Timestamp in milliseconds
    bool is_valid;                 // Reading validity flag
    std::optional<std::string> error_message;  // Error description if any
    
    /**
     * @brief Convert reading to JSON for SharedState
     */
    nlohmann::json to_json() const {
        nlohmann::json j;
        j["value"] = value;
        j["unit"] = unit;
        j["timestamp_ms"] = timestamp_ms;
        j["is_valid"] = is_valid;
        if (error_message.has_value()) {
            j["error"] = error_message.value();
        }
        return j;
    }
};

/**
 * @brief Base interface for all sensor drivers
 * 
 * Each sensor driver implementation must:
 * 1. Handle all hardware-specific communication
 * 2. Process raw data into standardized readings
 * 3. Manage its own configuration
 * 4. Provide UI configuration schema
 * 5. Handle calibration if applicable
 */
class ISensorDriver {
public:
    virtual ~ISensorDriver() = default;
    
    /**
     * @brief Initialize the sensor driver
     * 
     * @param hal Reference to HAL for accessing hardware resources
     * @param config Driver-specific configuration from JSON
     * @return ESP_OK on success
     */
    virtual esp_err_t init(ESPhal& hal, const nlohmann::json& config) = 0;
    
    /**
     * @brief Read current sensor value
     * 
     * This method performs all necessary operations to get a reading:
     * - Communication with hardware
     * - Data conversion and processing  
     * - Applying calibration
     * - Error handling
     * 
     * @return Processed sensor reading
     */
    virtual SensorReading read() = 0;
    
    /**
     * @brief Get driver type identifier
     * @return String identifier like "DS18B20", "NTC", "PRESSURE_4_20MA"
     */
    virtual std::string get_type() const = 0;
    
    /**
     * @brief Get human-readable driver description
     * @return Description for UI display
     */
    virtual std::string get_description() const = 0;
    
    /**
     * @brief Check if sensor is ready/available
     * @return true if sensor is functioning properly
     */
    virtual bool is_available() const = 0;
    
    /**
     * @brief Get current driver configuration
     * @return JSON object with all configuration parameters
     */
    virtual nlohmann::json get_config() const = 0;
    
    /**
     * @brief Update driver configuration at runtime
     * 
     * @param config New configuration parameters
     * @return ESP_OK on success
     */
    virtual esp_err_t set_config(const nlohmann::json& config) = 0;
    
    /**
     * @brief Get UI configuration schema for this driver
     * 
     * Returns JSON schema describing all configurable parameters
     * and their UI representation (sliders, inputs, dropdowns, etc.)
     * 
     * @return JSON schema for UI generation
     */
    virtual nlohmann::json get_ui_schema() const = 0;
    
    /**
     * @brief Perform sensor calibration if supported
     * 
     * @param calibration_data Driver-specific calibration parameters
     * @return ESP_OK if calibration successful, ESP_ERR_NOT_SUPPORTED if not applicable
     */
    virtual esp_err_t calibrate(const nlohmann::json& calibration_data) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    /**
     * @brief Get diagnostic information
     * @return JSON object with driver-specific diagnostics
     */
    virtual nlohmann::json get_diagnostics() const {
        return nlohmann::json::object();
    }
};

/**
 * @brief Factory function type for creating sensor driver instances
 */
using SensorDriverFactory = std::unique_ptr<ISensorDriver>(*)();