/**
 * @file gpio_input_driver.h
 * @brief GPIO input sensor driver for digital inputs
 * 
 * Self-contained driver for digital inputs like door switches,
 * alarm contacts, and other binary sensors.
 */

#pragma once

#include "sensor_driver_interface.h"
#include "hal_interfaces.h"
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

/**
 * @brief GPIO input sensor driver
 * 
 * Features:
 * - Configurable input polarity (normal/inverted)
 * - Software debouncing with configurable time
 * - Edge detection and counting
 * - Pull-up/pull-down configuration
 * - Event generation on state change
 */
class GpioInputDriver : public ISensorDriver {
public:
    GpioInputDriver();
    ~GpioInputDriver() override;
    
    // ISensorDriver interface implementation
    esp_err_t init(ESPhal& hal, const nlohmann::json& config) override;
    SensorReading read() override;
    std::string get_type() const override { return "GPIO_INPUT"; }
    std::string get_description() const override { return "Digital Input Sensor"; }
    bool is_available() const override { return gpio_input_ != nullptr; }
    nlohmann::json get_config() const override;
    esp_err_t set_config(const nlohmann::json& config) override;
    nlohmann::json get_ui_schema() const override;
    nlohmann::json get_diagnostics() const override;
private:
    // Configuration parameters
    struct Config {
        std::string hal_id;         // GPIO input HAL identifier
        bool invert = false;        // Invert input logic
        uint32_t debounce_ms = 50;  // Debounce time in milliseconds
        bool count_edges = false;   // Count state transitions
        std::string active_label = "ON";   // Label for active state
        std::string inactive_label = "OFF"; // Label for inactive state
    } config_;
    
    // Runtime state
    IGpioInput* gpio_input_ = nullptr;
    bool last_state_ = false;
    bool debounced_state_ = false;
    uint32_t state_change_count_ = 0;
    uint32_t total_reads_ = 0;
    uint64_t last_change_time_ms_ = 0;
    uint64_t debounce_start_time_ms_ = 0;
    bool debouncing_ = false;
    
    // Helper methods
    bool read_raw_state();
    void update_debounced_state();
    uint64_t get_time_ms() const;
};