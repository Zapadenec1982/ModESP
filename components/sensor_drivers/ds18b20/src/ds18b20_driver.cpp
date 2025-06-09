/**
 * @file ds18b20_driver.cpp
 * @brief Implementation of DS18B20 temperature sensor driver
 */

#include "ds18b20_driver.h"
#include "sensor_driver_registry.h"
#include "esphal.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <sstream>
#include <iomanip>

static const char* TAG = "DS18B20";

// Auto-register this driver
static SensorDriverRegistrar<DS18B20Driver> registrar("DS18B20");

esp_err_t DS18B20Driver::init(ESPhal& hal, const nlohmann::json& config) {
    ESP_LOGI(TAG, "Initializing DS18B20 driver");
    
    // Parse configuration
    try {
        config_.hal_id = config["hal_id"].get<std::string>();
        config_.address = config["address"].get<std::string>();
        config_.resolution = config.value("resolution", 12);
        config_.offset = config.value("offset", 0.0f);
        config_.read_timeout_ms = config.value("read_timeout_ms", 1000);
        config_.use_crc = config.value("use_crc", true);
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
    // Validate configuration
    if (config_.resolution < 9 || config_.resolution > 12) {
        ESP_LOGE(TAG, "Invalid resolution: %d", config_.resolution);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get OneWire bus from HAL
    try {
        bus_ = &hal.get_onewire_bus(config_.hal_id);
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Failed to get OneWire bus '%s': %s", 
                 config_.hal_id.c_str(), e.what());
        return ESP_FAIL;
    }
    
    // Parse sensor address
    sensor_address_ = parse_address(config_.address);
    if (sensor_address_ == 0) {
        ESP_LOGE(TAG, "Invalid sensor address: %s", config_.address.c_str());
        return ESP_ERR_INVALID_ARG;
    }
    
    // Verify sensor presence
    auto devices = bus_->search_devices();
    sensor_available_ = false;
    
    for (auto addr : devices) {
        if (addr == sensor_address_) {
            sensor_available_ = true;
            ESP_LOGI(TAG, "Found sensor at address: 0x%016llx", sensor_address_);
            break;
        }
    }
    if (!sensor_available_) {
        ESP_LOGE(TAG, "Sensor not found on bus");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "DS18B20 initialized successfully");
    return ESP_OK;
}

SensorReading DS18B20Driver::read() {
    SensorReading reading;
    reading.unit = "°C";
    reading.timestamp_ms = esp_timer_get_time() / 1000;
    
    if (!sensor_available_) {
        reading.is_valid = false;
        reading.error_message = "Sensor not available";
        return reading;
    }
    
    // Request temperature conversion
    esp_err_t ret = bus_->request_temperatures();
    if (ret != ESP_OK) {
        error_count_++;
        reading.is_valid = false;
        reading.error_message = "Failed to request temperature";
        return reading;
    }
    
    // Wait for conversion
    vTaskDelay(pdMS_TO_TICKS(get_conversion_time_ms()));
    // Read temperature
    auto result = bus_->read_temperature(sensor_address_);
    if (!result.is_ok()) {
        error_count_++;
        reading.is_valid = false;
        reading.error_message = "Failed to read temperature";
        return reading;
    }
    
    float temperature = result.value;
    
    // Validate temperature range
    if (!validate_temperature(temperature)) {
        error_count_++;
        reading.is_valid = false;
        reading.error_message = "Temperature out of range";
        return reading;
    }
    
    // Apply calibration offset
    temperature += config_.offset;
    
    // Update state
    last_temperature_ = temperature;
    successful_reads_++;
    
    reading.value = temperature;
    reading.is_valid = true;
    
    return reading;
}
nlohmann::json DS18B20Driver::get_config() const {
    return {
        {"hal_id", config_.hal_id},
        {"address", config_.address},
        {"resolution", config_.resolution},
        {"offset", config_.offset},
        {"read_timeout_ms", config_.read_timeout_ms},
        {"use_crc", config_.use_crc}
    };
}

esp_err_t DS18B20Driver::set_config(const nlohmann::json& config) {
    try {
        if (config.contains("resolution")) {
            int res = config["resolution"].get<int>();
            if (res >= 9 && res <= 12) {
                config_.resolution = res;
            }
        }
        
        if (config.contains("offset")) {
            config_.offset = config["offset"].get<float>();
        }
        
        if (config.contains("use_crc")) {
            config_.use_crc = config["use_crc"].get<bool>();
        }
        
        return ESP_OK;
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Configuration update error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
}
nlohmann::json DS18B20Driver::get_ui_schema() const {
    return {
        {"type", "object"},
        {"title", "DS18B20 Temperature Sensor Settings"},
        {"properties", {
            {"resolution", {
                {"type", "integer"},
                {"title", "Resolution"},
                {"description", "Measurement resolution in bits"},
                {"minimum", 9},
                {"maximum", 12},
                {"default", 12},
                {"ui:widget", "slider"},
                {"ui:help", "Higher resolution = slower conversion"}
            }},
            {"offset", {
                {"type", "number"},
                {"title", "Temperature Offset"},
                {"description", "Calibration offset in °C"},
                {"minimum", -10.0},
                {"maximum", 10.0},
                {"default", 0.0},
                {"ui:widget", "slider"},
                {"ui:step", 0.1}
            }},
            {"use_crc", {
                {"type", "boolean"},
                {"title", "Enable CRC Check"},
                {"description", "Validate data integrity"},
                {"default", true}
            }}
        }}
    };
}
nlohmann::json DS18B20Driver::get_ui_schema() const {
    return {
        {"type", "object"},
        {"title", "DS18B20 Temperature Sensor Settings"},
        {"properties", {
            {"resolution", {
                {"type", "integer"},
                {"title", "Resolution"},
                {"description", "Measurement resolution in bits"},
                {"minimum", 9},
                {"maximum", 12},
                {"default", 12},
                {"ui:widget", "slider"},
                {"ui:help", "Higher resolution = slower conversion"}
            }},
            {"offset", {
                {"type", "number"},
                {"title", "Temperature Offset"},
                {"description", "Calibration offset in °C"},
                {"minimum", -10.0},
                {"maximum", 10.0},
                {"default", 0.0},
                {"ui:widget", "slider"},
                {"ui:step", 0.1}
            }},
            {"use_crc", {
                {"type", "boolean"},
                {"title", "Enable CRC Check"},
                {"description", "Validate data integrity"},
                {"default", true}
            }}
        }}
    };
}
esp_err_t DS18B20Driver::calibrate(const nlohmann::json& calibration_data) {
    try {
        if (calibration_data.contains("known_temperature")) {
            float known_temp = calibration_data["known_temperature"].get<float>();
            
            // Read current temperature
            SensorReading reading = read();
            if (!reading.is_valid) {
                return ESP_FAIL;
            }
            
            // Calculate offset
            float raw_temp = reading.value - config_.offset;  // Remove current offset
            config_.offset = known_temp - raw_temp;
            
            ESP_LOGI(TAG, "Calibrated with offset: %.2f°C", config_.offset);
            return ESP_OK;
        }
        
        return ESP_ERR_INVALID_ARG;
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "Calibration error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    }
}

nlohmann::json DS18B20Driver::get_diagnostics() const {
    return {
        {"sensor_available", sensor_available_},
        {"last_temperature", last_temperature_},
        {"successful_reads", successful_reads_},
        {"error_count", error_count_},
        {"error_rate", successful_reads_ > 0 ? 
            (float)error_count_ / (successful_reads_ + error_count_) : 0.0f},
        {"sensor_address", config_.address},
        {"resolution_bits", config_.resolution}
    };
}
// Helper methods implementation
uint64_t DS18B20Driver::parse_address(const std::string& hex_address) {
    // Remove any spaces or dashes
    std::string clean_address;
    for (char c : hex_address) {
        if (std::isxdigit(c)) {
            clean_address += c;
        }
    }
    
    if (clean_address.length() != 16) {
        ESP_LOGE(TAG, "Invalid address length: %zu", clean_address.length());
        return 0;
    }
    
    try {
        return std::stoull(clean_address, nullptr, 16);
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Failed to parse address: %s", e.what());
        return 0;
    }
}

int DS18B20Driver::get_conversion_time_ms() const {
    // Conversion times for different resolutions
    switch (config_.resolution) {
        case 9:  return 94;
        case 10: return 188;
        case 11: return 375;
        case 12: return 750;
        default: return 750;
    }
}

bool DS18B20Driver::validate_temperature(float temp) const {
    // DS18B20 valid range: -55°C to +125°C
    return temp >= -55.0f && temp <= 125.0f;
}