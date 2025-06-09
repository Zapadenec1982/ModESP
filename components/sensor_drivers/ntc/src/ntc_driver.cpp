/**
 * @file ntc_driver.cpp
 * @brief Implementation of NTC thermistor sensor driver
 */

#include "ntc_driver.h"
#include "sensor_driver_registry.h"
#include "esphal.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cmath>
#include <numeric>

static const char* TAG = "NTC";

// Auto-register this driver
static SensorDriverRegistrar<NTCDriver> registrar("NTC");

// Constants
static constexpr float KELVIN_OFFSET = 273.15f;

esp_err_t NTCDriver::init(ESPhal& hal, const nlohmann::json& config) {
    ESP_LOGI(TAG, "Initializing NTC driver");
    
    // Parse configuration
    try {
        config_.hal_id = config["hal_id"].get<std::string>();
        
        // Load NTC type if specified
        if (config.contains("ntc_type")) {
            std::string type_str = config["ntc_type"].get<std::string>();
            if (type_str == "10K_3950") config_.ntc_type = NTCType::NTC_10K_3950;
            else if (type_str == "10K_3435") config_.ntc_type = NTCType::NTC_10K_3435;
            else if (type_str == "100K_3950") config_.ntc_type = NTCType::NTC_100K_3950;
            else config_.ntc_type = NTCType::CUSTOM;
        }        
        // Load profile or custom parameters
        if (config_.ntc_type != NTCType::CUSTOM) {
            load_ntc_profile(config_.ntc_type);
        }
        
        // Override with any custom parameters
        config_.r_nominal = config.value("r_nominal", config_.r_nominal);
        config_.t_nominal = config.value("t_nominal", config_.t_nominal);
        config_.beta = config.value("beta", config_.beta);
        config_.r_series = config.value("r_series", config_.r_series);
        config_.vcc = config.value("vcc", config_.vcc);
        config_.averaging_samples = config.value("averaging_samples", config_.averaging_samples);
        config_.offset = config.value("offset", config_.offset);
        
        // Steinhart-Hart coefficients if provided
        if (config.contains("steinhart_hart")) {
            auto sh = config["steinhart_hart"];
            config_.sh_coefficients[0] = sh.value("a", 0.0f);
            config_.sh_coefficients[1] = sh.value("b", 0.0f);
            config_.sh_coefficients[2] = sh.value("c", 0.0f);
            config_.use_steinhart_hart = true;
        }
        
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
    sample_buffer_.resize(config_.averaging_samples);
    
    ESP_LOGI(TAG, "NTC initialized: R_nominal=%.0f, Beta=%.0f, R_series=%.0f", 
             config_.r_nominal, config_.beta, config_.r_series);
    
    return ESP_OK;
}

SensorReading NTCDriver::read() {
    SensorReading reading;
    reading.unit = "°C";
    reading.timestamp_ms = esp_timer_get_time() / 1000;
    
    if (!adc_channel_) {
        reading.is_valid = false;
        reading.error_message = "ADC channel not initialized";
        return reading;
    }    
    // Read multiple samples for averaging
    float sum_resistance = 0;
    int valid_samples = 0;
    
    for (int i = 0; i < config_.averaging_samples; i++) {
        auto adc_result = adc_channel_->read_raw();
        if (!adc_result.is_ok()) {
            error_count_++;
            continue;
        }
        
        float resistance = calculate_resistance(adc_result.value);
        if (resistance > 0) {
            sum_resistance += resistance;
            valid_samples++;
        }
        
        // Small delay between samples
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    
    if (valid_samples == 0) {
        reading.is_valid = false;
        reading.error_message = "No valid ADC readings";
        return reading;
    }
    
    // Calculate average resistance
    float avg_resistance = sum_resistance / valid_samples;
    last_resistance_ = avg_resistance;    
    // Convert resistance to temperature
    float temperature = resistance_to_temperature(avg_resistance);
    
    // Apply calibration offset
    temperature += config_.offset;
    
    // Validate temperature range
    if (temperature < -40.0f || temperature > 150.0f) {
        error_count_++;
        reading.is_valid = false;
        reading.error_message = "Temperature out of range";
        return reading;
    }
    
    // Update state
    last_temperature_ = temperature;
    total_reads_++;
    
    reading.value = temperature;
    reading.is_valid = true;
    
    return reading;
}

nlohmann::json NTCDriver::get_config() const {
    nlohmann::json config = {
        {"hal_id", config_.hal_id},
        {"r_nominal", config_.r_nominal},
        {"t_nominal", config_.t_nominal},
        {"beta", config_.beta},
        {"r_series", config_.r_series},
        {"vcc", config_.vcc},
        {"averaging_samples", config_.averaging_samples},
        {"offset", config_.offset}
    };    
    // Add NTC type string
    std::string type_str = "CUSTOM";
    switch (config_.ntc_type) {
        case NTCType::NTC_10K_3950: type_str = "10K_3950"; break;
        case NTCType::NTC_10K_3435: type_str = "10K_3435"; break;
        case NTCType::NTC_100K_3950: type_str = "100K_3950"; break;
        default: break;
    }
    config["ntc_type"] = type_str;
    
    if (config_.use_steinhart_hart) {
        config["steinhart_hart"] = {
            {"a", config_.sh_coefficients[0]},
            {"b", config_.sh_coefficients[1]},
            {"c", config_.sh_coefficients[2]}
        };
    }
    
    return config;
}

esp_err_t NTCDriver::set_config(const nlohmann::json& config) {
    try {
        if (config.contains("r_series")) {
            config_.r_series = config["r_series"].get<float>();
        }
        
        if (config.contains("averaging_samples")) {
            int samples = config["averaging_samples"].get<int>();
            if (samples > 0 && samples <= 100) {
                config_.averaging_samples = samples;
                sample_buffer_.resize(samples);
            }
        }        
        if (config.contains("offset")) {
            config_.offset = config["offset"].get<float>();
        }
        
        return ESP_OK;
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration update error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
}

nlohmann::json NTCDriver::get_ui_schema() const {
    return {
        {"type", "object"},
        {"title", "NTC Thermistor Settings"},
        {"properties", {
            {"ntc_type", {
                {"type", "string"},
                {"title", "NTC Type"},
                {"enum", {"10K_3950", "10K_3435", "100K_3950", "CUSTOM"}},
                {"default", "10K_3950"},
                {"ui:widget", "select"}
            }},
            {"r_series", {
                {"type", "number"},
                {"title", "Series Resistor (Ω)"},
                {"description", "Value of the voltage divider resistor"},
                {"minimum", 100},
                {"maximum", 1000000},
                {"default", 10000}
            }},
            {"averaging_samples", {
                {"type", "integer"},
                {"title", "Averaging Samples"},
                {"description", "Number of ADC samples to average"},
                {"minimum", 1},
                {"maximum", 100},
                {"default", 10}
            }},
            {"offset", {
                {"type", "number"},
                {"title", "Temperature Offset (°C)"},
                {"description", "Calibration offset"},
                {"minimum", -10.0},
                {"maximum", 10.0},
                {"default", 0.0},
                {"ui:widget", "slider"},
                {"ui:step", 0.1}
            }}
        }}
    };
}
esp_err_t NTCDriver::calibrate(const nlohmann::json& calibration_data) {
    try {
        if (calibration_data.contains("known_temperature")) {
            float known_temp = calibration_data["known_temperature"].get<float>();
            
            // Read current temperature
            SensorReading reading = read();
            if (!reading.is_valid) {
                return ESP_FAIL;
            }
            
            // Calculate offset
            config_.offset = known_temp - (reading.value - config_.offset);
            
            ESP_LOGI(TAG, "Calibrated with offset: %.2f°C", config_.offset);
            return ESP_OK;
        }
        
        if (calibration_data.contains("resistance_points")) {
            // Advanced calibration with multiple points for Steinhart-Hart
            // TODO: Implement multi-point calibration
            return ESP_ERR_NOT_SUPPORTED;
        }
        
        return ESP_ERR_INVALID_ARG;
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Calibration error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
}
nlohmann::json NTCDriver::get_diagnostics() const {
    return {
        {"last_temperature", last_temperature_},
        {"last_resistance", last_resistance_},
        {"total_reads", total_reads_},
        {"error_count", error_count_},
        {"error_rate", total_reads_ > 0 ? 
            (float)error_count_ / total_reads_ : 0.0f},
        {"adc_channel", config_.hal_id},
        {"ntc_parameters", {
            {"r_nominal", config_.r_nominal},
            {"beta", config_.beta},
            {"r_series", config_.r_series}
        }}
    };
}

// Helper methods
float NTCDriver::calculate_resistance(int adc_value) const {
    // ADC typically 12-bit (0-4095)
    const float adc_max = 4095.0f;
    float adc_voltage = (adc_value / adc_max) * config_.vcc;
    
    // Calculate NTC resistance using voltage divider formula
    // Vout = Vcc * R_ntc / (R_series + R_ntc)
    // R_ntc = R_series * Vout / (Vcc - Vout)
    
    if (adc_voltage >= config_.vcc) {
        return 0;  // Invalid reading
    }
    
    return config_.r_series * adc_voltage / (config_.vcc - adc_voltage);
}
float NTCDriver::resistance_to_temperature(float resistance) const {
    if (config_.use_steinhart_hart) {
        return steinhart_hart(resistance);
    }
    
    // Use Beta equation
    // 1/T = 1/T0 + (1/Beta) * ln(R/R0)
    float t_nominal_k = config_.t_nominal + KELVIN_OFFSET;
    float ln_r = std::log(resistance / config_.r_nominal);
    float t_kelvin = 1.0f / (1.0f / t_nominal_k + ln_r / config_.beta);
    
    return t_kelvin - KELVIN_OFFSET;
}

float NTCDriver::steinhart_hart(float resistance) const {
    // Steinhart-Hart equation: 1/T = A + B*ln(R) + C*ln(R)^3
    float ln_r = std::log(resistance);
    float ln_r3 = ln_r * ln_r * ln_r;
    
    float t_kelvin = 1.0f / (config_.sh_coefficients[0] + 
                             config_.sh_coefficients[1] * ln_r + 
                             config_.sh_coefficients[2] * ln_r3);
    
    return t_kelvin - KELVIN_OFFSET;
}

void NTCDriver::load_ntc_profile(NTCType type) {
    switch (type) {
        case NTCType::NTC_10K_3950:
            config_.r_nominal = 10000.0f;
            config_.t_nominal = 25.0f;
            config_.beta = 3950.0f;
            break;            
        case NTCType::NTC_10K_3435:
            config_.r_nominal = 10000.0f;
            config_.t_nominal = 25.0f;
            config_.beta = 3435.0f;
            break;
            
        case NTCType::NTC_100K_3950:
            config_.r_nominal = 100000.0f;
            config_.t_nominal = 25.0f;
            config_.beta = 3950.0f;
            break;
            
        default:
            // Keep current values for CUSTOM
            break;
    }
}