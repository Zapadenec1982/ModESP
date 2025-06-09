/**
 * @file relay_driver.h
 * @brief Relay actuator driver
 * 
 * Self-contained driver for relay control with safety features.
 * Includes minimum on/off time protection, state tracking, and diagnostics.
 */

#pragma once

#include "actuator_driver_interface.h"
#include "hal_interfaces.h"
#include <esp_timer.h>

/**
 * @brief Relay actuator driver
 * 
 * Features:
 * - Binary on/off control
 * - Minimum on/off time protection
 * - State change counting
 * - Inrush current delay support
 * - Active high/low configuration
 */
class RelayDriver : public IActuatorDriver {
public:
    RelayDriver() = default;
    ~RelayDriver() override = default;
    
    // IActuatorDriver interface implementation
    esp_err_t init(ESPhal& hal, const nlohmann::json& config) override;
    esp_err_t execute_command(const nlohmann::json& command) override;
    ActuatorStatus get_status() const override;
    std::string get_type() const override { return "RELAY"; }
    std::string get_description() const override { return "Relay Output Driver"; }
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
        std::string hal_id;              // GPIO output HAL identifier
        uint32_t min_off_time_s = 0;     // Minimum off time in seconds
        uint32_t min_on_time_s = 0;      // Minimum on time in seconds  
        uint32_t inrush_delay_ms = 0;    // Delay after turn on (for inrush current)
        bool active_low = false;         // Invert relay logic
        bool default_state = false;      // Default state on init
        std::string on_label = "ON";     // Label for ON state
        std::string off_label = "OFF";   // Label for OFF state
    } config_;    
    // Runtime state
    IGpioOutput* gpio_output_ = nullptr;
    bool current_state_ = false;
    bool commanded_state_ = false;
    uint64_t last_change_time_ms_ = 0;
    uint64_t protection_end_time_ms_ = 0;
    bool protection_active_ = false;
    
    // Statistics
    uint32_t state_changes_ = 0;
    uint32_t protection_blocks_ = 0;
    uint32_t total_on_time_s_ = 0;
    uint64_t last_on_time_ms_ = 0;
    
    // Helper methods
    bool can_change_state(bool new_state) const;
    void apply_state(bool state);
    uint64_t get_time_ms() const;
    uint32_t get_time_in_state_ms() const;
};