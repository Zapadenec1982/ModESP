/**
 * @file gpio_output_driver.h
 * @brief Simple GPIO output actuator driver
 * 
 * Basic driver for controlling GPIO outputs without relay-specific features.
 * Useful for LEDs, buzzers, and other simple digital outputs.
 */

#pragma once

#include "actuator_driver_interface.h"
#include "hal_interfaces.h"

/**
 * @brief GPIO output actuator driver
 * 
 * Features:
 * - Simple on/off control
 * - Configurable active high/low
 * - Blink pattern support
 * - State tracking
 */
class GpioOutputDriver : public IActuatorDriver {
public:
    GpioOutputDriver() = default;
    ~GpioOutputDriver() override = default;
    
    // IActuatorDriver interface implementation
    esp_err_t init(ESPhal& hal, const nlohmann::json& config) override;
    esp_err_t execute_command(const nlohmann::json& command) override;
    ActuatorStatus get_status() const override;
    std::string get_type() const override { return "GPIO_OUTPUT"; }
    std::string get_description() const override { return "GPIO Output Driver"; }
    bool is_available() const override { return gpio_output_ != nullptr; }
    nlohmann::json get_config() const override;
    esp_err_t set_config(const nlohmann::json& config) override;
    nlohmann::json get_ui_schema() const override;
    esp_err_t emergency_stop() override;
    nlohmann::json get_diagnostics() const override;
    void update() override;

private:
    // Configuration parameters
    struct Config {
        std::string hal_id;          // GPIO output HAL identifier
        bool active_low = false;     // Invert output logic
        bool default_state = false;  // Default state on init
        uint32_t blink_on_ms = 0;    // Blink on time (0 = no blink)
        uint32_t blink_off_ms = 0;   // Blink off time
    } config_;
    
    // Runtime state
    IGpioOutput* gpio_output_ = nullptr;
    bool current_state_ = false;
    bool commanded_state_ = false;
    bool blinking_ = false;
    uint64_t last_blink_time_ = 0;
    bool blink_phase_ = false;
    
    // Statistics
    uint32_t state_changes_ = 0;
    uint64_t total_on_time_ms_ = 0;
    uint64_t last_on_time_ = 0;
    
    // Helper methods
    void set_state(bool state);
    void update_blink();
    uint64_t get_time_ms() const;
};
