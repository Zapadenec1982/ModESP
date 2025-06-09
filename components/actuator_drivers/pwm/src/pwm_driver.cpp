/**
 * @file pwm_driver.cpp
 * @brief Implementation of PWM output actuator driver
 */

#include "pwm_driver.h"
#include "actuator_driver_registry.h"
#include <esp_log.h>
#include <cmath>
#include <algorithm>

static const char* TAG = "PwmDriver";

// Auto-register this driver
static ActuatorDriverRegistrar<PwmDriver> registrar("PWM");

// Static channel allocation (simple approach)
static uint8_t allocated_channels = 0;

PwmDriver::~PwmDriver() {
    if (initialized_) {
        ledc_stop(LEDC_LOW_SPEED_MODE, channel_, 0);
    }
}

esp_err_t PwmDriver::init(ESPhal& hal, const nlohmann::json& config) {
    ESP_LOGI(TAG, "Initializing PWM driver");
    
    // Parse configuration
    try {
        config_.gpio_num = config["gpio_num"].get<int>();
        config_.frequency = config.value("frequency", 5000);
        config_.resolution_bits = config.value("resolution_bits", 10);
        config_.min_duty_percent = config.value("min_duty_percent", 0.0f);
        config_.max_duty_percent = config.value("max_duty_percent", 100.0f);
        config_.ramp_time_ms = config.value("ramp_time_ms", 0);
        config_.gamma = config.value("gamma", 1.0f);
        config_.invert = config.value("invert", false);
        config_.default_duty = config.value("default_duty", 0.0f);
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }    
    // Validate configuration
    if (config_.resolution_bits < 1 || config_.resolution_bits > 15) {
        ESP_LOGE(TAG, "Invalid resolution: %d", config_.resolution_bits);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize PWM
    esp_err_t ret = init_pwm();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Set default duty
    set_duty_immediate(config_.default_duty);
    target_duty_ = config_.default_duty;
    
    initialized_ = true;
    ESP_LOGI(TAG, "PWM driver initialized on GPIO %d", config_.gpio_num);
    
    return ESP_OK;
}

esp_err_t PwmDriver::init_pwm() {
    // Allocate channel and timer
    channel_ = (ledc_channel_t)(allocated_channels % LEDC_CHANNEL_MAX);
    timer_ = (ledc_timer_t)((allocated_channels / LEDC_CHANNEL_MAX) % LEDC_TIMER_MAX);
    allocated_channels++;
    
    // Configure timer
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)config_.resolution_bits,
        .timer_num = timer_,
        .freq_hz = config_.frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };    
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure channel
    ledc_channel_config_t channel_conf = {
        .gpio_num = config_.gpio_num,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel_,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer_,
        .duty = 0,
        .hpoint = 0,
        .flags = {
            .output_invert = config_.invert ? 1u : 0u
        }
    };
    
    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t PwmDriver::execute_command(const nlohmann::json& command) {
    float new_duty = 0.0f;
    
    // Parse command
    try {
        if (command.is_number()) {
            new_duty = command.get<float>();
        } else if (command.is_object() && command.contains("duty")) {
            new_duty = command["duty"].get<float>();
        } else {
            ESP_LOGE(TAG, "Invalid command format");
            return ESP_ERR_INVALID_ARG;
        }
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Command parsing error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }    
    // Clamp to configured limits
    new_duty = std::max(config_.min_duty_percent, 
                       std::min(config_.max_duty_percent, new_duty));
    
    target_duty_ = new_duty;
    command_count_++;
    
    // Apply duty cycle
    if (config_.ramp_time_ms > 0 && std::abs(new_duty - current_duty_) > 0.1f) {
        start_ramp(new_duty);
    } else {
        set_duty_immediate(new_duty);
    }
    
    return ESP_OK;
}

void PwmDriver::update() {
    if (ramping_) {
        update_ramp();
    }
    
    // Update statistics
    if (current_duty_ > 0.0f && last_on_time_ > 0) {
        uint64_t now = get_time_ms();
        total_on_time_ms_ += now - last_on_time_;
        last_on_time_ = now;
    }
}

ActuatorStatus PwmDriver::get_status() const {
    ActuatorStatus status;
    status.is_active = current_duty_ > 0.0f;
    status.current_value = current_duty_;
    status.state_description = std::to_string((int)current_duty_) + "%";
    status.last_change_ms = ramp_start_time_;
    status.is_healthy = initialized_;
    
    if (ramping_) {
        status.state_description += " (ramping to " + 
                                   std::to_string((int)target_duty_) + "%)";
    }
    
    return status;
}
esp_err_t PwmDriver::emergency_stop() {
    ESP_LOGW(TAG, "Emergency stop activated");
    
    // Stop immediately
    ramping_ = false;
    set_duty_immediate(0.0f);
    target_duty_ = 0.0f;
    
    return ESP_OK;
}

