/**
 * @file relay_driver.cpp
 * @brief Implementation of relay actuator driver
 */

#include "relay_driver.h"
#include "actuator_driver_registry.h"
#include "esphal.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "RelayDriver";

// Auto-register this driver
static ActuatorDriverRegistrar<RelayDriver> registrar("RELAY");

esp_err_t RelayDriver::init(ESPhal& hal, const nlohmann::json& config) {
    ESP_LOGI(TAG, "Initializing relay driver");
    
    // Parse configuration
    try {
        config_.hal_id = config["hal_id"].get<std::string>();
        config_.min_off_time_s = config.value("min_off_time_s", 0);
        config_.min_on_time_s = config.value("min_on_time_s", 0);
        config_.inrush_delay_ms = config.value("inrush_delay_ms", 0);
        config_.active_low = config.value("active_low", false);
        config_.default_state = config.value("default_state", false);
        config_.on_label = config.value("on_label", "ON");
        config_.off_label = config.value("off_label", "OFF");
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
    apply_state(config_.default_state);
    commanded_state_ = config_.default_state;
    last_change_time_ms_ = get_time_ms();
    
    ESP_LOGI(TAG, "Relay driver initialized: %s, default state: %s", 
             config_.hal_id.c_str(), 
             config_.default_state ? config_.on_label.c_str() : config_.off_label.c_str());
    
    return ESP_OK;
}

esp_err_t RelayDriver::execute_command(const nlohmann::json& command) {
    bool new_state = false;
    
    // Parse command
    try {
        if (command.is_boolean()) {
            new_state = command.get<bool>();
        } else if (command.is_number()) {
            // Accept 0/1 as off/on
            new_state = command.get<int>() != 0;
        } else if (command.is_object() && command.contains("state")) {
            new_state = command["state"].get<bool>();
        } else {
            ESP_LOGE(TAG, "Invalid command format");
            return ESP_ERR_INVALID_ARG;
        }
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Command parsing error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }    
    commanded_state_ = new_state;
    
    // Check if we can change state (protection timers)
    if (!can_change_state(new_state)) {
        protection_blocks_++;
        ESP_LOGW(TAG, "State change blocked by protection timer");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Apply new state
    if (new_state != current_state_) {
        apply_state(new_state);
        
        // Handle inrush delay if turning on
        if (new_state && config_.inrush_delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(config_.inrush_delay_ms));
        }
    }
    
    return ESP_OK;
}

ActuatorStatus RelayDriver::get_status() const {
    ActuatorStatus status;
    status.is_active = current_state_;
    status.current_value = current_state_ ? 1.0f : 0.0f;
    status.state_description = current_state_ ? config_.on_label : config_.off_label;
    status.last_change_ms = last_change_time_ms_;
    status.is_healthy = gpio_output_ != nullptr;
    
    if (protection_active_) {
        uint64_t remaining_ms = 0;
        uint64_t now = get_time_ms();
        if (protection_end_time_ms_ > now) {
            remaining_ms = protection_end_time_ms_ - now;
        }
        status.error_message = "Protection timer active (" + 
                              std::to_string(remaining_ms / 1000) + "s remaining)";
    }
    
    return status;
}
void RelayDriver::update() {
    // Check if protection timer has expired
    if (protection_active_) {
        uint64_t now = get_time_ms();
        if (now >= protection_end_time_ms_) {
            protection_active_ = false;
            ESP_LOGI(TAG, "Protection timer expired");
            
            // Try to apply commanded state
            if (commanded_state_ != current_state_ && can_change_state(commanded_state_)) {
                apply_state(commanded_state_);
            }
        }
    }
    
    // Update total on time
    if (current_state_ && last_on_time_ms_ > 0) {
        uint64_t now = get_time_ms();
        total_on_time_s_ += (now - last_on_time_ms_) / 1000;
        last_on_time_ms_ = now;
    }
}

esp_err_t RelayDriver::emergency_stop() {
    ESP_LOGW(TAG, "Emergency stop activated");
    
    // Force relay to safe state (off) immediately
    protection_active_ = false;  // Override protection
    apply_state(false);
    commanded_state_ = false;
    
    return ESP_OK;
}

nlohmann::json RelayDriver::get_config() const {
    return {
        {"hal_id", config_.hal_id},
        {"min_off_time_s", config_.min_off_time_s},
        {"min_on_time_s", config_.min_on_time_s},
        {"inrush_delay_ms", config_.inrush_delay_ms},
        {"active_low", config_.active_low},
        {"default_state", config_.default_state},
        {"on_label", config_.on_label},
        {"off_label", config_.off_label}
    };
}
esp_err_t RelayDriver::set_config(const nlohmann::json& config) {
    try {
        if (config.contains("min_off_time_s")) {
            config_.min_off_time_s = config["min_off_time_s"].get<uint32_t>();
        }
        
        if (config.contains("min_on_time_s")) {
            config_.min_on_time_s = config["min_on_time_s"].get<uint32_t>();
        }
        
        if (config.contains("inrush_delay_ms")) {
            config_.inrush_delay_ms = config["inrush_delay_ms"].get<uint32_t>();
        }
        
        return ESP_OK;
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration update error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
}

nlohmann::json RelayDriver::get_ui_schema() const {
    return {
        {"type", "object"},
        {"title", "Relay Settings"},
        {"properties", {
            {"min_off_time_s", {
                {"type", "integer"},
                {"title", "Minimum OFF Time (seconds)"},
                {"description", "Minimum time relay must stay OFF"},
                {"minimum", 0},
                {"maximum", 3600},
                {"default", 0}
            }},
            {"min_on_time_s", {
                {"type", "integer"},
                {"title", "Minimum ON Time (seconds)"},
                {"description", "Minimum time relay must stay ON"},
                {"minimum", 0},
                {"maximum", 3600},
                {"default", 0}
            }},
            {"inrush_delay_ms", {
                {"type", "integer"},
                {"title", "Inrush Delay (ms)"},
                {"description", "Delay after turning ON for inrush current"},
                {"minimum", 0},
                {"maximum", 5000},
                {"default", 0}
            }}
        }}
    };
}
nlohmann::json RelayDriver::get_diagnostics() const {
    uint32_t time_in_state_s = get_time_in_state_ms() / 1000;
    
    return {
        {"current_state", current_state_},
        {"commanded_state", commanded_state_},
        {"state_label", current_state_ ? config_.on_label : config_.off_label},
        {"time_in_state_s", time_in_state_s},
        {"state_changes", state_changes_},
        {"protection_blocks", protection_blocks_},
        {"protection_active", protection_active_},
        {"total_on_time_s", total_on_time_s_},
        {"hal_id", config_.hal_id}
    };
}

// Helper methods
bool RelayDriver::can_change_state(bool new_state) const {
    if (protection_active_) {
        return false;
    }
    
    uint32_t time_in_state_s = get_time_in_state_ms() / 1000;
    
    if (current_state_ && !new_state) {
        // Turning OFF - check minimum ON time
        return time_in_state_s >= config_.min_on_time_s;
    } else if (!current_state_ && new_state) {
        // Turning ON - check minimum OFF time
        return time_in_state_s >= config_.min_off_time_s;
    }
    
    return true;
}
void RelayDriver::apply_state(bool state) {
    if (!gpio_output_) {
        return;
    }
    
    // Apply state considering active_low configuration
    bool physical_state = config_.active_low ? !state : state;
    esp_err_t ret = gpio_output_->set_state(physical_state);
    
    if (ret == ESP_OK) {
        // Update state tracking
        if (state != current_state_) {
            current_state_ = state;
            last_change_time_ms_ = get_time_ms();
            state_changes_++;
            
            // Start protection timer
            if (state && config_.min_on_time_s > 0) {
                protection_active_ = true;
                protection_end_time_ms_ = last_change_time_ms_ + (config_.min_on_time_s * 1000);
            } else if (!state && config_.min_off_time_s > 0) {
                protection_active_ = true;
                protection_end_time_ms_ = last_change_time_ms_ + (config_.min_off_time_s * 1000);
            }
            
            // Track on time
            if (state) {
                last_on_time_ms_ = last_change_time_ms_;
            } else if (last_on_time_ms_ > 0) {
                total_on_time_s_ += (last_change_time_ms_ - last_on_time_ms_) / 1000;
                last_on_time_ms_ = 0;
            }
            
            ESP_LOGI(TAG, "Relay state changed to: %s", 
                     state ? config_.on_label.c_str() : config_.off_label.c_str());
        }
    } else {
        ESP_LOGE(TAG, "Failed to set GPIO state: %s", esp_err_to_name(ret));
    }
}

uint64_t RelayDriver::get_time_ms() const {
    return esp_timer_get_time() / 1000;
}

uint32_t RelayDriver::get_time_in_state_ms() const {
    return (get_time_ms() - last_change_time_ms_);
}