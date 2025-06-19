/**
 * @file ds18b20_async_driver.h
 * @brief Asynchronous DS18B20 OneWire temperature sensor driver
 * 
 * Non-blocking driver for DS18B20 digital temperature sensors.
 * Uses state machine to avoid blocking during temperature conversion.
 */

#pragma once

#include "sensor_driver_interface.h"
#include "hal_interfaces.h"
#include <string>
#include <esp_timer.h>

/**
 * @brief Asynchronous DS18B20 temperature sensor driver
 * 
 * Features:
 * - Non-blocking temperature conversion
 * - Automatic sensor discovery on OneWire bus
 * - Configurable resolution (9-12 bits)
 * - Temperature offset calibration
 * - CRC validation
 * - Parasitic power support
 */
class DS18B20AsyncDriver : public ISensorDriver {
public:
    DS18B20AsyncDriver() = default;
    ~DS18B20AsyncDriver() override = default;
    
    // ISensorDriver interface implementation
    esp_err_t init(ESPhal* hal, const nlohmann::json& config) override;
    SensorReading read() override;
    std::string get_type() const override { return "DS18B20_Async"; }
    std::string get_description() const override { return "Async Digital Temperature Sensor"; }
    bool is_available() const override { return sensor_available_; }
    nlohmann::json get_config() const override;
    esp_err_t set_config(const nlohmann::json& config) override;
    nlohmann::json get_ui_schema() const override;
    esp_err_t calibrate(const nlohmann::json& calibration_data) override;
    nlohmann::json get_diagnostics() const override;

private:
    // State machine states
    enum class State {
        IDLE,
        CONVERSION_REQUESTED,
        WAITING_FOR_CONVERSION,
        READY_TO_READ,
        ERROR
    };
    
    // Configuration parameters
    struct Config {
        std::string hal_id;         // OneWire bus HAL identifier
        std::string address;        // 64-bit sensor address as hex string
        int resolution = 12;        // Resolution in bits (9-12)
        float offset = 0.0f;        // Temperature offset for calibration
        int read_timeout_ms = 1000; // Read timeout
        bool use_crc = true;        // Enable CRC checking
        int max_retries = 3;        // Maximum read retries
    } config_;
    
    // Runtime state
    IOneWireBus* bus_ = nullptr;
    uint64_t sensor_address_ = 0;
    bool sensor_available_ = false;
    
    // State machine
    State state_ = State::IDLE;
    int64_t conversion_start_time_ms_ = 0;
    int retry_count_ = 0;
    
    // Cached values
    float last_temperature_ = 0.0f;
    int64_t last_valid_read_time_ms_ = 0;
    bool has_valid_reading_ = false;
    
    // Statistics
    uint32_t error_count_ = 0;
    uint32_t successful_reads_ = 0;
    uint32_t total_conversions_ = 0;
    
    // Helper methods
    uint64_t parse_address(const std::string& hex_address);
    int get_conversion_time_ms() const;
    bool validate_temperature(float temp) const;
    bool is_conversion_complete() const;
    void reset_state();
};
