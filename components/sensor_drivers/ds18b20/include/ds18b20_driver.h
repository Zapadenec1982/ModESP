/**
 * @file ds18b20_driver.h
 * @brief DS18B20 OneWire temperature sensor driver
 * 
 * Self-contained driver for DS18B20 digital temperature sensors.
 * Includes all sensor-specific logic, configuration, and UI schema.
 */

#pragma once

#include "sensor_driver_interface.h"
#include "hal_interfaces.h"
#include <string>

/**
 * @brief DS18B20 temperature sensor driver
 * 
 * Features:
 * - Automatic sensor discovery on OneWire bus
 * - Configurable resolution (9-12 bits)
 * - Temperature offset calibration
 * - CRC validation
 * - Parasitic power support
 */
class DS18B20Driver : public ISensorDriver {
public:
    DS18B20Driver() = default;
    ~DS18B20Driver() override = default;
    
    // ISensorDriver interface implementation
    esp_err_t init(ESPhal& hal, const nlohmann::json& config) override;
    SensorReading read() override;
    std::string get_type() const override { return "DS18B20"; }
    std::string get_description() const override { return "Digital Temperature Sensor"; }
    bool is_available() const override { return sensor_available_; }
    nlohmann::json get_config() const override;
    esp_err_t set_config(const nlohmann::json& config) override;
    nlohmann::json get_ui_schema() const override;
    esp_err_t calibrate(const nlohmann::json& calibration_data) override;
    nlohmann::json get_diagnostics() const override;

private:
    // Configuration parameters
    struct Config {
        std::string hal_id;         // OneWire bus HAL identifier
        std::string address;        // 64-bit sensor address as hex string
        int resolution = 12;        // Resolution in bits (9-12)
        float offset = 0.0f;        // Temperature offset for calibration
        int read_timeout_ms = 1000; // Read timeout
        bool use_crc = true;        // Enable CRC checking
    } config_;
    
    // Runtime state
    IOneWireBus* bus_ = nullptr;
    uint64_t sensor_address_ = 0;
    bool sensor_available_ = false;
    uint32_t error_count_ = 0;
    uint32_t successful_reads_ = 0;
    float last_temperature_ = 0.0f;
    
    // Helper methods
    uint64_t parse_address(const std::string& hex_address);
    int get_conversion_time_ms() const;
    bool validate_temperature(float temp) const;
};