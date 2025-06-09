/**
 * @file gpio_input_driver.cpp
 * @brief Implementation of GPIO input sensor driver
 */

#include "gpio_input_driver.h"
#include "sensor_driver_registry.h"
#include "esphal.h"
#include <esp_log.h>
#include <esp_timer.h>

static const char* TAG = "GPIO_INPUT";

// Auto-register this driver
static SensorDriverRegistrar<GpioInputDriver> registrar("GPIO_INPUT");

GpioInputDriver::GpioInputDriver() {
    last_change_time_ms_ = get_time_ms();
}

GpioInputDriver::~GpioInputDriver() = default;

esp_err_t GpioInputDriver::init(ESPhal& hal, const nlohmann::json& config) {
    ESP_LOGI(TAG, "Initializing GPIO input driver");
    
    // Parse configuration
    try {
        config_.hal_id = config["hal_id"].get<std::string>();
        config_.invert = config.value("invert", false);
        config_.debounce_ms = config.value("debounce_ms", 50);
        config_.count_edges = config.value("count_edges", false);
        config_.active_label = config.value("active_label", "ON");
        config_.inactive_label = config.value("inactive_label", "OFF");
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }    
    // Get GPIO input from HAL
    try {
        gpio_input_ = &hal.get_gpio_input(config_.hal_id);
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Failed to get GPIO input '%s': %s", 
                 config_.hal_id.c_str(), e.what());
        return ESP_FAIL;
    }
    
    // Read initial state
    last_state_ = read_raw_state();
    debounced_state_ = last_state_;
    
    ESP_LOGI(TAG, "GPIO input initialized: %s, initial state: %s", 
             config_.hal_id.c_str(), debounced_state_ ? "HIGH" : "LOW");
    
    return ESP_OK;
}

SensorReading GpioInputDriver::read() {
    SensorReading reading;
    reading.unit = "";  // No unit for binary state
    reading.timestamp_ms = get_time_ms();
    
    if (!gpio_input_) {
        reading.is_valid = false;
        reading.error_message = "GPIO input not initialized";
        return reading;
    }
    
    total_reads_++;    
    // Update debounced state
    update_debounced_state();
    
    // Return binary value (0.0 or 1.0)
    reading.value = debounced_state_ ? 1.0f : 0.0f;
    reading.is_valid = true;
    
    return reading;
}

void GpioInputDriver::update_debounced_state() {
    bool current_state = read_raw_state();
    uint64_t now_ms = get_time_ms();
    
    if (current_state != last_state_) {
        // State changed, start debouncing
        if (!debouncing_) {
            debouncing_ = true;
            debounce_start_time_ms_ = now_ms;
        }
        last_state_ = current_state;
    } else if (debouncing_) {
        // Check if debounce time has passed
        if ((now_ms - debounce_start_time_ms_) >= config_.debounce_ms) {
            // Debounce complete, update state
            if (current_state != debounced_state_) {
                debounced_state_ = current_state;
                last_change_time_ms_ = now_ms;
                
                if (config_.count_edges) {
                    state_change_count_++;
                }
                
                ESP_LOGD(TAG, "State changed to: %s", 
                         debounced_state_ ? "HIGH" : "LOW");
            }
            debouncing_ = false;
        }
    }
}
nlohmann::json GpioInputDriver::get_config() const {
    return {
        {"hal_id", config_.hal_id},
        {"invert", config_.invert},
        {"debounce_ms", config_.debounce_ms},
        {"count_edges", config_.count_edges},
        {"active_label", config_.active_label},
        {"inactive_label", config_.inactive_label}
    };
}

esp_err_t GpioInputDriver::set_config(const nlohmann::json& config) {
    try {
        if (config.contains("invert")) {
            config_.invert = config["invert"].get<bool>();
        }
        
        if (config.contains("debounce_ms")) {
            config_.debounce_ms = config["debounce_ms"].get<uint32_t>();
        }
        
        if (config.contains("count_edges")) {
            config_.count_edges = config["count_edges"].get<bool>();
        }
        
        if (config.contains("active_label")) {
            config_.active_label = config["active_label"].get<std::string>();
        }
        
        if (config.contains("inactive_label")) {
            config_.inactive_label = config["inactive_label"].get<std::string>();
        }
        
        return ESP_OK;
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration update error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
}
nlohmann::json GpioInputDriver::get_ui_schema() const {
    return {
        {"type", "object"},
        {"title", "GPIO Input Settings"},
        {"properties", {
            {"invert", {
                {"type", "boolean"},
                {"title", "Invert Logic"},
                {"description", "Invert the input signal"},
                {"default", false}
            }},
            {"debounce_ms", {
                {"type", "integer"},
                {"title", "Debounce Time (ms)"},
                {"description", "Time to wait for stable signal"},
                {"minimum", 0},
                {"maximum", 1000},
                {"default", 50}
            }},
            {"count_edges", {
                {"type", "boolean"},
                {"title", "Count State Changes"},
                {"description", "Count transitions for diagnostics"},
                {"default", false}
            }},
            {"active_label", {
                {"type", "string"},
                {"title", "Active State Label"},
                {"description", "Label for active (HIGH) state"},
                {"default", "ON"}
            }},
            {"inactive_label", {
                {"type", "string"},
                {"title", "Inactive State Label"},
                {"description", "Label for inactive (LOW) state"},
                {"default", "OFF"}
            }}
        }}
    };
}
nlohmann::json GpioInputDriver::get_diagnostics() const {
    uint64_t now_ms = get_time_ms();
    uint64_t time_since_change_ms = now_ms - last_change_time_ms_;
    
    return {
        {"current_state", debounced_state_},
        {"state_label", debounced_state_ ? config_.active_label : config_.inactive_label},
        {"raw_state", gpio_input_ ? gpio_input_->get_state() : false},
        {"inverted", config_.invert},
        {"debouncing", debouncing_},
        {"state_change_count", state_change_count_},
        {"total_reads", total_reads_},
        {"time_since_change_ms", time_since_change_ms},
        {"time_since_change_s", time_since_change_ms / 1000.0}
    };
}

// Helper methods
bool GpioInputDriver::read_raw_state() {
    if (!gpio_input_) {
        return false;
    }
    
    bool state = gpio_input_->get_state();
    return config_.invert ? !state : state;
}

uint64_t GpioInputDriver::get_time_ms() const {
    return esp_timer_get_time() / 1000;
}