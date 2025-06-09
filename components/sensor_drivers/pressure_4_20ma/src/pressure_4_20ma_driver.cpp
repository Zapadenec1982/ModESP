/**
 * @file pressure_4_20ma_driver.cpp
 * @brief Implementation of 4-20mA pressure sensor driver
 */

#include "pressure_4_20ma_driver.h"
#include "sensor_driver_registry.h"
#include "esphal.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <numeric>
#include <cmath>

static const char* TAG = "PRESSURE_4_20MA";

// Auto-register this driver
static SensorDriverRegistrar<Pressure420mADriver> registrar("PRESSURE_4_20MA");

esp_err_t Pressure420mADriver::init(ESPhal& hal, const nlohmann::json& config) {
    ESP_LOGI(TAG, "Initializing 4-20mA pressure sensor driver");
    
    // Parse configuration
    try {
        config_.hal_id = config["hal_id"].get<std::string>();
        config_.pressure_min = config.value("pressure_min", 0.0f);
        config_.pressure_max = config.value("pressure_max", 10.0f);
        
        // Parse pressure unit
        std::string unit_str = config.value("unit", "bar");
        if (unit_str == "bar") config_.unit = PressureUnit::BAR;
        else if (unit_str == "psi") config_.unit = PressureUnit::PSI;
        else if (unit_str == "kpa") config_.unit = PressureUnit::KPA;
        else if (unit_str == "mpa") config_.unit = PressureUnit::MPA;        
        config_.r_sense = config.value("r_sense", 250.0f);
        config_.vcc = config.value("vcc", 3.3f);
        config_.averaging_samples = config.value("averaging_samples", 10);
        config_.alarm_low = config.value("alarm_low", -1.0f);
        config_.alarm_high = config.value("alarm_high", -1.0f);
        
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get ADC channel from HAL
    try {
        adc_channel_ = &hal.get_adc_channel(config_.hal_id);
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Failed to get ADC channel '%s': %s", 
                 config_.hal_id.c_str(), e.what());
        return ESP_FAIL;
    }
    
    // Initialize sample buffer
    sample_buffer_.resize(config_.averaging_samples, 0.0f);
    
    ESP_LOGI(TAG, "Pressure sensor initialized: %.1f-%.1f %s", 
             config_.pressure_min, config_.pressure_max, 
             unit_to_string(config_.unit).c_str());
    
    return ESP_OK;
}
SensorReading Pressure420mADriver::read() {
    SensorReading reading;
    reading.unit = unit_to_string(config_.unit);
    reading.timestamp_ms = esp_timer_get_time() / 1000;
    
    if (!adc_channel_) {
        reading.is_valid = false;
        reading.error_message = "ADC channel not initialized";
        return reading;
    }
    
    // Read ADC value
    auto adc_result = adc_channel_->read_raw();
    if (!adc_result.is_ok()) {
        reading.is_valid = false;
        reading.error_message = "ADC read failed";
        return reading;
    }
    
    // Convert to current
    float current_ma = adc_to_current_ma(adc_result.value);
    last_current_ma_ = current_ma;
    
    // Check for sensor fault
    if (current_ma < config_.fault_threshold_low || 
        current_ma > config_.fault_threshold_high) {
        fault_count_++;
        last_fault_state_ = true;
        reading.is_valid = false;
        reading.error_message = "Sensor fault (current out of range)";
        return reading;
    }
    
    last_fault_state_ = false;    
    // Convert to pressure
    float pressure = current_to_pressure(current_ma);
    
    // Apply calibration
    pressure = (pressure + config_.zero_offset) * config_.span_factor;
    
    // Update moving average
    update_moving_average(pressure);
    pressure = get_average_pressure();
    
    last_pressure_ = pressure;
    total_reads_++;
    
    // Check alarms
    if (config_.alarm_low > 0 && pressure < config_.alarm_low) {
        alarm_low_count_++;
    }
    if (config_.alarm_high > 0 && pressure > config_.alarm_high) {
        alarm_high_count_++;
    }
    
    reading.value = pressure;
    reading.is_valid = true;
    
    return reading;
}

// Helper methods
float Pressure420mADriver::adc_to_current_ma(int adc_value) const {
    // Convert ADC reading to voltage
    const float adc_max = 4095.0f; // 12-bit ADC
    float voltage = (adc_value / adc_max) * config_.vcc;
    
    // Convert voltage to current using Ohm's law (I = V/R)
    return (voltage / config_.r_sense) * 1000.0f; // Convert to mA
}
float Pressure420mADriver::current_to_pressure(float current_ma) const {
    // Linear conversion: 4mA = min pressure, 20mA = max pressure
    const float current_span = 16.0f; // 20mA - 4mA
    const float current_offset = current_ma - 4.0f;
    const float pressure_span = config_.pressure_max - config_.pressure_min;
    
    return config_.pressure_min + (current_offset / current_span) * pressure_span;
}

