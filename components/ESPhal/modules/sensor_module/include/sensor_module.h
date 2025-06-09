/**
 * @file sensor_module.h
 * @brief SensorModule - HAL module for sensor data collection
 * 
 * SensorModule uses a modular driver system where each sensor type is a separate
 * self-contained driver. It reads configuration, creates driver instances through
 * the registry, periodically polls sensors, and publishes data to SharedState.
 * 
 * Key features:
 * - Dynamic driver loading through registry
 * - Type-agnostic sensor management  
 * - Configuration-driven sensor creation
 * - Standardized data publishing
 */

#pragma once

#include "base_module.h"
#include "esphal.h"
#include "shared_state.h"
#include "sensor_driver_interface.h"
#include "nlohmann/json.hpp"
#include <vector>
#include <memory>
#include <string>

/**
 * @brief Configuration for a single sensor instance
 */
struct SensorConfig {
    std::string role;           // Logical role (e.g., "chamber_temp")
    std::string type;           // Driver type (e.g., "DS18B20", "NTC")
    std::string publish_key;    // SharedState key to publish data
    nlohmann::json config;      // Driver-specific configuration
    
    // Parsing helper
    static SensorType parse_type(const std::string& type_str);
    static std::string type_to_string(SensorType type);
};

/**
 * @brief SensorModule - HAL module for sensor management
 * 
 * Uses modular driver architecture where each sensor type is a self-contained
 * driver component. Drivers are loaded dynamically through the registry.
 */
class SensorModule : public BaseModule {
public:
    /**
     * @brief Constructor with Dependency Injection
     * @param hal Reference to ESPhal instance for hardware access
     */
    explicit SensorModule(ESPhal& hal);
    
    /**
     * @brief Destructor
     */
    ~SensorModule() override = default;
    
    // Non-copyable, non-movable
    SensorModule(const SensorModule&) = delete;
    SensorModule& operator=(const SensorModule&) = delete;
    SensorModule(SensorModule&&) = delete;
    SensorModule& operator=(SensorModule&&) = delete;

    // === BaseModule interface ===
    
    const char* get_name() const override {
        return "SensorModule";  // Config section: "sensors"
    }
    
    esp_err_t init() override;
    void update() override;
    void stop() override;
    void configure(const nlohmann::json& config) override;
    bool is_healthy() const override;
    uint8_t get_health_score() const override;
    uint32_t get_max_update_time_us() const override;
    
    // === SensorModule specific methods ===
    
    /**
     * @brief Get latest reading from a sensor by role
     * @param role Sensor role (e.g., "chamber_temp")
     * @return Latest reading or nullopt if sensor not found
     */
    std::optional<::SensorReading> get_sensor_reading(const std::string& role) const;
    
    /**
     * @brief Get sensor configuration by role
     * @param role Sensor role
     * @return Sensor config or nullopt if not found
     */
    std::optional<SensorConfig> get_sensor_config(const std::string& role) const;
    
    /**
     * @brief Force immediate sensor poll (for testing)
     * @return ESP_OK on success
     */
    esp_err_t poll_sensors_now();
    
    /**
     * @brief Get available sensor driver types
     * @return List of registered driver type identifiers
     */
    std::vector<std::string> get_available_drivers() const;

private:
    // Structure to hold sensor instance with its configuration
    struct SensorInstance {
        std::unique_ptr<ISensorDriver> driver;
        SensorConfig config;
        ::SensorReading last_reading;
        uint32_t poll_failures = 0;
    };
    
    // Reference to HAL for hardware access
    ESPhal& hal_;
    
    // All active sensors
    std::vector<SensorInstance> sensors_;
    
    // Module state
    bool initialized_ = false;
    uint32_t update_count_ = 0;
    uint32_t total_errors_ = 0;
    
    // Configuration
    uint32_t poll_interval_ms_ = 1000;  // Default 1 second
    bool publish_on_error_ = true;       // Publish error states
    
    // Helper methods
    esp_err_t create_sensor_from_config(const nlohmann::json& sensor_config);
    void publish_sensor_data(const SensorInstance& sensor, const ::SensorReading& reading);
    std::unique_ptr<ISensorDriver> create_driver(const std::string& type);
};

#endif // SENSOR_MODULE_H

    // === SensorModule member variables ===
    
    ESPhal& hal_;                                           // HAL instance reference
    std::vector<SensorConfig> sensor_configs_;              // Sensor configurations
    std::vector<std::unique_ptr<ISensorDriver>> sensors_;   // Internal sensor objects
    std::vector<SensorReading> last_readings_;              // Last readings cache
    
    uint32_t poll_interval_ms_;     // Polling interval in milliseconds
    uint32_t last_poll_time_;       // Last poll time (milliseconds since boot)
    bool initialized_;              // Initialization state
    uint32_t error_count_;          // Error counter for health monitoring
    uint32_t successful_polls_;     // Successful poll counter
    
    // Helper methods
    esp_err_t load_sensor_config(const nlohmann::json& config);
    esp_err_t create_sensor_drivers();
    std::unique_ptr<ISensorDriver> create_sensor_driver(const SensorConfig& config);
    
    void poll_sensors();
    void publish_sensor_data(size_t sensor_index, const SensorReading& reading);
    
    uint32_t get_current_time_ms() const;
    size_t find_sensor_by_role(const std::string& role) const;
};
