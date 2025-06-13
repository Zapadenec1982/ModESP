/**
 * @file gpio_output_driver.cpp
 * @brief Implementation of GPIO output actuator driver
 */

#include "gpio_output_driver.h"
#include "actuator_driver_registry.h"
#include "esphal.h"
#include <esp_log.h>
#include <esp_timer.h>

static const char* TAG = "GpioOutputDriver";

// Auto-register this driver
static ActuatorDriverRegistrar<GpioOutputDriver> registrar("GPIO_OUTPUT");

esp_err_t GpioOutputDriver::init(ESPhal& hal, const nlohmann::json& config) {
    ESP_LOGI(TAG, "Initializing GPIO output driver");
    
    // Parse configuration
    try {
        config_.hal_id = config["hal_id"].get<std::string>();
        config_.active_low = config.value("active_low", false);
        config_.default_state = config.value("default_state", false);
        config_.blink_on_ms = config.value("blink_on_ms", 0);
        config_.blink_off_ms = config.value("blink_off_ms", 0);
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get GPIO output from HAL
    try {
        gpio_output_ = &hal.get_gpio_output(config_.hal_id);
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Failed to get GPIO output '%s': %s", 
                 config_.hal_id.c_str(), e.what());
        return ESP_FAIL;
    }
    
    // Set initial state
    set_state(config_.default_state);
    commanded_state_ = config_.default_state;
    
    ESP_LOGI(TAG, "GPIO output initialized: %s", config_.hal_id.c_str());
    
    return ESP_OK;
}

esp_err_t GpioOutputDriver::execute_command(const nlohmann::json& command) {
    try {
        if (command.is_boolean()) {
            commanded_state_ = command.get<bool>();
            blinking_ = false;
        } else if (command.is_string()) {
            std::string cmd = command.get<std::string>();
            if (cmd == "on") {
                commanded_state_ = true;
                blinking_ = false;
            } else if (cmd == "off") {
                commanded_state_ = false;
                blinking_ = false;
            } else if (cmd == "toggle") {
                commanded_state_ = !current_state_;
                blinking_ = false;
            } else if (cmd == "blink") {
                blinking_ = true;
                last_blink_time_ = get_time_ms();
            }
        } else if (command.is_object()) {
            if (command.contains("state")) {
                commanded_state_ = command["state"].get<bool>();
                blinking_ = false;
            }
            if (command.contains("blink")) {
                blinking_ = command["blink"].get<bool>();
                if (blinking_) {
                    last_blink_time_ = get_time_ms();
                }
            }
        }
        
        if (!blinking_) {
            set_state(commanded_state_);
        }
        
        return ESP_OK;
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Command parsing error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
}

void GpioOutputDriver::update() {
    if (blinking_ && config_.blink_on_ms > 0 && config_.blink_off_ms > 0) {
        update_blink();
    }
    
    // Update on-time statistics
    if (current_state_ && last_on_time_ > 0) {
        uint64_t now = get_time_ms();
        total_on_time_ms_ += now - last_on_time_;
        last_on_time_ = now;
    }
}

void GpioOutputDriver::update_blink() {
    uint64_t now = get_time_ms();
    uint64_t elapsed = now - last_blink_time_;
    
    uint32_t current_duration = blink_phase_ ? config_.blink_on_ms : config_.blink_off_ms;
    
    if (elapsed >= current_duration) {
        blink_phase_ = !blink_phase_;
        set_state(blink_phase_);
        last_blink_time_ = now;
    }
}

ActuatorStatus GpioOutputDriver::get_status() const {
    ActuatorStatus status;
    status.is_active = current_state_;
    status.current_value = current_state_ ? 1.0f : 0.0f;
    
    if (blinking_) {
        status.state_description = "BLINKING";
    } else {
        status.state_description = current_state_ ? "ON" : "OFF";
    }
    
    status.last_change_ms = last_on_time_;
    status.is_healthy = gpio_output_ != nullptr;
    
    return status;
}

esp_err_t GpioOutputDriver::emergency_stop() {
    ESP_LOGW(TAG, "Emergency stop activated");
    
    blinking_ = false;
    set_state(false);
    commanded_state_ = false;
    
    return ESP_OK;
}

nlohmann::json GpioOutputDriver::get_config() const {
    return {
        {"hal_id", config_.hal_id},
        {"active_low", config_.active_low},
        {"default_state", config_.default_state},
        {"blink_on_ms", config_.blink_on_ms},
        {"blink_off_ms", config_.blink_off_ms}
    };
}

esp_err_t GpioOutputDriver::set_config(const nlohmann::json& config) {
    try {
        if (config.contains("blink_on_ms")) {
            config_.blink_on_ms = config["blink_on_ms"].get<uint32_t>();
        }
        
        if (config.contains("blink_off_ms")) {
            config_.blink_off_ms = config["blink_off_ms"].get<uint32_t>();
        }
        
        return ESP_OK;
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration update error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
}

nlohmann::json GpioOutputDriver::get_ui_schema() const {
    return {
        {"type", "object"},
        {"title", "GPIO Output Settings"},
        {"properties", {
            {"blink_pattern", {
                {"type", "object"},
                {"title", "Blink Pattern"},
                {"properties", {
                    {"on_ms", {
                        {"type", "integer"},
                        {"title", "ON Duration (ms)"},
                        {"minimum", 0},
                        {"maximum", 10000},
                        {"default", 500}
                    }},
                    {"off_ms", {
                        {"type", "integer"},
                        {"title", "OFF Duration (ms)"},
                        {"minimum", 0},
                        {"maximum", 10000},
                        {"default", 500}
                    }}
                }}
            }}
        }}
    };
}

nlohmann::json GpioOutputDriver::get_diagnostics() const {
    return {
        {"current_state", current_state_},
        {"commanded_state", commanded_state_},
        {"blinking", blinking_},
        {"state_changes", state_changes_},
        {"total_on_time_ms", total_on_time_ms_},
        {"hal_id", config_.hal_id}
    };
}

// Helper methods
void GpioOutputDriver::set_state(bool state) {
    if (!gpio_output_) {
        return;
    }
    
    bool physical_state = config_.active_low ? !state : state;
    esp_err_t ret = gpio_output_->set_state(physical_state);
    
    if (ret == ESP_OK && state != current_state_) {
        current_state_ = state;
        state_changes_++;
        
        // Track on time
        if (state) {
            last_on_time_ = get_time_ms();
        } else if (last_on_time_ > 0) {
            total_on_time_ms_ += get_time_ms() - last_on_time_;
            last_on_time_ = 0;
        }
    }
}

uint64_t GpioOutputDriver::get_time_ms() const {
    return esp_timer_get_time() / 1000;
}