std::string Pressure420mADriver::unit_to_string(PressureUnit unit) const {
    switch (unit) {
        case PressureUnit::BAR: return "bar";
        case PressureUnit::PSI: return "psi";
        case PressureUnit::KPA: return "kPa";
        case PressureUnit::MPA: return "MPa";
        default: return "?";
    }
}

nlohmann::json Pressure420mADriver::get_config() const {
    return {
        {"hal_id", config_.hal_id},
        {"pressure_min", config_.pressure_min},
        {"pressure_max", config_.pressure_max},
        {"unit", unit_to_string(config_.unit)},
        {"r_sense", config_.r_sense},
        {"averaging_samples", config_.averaging_samples},
        {"alarm_low", config_.alarm_low},
        {"alarm_high", config_.alarm_high}
    };
}
nlohmann::json Pressure420mADriver::get_ui_schema() const {
    return {
        {"type", "object"},
        {"title", "4-20mA Pressure Sensor Settings"},
        {"properties", {
            {"pressure_range", {
                {"type", "object"},
                {"title", "Pressure Range"},
                {"properties", {
                    {"min", {
                        {"type", "number"},
                        {"title", "Minimum (at 4mA)"},
                        {"default", 0.0}
                    }},
                    {"max", {
                        {"type", "number"},
                        {"title", "Maximum (at 20mA)",
                        {"default", 10.0}
                    }}
                }}
            }},
            {"unit", {
                {"type", "string"},
                {"title", "Pressure Unit"},
                {"enum", {"bar", "psi", "kpa", "mpa"}},
                {"default", "bar"}
            }},
            {"alarms", {
                {"type", "object"},
                {"title", "Alarm Settings"},
                {"properties", {
                    {"low", {
                        {"type", "number"},
                        {"title", "Low Pressure Alarm",
                        {"description", "Set to -1 to disable"},
                        {"default", -1}
                    }},
                    {"high", {
                        {"type", "number"},
                        {"title", "High Pressure Alarm",
                        {"description", "Set to -1 to disable"},
                        {"default", -1}
                    }}
                }}
            }}
        }}
    };
}
esp_err_t Pressure420mADriver::calibrate(const nlohmann::json& calibration_data) {
    try {
        if (calibration_data.contains("zero")) {
            // Zero calibration: current pressure should read as specified value
            float target_pressure = calibration_data["zero"].get<float>();
            config_.zero_offset = target_pressure - last_pressure_;
            ESP_LOGI(TAG, "Zero calibrated: offset = %.3f", config_.zero_offset);
        }
        
        if (calibration_data.contains("span")) {
            // Span calibration: adjust scaling factor
            nlohmann::json span = calibration_data["span"];
            float actual_pressure = span["actual"].get<float>();
            float measured_pressure = last_pressure_ - config_.zero_offset;
            
            if (measured_pressure != 0) {
                config_.span_factor = actual_pressure / measured_pressure;
                ESP_LOGI(TAG, "Span calibrated: factor = %.3f", config_.span_factor);
            }
        }
        
        return ESP_OK;
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Calibration error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
}
nlohmann::json Pressure420mADriver::get_diagnostics() const {
    return {
        {"last_pressure", last_pressure_},
        {"last_current_ma", last_current_ma_},
        {"unit", unit_to_string(config_.unit)},
        {"total_reads", total_reads_},
        {"fault_count", fault_count_},
        {"fault_state", last_fault_state_},
        {"alarm_low_count", alarm_low_count_},
        {"alarm_high_count", alarm_high_count_},
        {"calibration", {
            {"zero_offset", config_.zero_offset},
            {"span_factor", config_.span_factor}
        }},
        {"sensor_range", {
            {"min", config_.pressure_min},
            {"max", config_.pressure_max},
            {"current_4_20", {4.0, 20.0}}
        }}
    };
}

esp_err_t Pressure420mADriver::set_config(const nlohmann::json& config) {
    // Allow runtime updates of some parameters
    try {
        if (config.contains("averaging_samples")) {
            int samples = config["averaging_samples"].get<int>();
            if (samples > 0 && samples <= 100) {
                config_.averaging_samples = samples;
                sample_buffer_.resize(samples, last_pressure_);
                buffer_filled_ = false;
                buffer_index_ = 0;
            }
        }
        
        if (config.contains("alarm_low")) {
            config_.alarm_low = config["alarm_low"].get<float>();
        }
        
        if (config.contains("alarm_high")) {
            config_.alarm_high = config["alarm_high"].get<float>();
        }
        
        return ESP_OK;
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration update error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
}
void Pressure420mADriver::update_moving_average(float value) {
    sample_buffer_[buffer_index_] = value;
    buffer_index_ = (buffer_index_ + 1) % config_.averaging_samples;
    
    if (buffer_index_ == 0) {
        buffer_filled_ = true;
    }
}

float Pressure420mADriver::get_average_pressure() const {
    size_t count = buffer_filled_ ? sample_buffer_.size() : buffer_index_;
    if (count == 0) return 0.0f;
    
    float sum = std::accumulate(sample_buffer_.begin(), 
                               sample_buffer_.begin() + count, 0.0f);
    return sum / count;
}