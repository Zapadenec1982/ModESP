// sensor_manager_adaptive.h
// Example of SensorManager with Manager-Driver composition

#pragma once

#include "base_module.h"
#include "base_driver.h"
#include <vector>
#include <memory>
#include <unordered_map>

/**
 * @brief Adaptive SensorManager with driver composition
 * 
 * This is an example of how to implement Manager-Driver pattern
 * for Phase 5 Adaptive UI Architecture
 */
class SensorManagerAdaptive : public BaseModule {
private:
    // Registered sensor drivers
    std::vector<std::unique_ptr<ISensorDriver>> drivers;
    std::unordered_map<std::string, ISensorDriver*> driver_map;
    
    // Configuration
    struct SensorConfig {
        std::string role;         // "temperature", "humidity", etc.
        std::string driver_type;  // "DS18B20", "NTC", etc.
        std::string hal_id;       // HAL resource ID
        nlohmann::json config;    // Driver-specific config
    };
    
    std::vector<SensorConfig> sensor_configs;
    uint32_t update_interval_ms = 1000;
    
    // State
    struct SensorReading {
        std::string role;
        float value;
        std::string unit;
        uint32_t timestamp;
        bool valid;
    };
    
    std::unordered_map<std::string, SensorReading> readings;
    
public:
    SensorManagerAdaptive() : BaseModule() {}
    
    // BaseModule interface
    const char* get_name() const override { return "SensorManagerAdaptive"; }
    
    esp_err_t init() override;
    esp_err_t configure(const nlohmann::json& config) override;
    esp_err_t start() override;
    void update() override;
    
    // Manager-specific methods
    
    /**
     * @brief Register a sensor driver
     * 
     * Called by ModuleManager during initialization
     */
    esp_err_t registerDriver(std::unique_ptr<ISensorDriver> driver);
    
    /**
     * @brief Get all registered drivers
     */
    std::vector<ISensorDriver*> getDrivers() const;
    
    /**
     * @brief Get driver by type
     */
    ISensorDriver* getDriverByType(const std::string& type) const;
    
    /**
     * @brief Get UI components from all drivers
     */
    std::vector<std::string> getAllUIComponents() const;
    
    // RPC methods
    esp_err_t get_temperature_rpc(const nlohmann::json& params, 
                                  nlohmann::json& response);
    
    esp_err_t get_all_readings_rpc(const nlohmann::json& params,
                                   nlohmann::json& response);
    
    esp_err_t calibrate_sensor_rpc(const nlohmann::json& params,
                                   nlohmann::json& response);
    
private:
    /**
     * @brief Create driver instance based on type
     */
    std::unique_ptr<ISensorDriver> createDriver(const std::string& type);
    
    /**
     * @brief Update readings from all drivers
     */
    void updateReadings();
    
    /**
     * @brief Publish readings to SharedState
     */
    void publishReadings();
    
    /**
     * @brief Register RPC methods
     */
    void registerRPCMethods();
};

/**
 * @brief Example DS18B20 driver implementation
 */
class DS18B20DriverAdaptive : public ISensorDriver {
private:
    std::string name = "DS18B20Driver";
    DriverState state = DriverState::CREATED;
    
    // Configuration
    uint8_t pin = 0;
    uint8_t resolution = 12;
    bool parasite_power = false;
    
    // Runtime data
    float last_temperature = 0.0f;
    bool sensor_present = false;
    
public:
    // BaseDriver interface
    const char* get_name() const override { return name.c_str(); }
    const char* get_type() const override { return "DS18B20"; }
    
    esp_err_t init() override;
    esp_err_t configure(const nlohmann::json& config) override;
    esp_err_t update() override;
    
    DriverState get_state() const override { return state; }
    bool is_healthy() const override { return sensor_present && state == DriverState::RUNNING; }
    
    nlohmann::json get_capabilities() const override {
        return {
            {"measurement_range", {{"min", -55}, {"max", 125}}},
            {"accuracy", "±0.5°C"},
            {"resolution", "0.0625°C"},
            {"supports_calibration", true}
        };
    }
    
    std::vector<std::string> get_ui_components() const override {
        return {
            "ds18b20_resolution_slider",
            "ds18b20_parasite_toggle",
            "ds18b20_address_display"
        };
    }
    
    // ISensorDriver interface
    esp_err_t read_value(float& value) override {
        if (!sensor_present) return ESP_ERR_NOT_FOUND;
        value = last_temperature;
        return ESP_OK;
    }
    
    const char* get_unit() const override { return "°C"; }
    
    esp_err_t calibrate(float reference_value) override;
};