nlohmann::json PwmDriver::get_config() const {
    return {
        {"gpio_num", config_.gpio_num},
        {"frequency", config_.frequency},
        {"resolution_bits", config_.resolution_bits},
        {"min_duty_percent", config_.min_duty_percent},
        {"max_duty_percent", config_.max_duty_percent},
        {"ramp_time_ms", config_.ramp_time_ms},
        {"gamma", config_.gamma},
        {"invert", config_.invert}
    };
}

esp_err_t PwmDriver::set_config(const nlohmann::json& config) {
    try {
        if (config.contains("min_duty_percent")) {
            config_.min_duty_percent = config["min_duty_percent"].get<float>();
        }
        
        if (config.contains("max_duty_percent")) {
            config_.max_duty_percent = config["max_duty_percent"].get<float>();
        }
        
        if (config.contains("ramp_time_ms")) {
            config_.ramp_time_ms = config["ramp_time_ms"].get<uint32_t>();
        }
        
        if (config.contains("gamma")) {
            config_.gamma = config["gamma"].get<float>();
        }
        
        return ESP_OK;
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration update error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
}
nlohmann::json PwmDriver::get_ui_schema() const {
    return {
        {"type", "object"},
        {"title", "PWM Output Settings"},
        {"properties", {
            {"duty_limits", {
                {"type", "object"},
                {"title", "Duty Cycle Limits"},
                {"properties", {
                    {"min", {
                        {"type", "number"},
                        {"title", "Minimum Duty (%)"},
                        {"minimum", 0},
                        {"maximum", 100},
                        {"default", 0}
                    }},
                    {"max", {
                        {"type", "number"},
                        {"title", "Maximum Duty (%)"},
                        {"minimum", 0},
                        {"maximum", 100},
                        {"default", 100}
                    }}
                }}
            }},
            {"ramp_time_ms", {
                {"type", "integer"},
                {"title", "Ramp Time (ms)"},
                {"description", "Soft start/stop duration"},
                {"minimum", 0},
                {"maximum", 10000},
                {"default", 0}
            }},
            {"gamma", {
                {"type", "number"},
                {"title", "Gamma Correction"},
                {"description", "For LED dimming (1.0 = linear)"},
                {"minimum", 0.1},
                {"maximum", 5.0},
                {"default", 1.0},
                {"ui:widget", "slider"},
                {"ui:step", 0.1}
            }}
        }}
    };
}
nlohmann::json PwmDriver::get_diagnostics() const {
    return {
        {"current_duty", current_duty_},
        {"target_duty", target_duty_},
        {"ramping", ramping_},
        {"command_count", command_count_},
        {"total_on_time_ms", total_on_time_ms_},
        {"gpio_num", config_.gpio_num},
        {"frequency", config_.frequency},
        {"channel", channel_},
        {"timer", timer_}
    };
}

// Helper methods
void PwmDriver::set_duty_immediate(float duty_percent) {
    if (!initialized_) return;
    
    // Apply gamma correction
    float corrected_duty = apply_gamma(duty_percent / 100.0f) * 100.0f;
    
    // Convert to raw duty value
    uint32_t duty = percent_to_duty(corrected_duty);
    
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty);
    if (ret == ESP_OK) {
        ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
        current_duty_ = duty_percent;
        
        // Update on-time tracking
        if (duty_percent > 0.0f && last_on_time_ == 0) {
            last_on_time_ = get_time_ms();
        } else if (duty_percent == 0.0f && last_on_time_ > 0) {
            total_on_time_ms_ += get_time_ms() - last_on_time_;
            last_on_time_ = 0;
        }
    }
}
void PwmDriver::start_ramp(float target_duty) {
    ramp_start_time_ = get_time_ms();
    ramp_start_duty_ = current_duty_;
    ramping_ = true;
}

void PwmDriver::update_ramp() {
    uint64_t now = get_time_ms();
    uint64_t elapsed = now - ramp_start_time_;
    
    if (elapsed >= config_.ramp_time_ms) {
        // Ramp complete
        set_duty_immediate(target_duty_);
        ramping_ = false;
    } else {
        // Calculate intermediate duty
        float progress = (float)elapsed / config_.ramp_time_ms;
        float duty = ramp_start_duty_ + (target_duty_ - ramp_start_duty_) * progress;
        set_duty_immediate(duty);
    }
}

uint32_t PwmDriver::percent_to_duty(float percent) const {
    uint32_t max_duty = (1 << config_.resolution_bits) - 1;
    return (uint32_t)((percent / 100.0f) * max_duty);
}

float PwmDriver::apply_gamma(float linear_value) const {
    if (config_.gamma == 1.0f) {
        return linear_value;
    }
    return std::pow(linear_value, config_.gamma);
}

uint64_t PwmDriver::get_time_ms() const {
    return esp_timer_get_time() / 1000;
}