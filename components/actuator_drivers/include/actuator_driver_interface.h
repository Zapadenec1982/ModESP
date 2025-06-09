/**
 * @file actuator_driver_interface.h
 * @brief Universal interface for all actuator drivers in ModuChill system
 * 
 * This interface defines the contract that all actuator drivers must implement.
 * Each driver is self-contained and includes:
 * - Hardware control logic
 * - Command execution
 * - State management
 * - Configuration handling
 * - UI configuration for driver-specific settings
 */

#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "esp_err.h"

// Forward declarations
class ESPhal;

/**
 * @brief Actuator status information
 */
struct ActuatorStatus {
    bool is_active;                  // Current active state
    float current_value;             // Current value (for PWM, position, etc.)
    std::string state_description;   // Human-readable state
    uint32_t last_change_ms;        // Time of last state change
    bool is_healthy;                // Health status
    std::optional<std::string> error_message;  // Error if any
    
    /**
     * @brief Convert status to JSON for SharedState
     */
    nlohmann::json to_json() const {
        nlohmann::json j;
        j["is_active"] = is_active;
        j["current_value"] = current_value;
        j["state"] = state_description;
        j["last_change_ms"] = last_change_ms;
        j["is_healthy"] = is_healthy;
        if (error_message.has_value()) {
            j["error"] = error_message.value();
        }
        return j;
    }
};

/**
 * @brief Base interface for all actuator drivers
 * 
 * Each actuator driver implementation must:
 * 1. Handle all hardware-specific control
 * 2. Execute commands from SharedState
 * 3. Manage timing constraints (min on/off times)
 * 4. Provide status feedback
 * 5. Handle safety interlocks
 */
class IActuatorDriver {
public:
    virtual ~IActuatorDriver() = default;
    
    /**
     * @brief Initialize the actuator driver
     * 
     * @param hal Reference to HAL for accessing hardware resources
     * @param config Driver-specific configuration from JSON
     * @return ESP_OK on success
     */
    virtual esp_err_t init(ESPhal& hal, const nlohmann::json& config) = 0;
    
    /**
     * @brief Execute a command
     * 
     * Commands can be:
     * - Boolean: true/false for on/off
     * - Number: 0-100 for PWM duty, position, etc.
     * - Object: Complex commands with multiple parameters
     * 
     * @param command Command data from SharedState
     * @return ESP_OK on success, error code if command cannot be executed
     */
    virtual esp_err_t execute_command(const nlohmann::json& command) = 0;
    
    /**
     * @brief Get current actuator status
     * @return Current status information
     */
    virtual ActuatorStatus get_status() const = 0;
    
    /**
     * @brief Get driver type identifier
     * @return String identifier like "RELAY", "PWM", "STEPPER"
     */
    virtual std::string get_type() const = 0;
    
    /**
     * @brief Get human-readable driver description
     * @return Description for UI display
     */
    virtual std::string get_description() const = 0;
    
    /**
     * @brief Check if actuator is available and ready
     * @return true if actuator can accept commands
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
     * and their UI representation
     * 
     * @return JSON schema for UI generation
     */
    virtual nlohmann::json get_ui_schema() const = 0;
    
    /**
     * @brief Force stop/safe state
     * 
     * Called in emergency situations or system shutdown
     * Driver should move to safe state immediately
     * 
     * @return ESP_OK on success
     */
    virtual esp_err_t emergency_stop() = 0;
    
    /**
     * @brief Get diagnostic information
     * @return JSON object with driver-specific diagnostics
     */
    virtual nlohmann::json get_diagnostics() const {
        return nlohmann::json::object();
    }
    
    /**
     * @brief Update driver state (called periodically)
     * 
     * Allows drivers to handle time-based constraints,
     * safety checks, etc.
     */
    virtual void update() {}
};

/**
 * @brief Factory function type for creating actuator driver instances
 */
using ActuatorDriverFactory = std::unique_ptr<IActuatorDriver>(*)();