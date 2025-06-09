/**
 * @file pwm_driver.h
 * @brief PWM output actuator driver
 * 
 * Self-contained driver for PWM control with features like
 * soft start/stop, duty cycle limits, and frequency configuration.
 */

#pragma once

#include "actuator_driver_interface.h"
#include "driver/ledc.h"
#include <esp_timer.h>

/**
 * @brief PWM output actuator driver
 * 
 * Features:
 * - Variable duty cycle control (0-100%)
 * - Configurable frequency
 * - Soft start/stop with ramping
 * - Min/max duty limits
 * - Gamma correction for LED dimming
 */
class PwmDriver : public IActuatorDriver {
public:
    PwmDriver() = default;
    ~PwmDriver() override;
    
    // IActuatorDriver interface implementation
    esp_err_t init(ESPhal& hal, const nlohmann::json& config) override;
    esp_err_t execute_command(const nlohmann::json& command) override;
    ActuatorStatus get_status() const override;
    std::string get_type() const override { return "PWM"; }
    std::string get_description() const override { return "PWM Output Driver"; }
    bool is_available() const override { return initialized_; }
    nlohmann::json get_config() const override;
    esp_err_t set_config(const nlohmann::json& config) override;
    nlohmann::json get_ui_schema() const override;
    esp_err_t emergency_stop() override;
    nlohmann::json get_diagnostics() const override;
    void update() override;
private:
    // Configuration parameters
    struct Config {
        int gpio_num = -1;               // GPIO pin number
        uint32_t frequency = 5000;       // PWM frequency in Hz
        uint8_t resolution_bits = 10;    // Resolution (8-15 bits)
        float min_duty_percent = 0.0f;   // Minimum duty cycle %
        float max_duty_percent = 100.0f; // Maximum duty cycle %
        uint32_t ramp_time_ms = 0;       // Soft start/stop time
        float gamma = 1.0f;              // Gamma correction factor
        bool invert = false;             // Invert PWM signal
        float default_duty = 0.0f;       // Default duty cycle
    } config_;
    
    // Runtime state
    bool initialized_ = false;
    ledc_channel_t channel_;
    ledc_timer_t timer_;
    float current_duty_ = 0.0f;
    float target_duty_ = 0.0f;
    uint64_t ramp_start_time_ = 0;
    float ramp_start_duty_ = 0.0f;
    bool ramping_ = false;
    
    // Statistics
    uint32_t command_count_ = 0;
    uint64_t total_on_time_ms_ = 0;
    uint64_t last_on_time_ = 0;
    
    // Helper methods
    esp_err_t init_pwm();
    void set_duty_immediate(float duty_percent);
    void start_ramp(float target_duty);
    void update_ramp();
    uint32_t percent_to_duty(float percent) const;
    float apply_gamma(float linear_value) const;
    uint64_t get_time_ms() const;
};